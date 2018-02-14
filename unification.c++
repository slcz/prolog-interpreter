#include <iostream>
#include <string>
#include <cstdint>
#include "parser.h"
#include "unification.h"

using namespace std;

void remove_from_table(var_lookup &table, const vector<uint64_t> &list)
{
	if (list.empty())
		return;
	for (auto item : list)
		assert(table.erase(item) == 1);
}

const bind_value &walk(const bind_value &value, const var_lookup &table)
{
	if (!holds_alternative<p_structure>(value))
		return value;
	const p_structure &t = get<p_structure>(value);
	if (t->get_type() == symbol::variable) {
		auto n = table.find(t->get_id());
		if (n != table.end())
			return walk(n->second, table);
	}
	return value;
}

optional<vector<uint64_t>> unification(const p_term &, const p_term &,
		uint64_t, uint64_t, var_lookup &);
optional<vector<uint64_t>>
unify_rest(p_structure &src, p_structure &dst, var_lookup &table)
{
	uint64_t srcoff = src->get_base(), dstoff = dst->get_base();
	auto &srcvec = src->get_root()->get_rest(),
	     &dstvec = dst->get_root()->get_rest();
	auto ss = srcvec.begin(), se = srcvec.end();
	auto ds = dstvec.begin(), de = dstvec.end();
	vector<uint64_t> all;
	optional<vector<uint64_t>> r;

	for (;ss != se && ds != de; ss ++, ds ++) {
		if ((r = unification(*ss, *ds, srcoff, dstoff, table))) {
			if (!r->empty())
				all.insert(all.end(), r->begin(), r->end());
		} else
			goto failure;
	}
	if (ss == se && ds == de)
		return move(all);
failure:
	remove_from_table(table, all);
	return nullopt;
}

inline bool
wildcard(const unique_ptr<token> &a)
{
	return a->get_type() == symbol::variable && a->get_text() == "_";
}

// returns false if variable table loop is detected.
bool loop(uint64_t id, const bind_value &value, const var_lookup &table)
{
	if (!holds_alternative<p_structure>(value))
		return true;
	const p_structure &t = get<p_structure>(value);
	const p_term &root = t->get_root();
	uint64_t offset = t->get_base();

	if (t->get_type() == symbol::variable) {
		if (t->get_id() == id)
			return false;
		const bind_value &tmp = walk(value, table);
		if (value == tmp)
			return true;
		return loop(id, tmp, table);
	} else {
		assert(t->get_type() == symbol::atom);
		for (const auto &i : root->get_rest()) {
			bind_value ptmp {make_shared<structure>(i, offset)};
			if (!loop(id, ptmp, table))
				return false;
		}
		return true;
	}
	return true;
}

optional<uint64_t> bind(p_structure &from, bind_value to, var_lookup &table)
{
	if (wildcard(from->get_root()->get_first()))
		return 0;
	uint64_t id = from->get_id();
	assert(id != 0);
	if (!holds_alternative<p_structure>(to)) {
		table.insert(make_pair(id, move(to)));
		return id;
	}
	const p_structure &t = get<p_structure>(to);
	if (wildcard(t->get_root()->get_first()))
		return 0;
	if (!loop(id, to, table))
		return nullopt;
	table.insert(make_pair(id, move(to)));
	return id;
}

optional<vector<uint64_t>>
unify_terms(p_structure &src, p_structure &dst, var_lookup &table)
{
	optional<vector<uint64_t>> r;
	vector<uint64_t> all;
	symbol stype = src->get_type(), dtype = dst->get_type();

	assert(stype == symbol::atom && dtype == symbol::atom);
	if (src->get_id() == dst->get_id() &&
	    (r = unify_rest(src, dst, table))) {
		if (!r->empty())
			all.insert(all.end(), r->begin(), r->end());
		return all;
	} else {
		remove_from_table(table, all);
		return nullopt;
	}
}

optional<vector<uint64_t>>
unify_variable(p_structure &src, bind_value dst, var_lookup &table)
{
	vector<uint64_t> all;
	optional<uint64_t> key = bind(src, move(dst), table);
	if (key) {
		/* wildcard matching */
		if (*key != 0)
			all.push_back(*key);
	}
	return all;
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

optional<vector<uint64_t>> unification_sub(bind_value src, bind_value dst,
        var_lookup &table)
{
	if (holds_alternative<p_structure>(src)) {
		p_structure p = get<p_structure>(src);
		assert(p);
		if (p->get_root()->get_first()->get_type() == symbol::variable)
			return unify_variable(p, dst, table);
	}
	if (src.index() != dst.index())
		return nullopt;
	return visit(overloaded {
		[&table, &dst](p_structure s) {
			p_structure &d = get<p_structure>(dst);
			return unify_terms(s, d, table);
		},
		[&table, &dst](int s) -> optional<vector<uint64_t>> {
			vector<uint64_t> v;
			int d = get<int>(dst);
			if (d == s)
				return move(v);
			else
				return nullopt;
		}
	}, src);
}

bind_value
build_target(const p_term &root, uint64_t offset, var_lookup &table)
{
	const bind_value newroot { make_shared<structure>(root, offset) };
	const bind_value tmp = walk(newroot, table);
	if (holds_alternative<int>(tmp))
		return (tmp);
	p_structure p = get<p_structure>(tmp);
	const unique_ptr<token> &t = p->get_root()->get_first();
	if (t->get_type() == symbol::number)
		return bind_value {t->get_value()};
	return tmp;
}

using builtin_fn = optional<vector<uint64_t>> (*)(const vector<p_term> &,
		uint64_t, var_lookup &);

optional<int> add(vector<int> args)
{
	if (args.size() == 2) return args[0] + args[1];
	return nullopt;
}

optional<int> sub(vector<int> args)
{
	if (args.size() == 2) return args[0] - args[1];
	if (args.size() == 1) return -args[0];
	return nullopt;
}

optional<int> mul(vector<int> args)
{
	if (args.size() == 2) return args[0] * args[1];
	return nullopt;
}

optional<int> div(vector<int> args)
{
	if (args.size() == 2 || args[1] != 0) return args[0] / args[1];
	return nullopt;
}

unordered_map<string, optional<int>(*)(vector<int>)> arith_ops = {
	{ "+", add},
	{ "-", sub},
	{ "*", mul},
	{ "/", div},
};

optional<int> eval_arith(bind_value, var_lookup &);
optional<int> eval_arith_sub(p_structure s, var_lookup &table)
{
	const auto &head = s->get_root()->get_first();
	const auto &body = s->get_root()->get_rest();
	if (head->get_type() == symbol::number)
		return head->get_value();
	string op = head->get_text();
	uint64_t base = s->get_id();
	vector<int> args;
	for (auto &i : body) {
		auto a = eval_arith(build_target(i, base, table), table);
		if (!a)
			return nullopt;
		args.push_back(move(*a));
	}
	auto m = arith_ops.find(op);
	if (m == arith_ops.end())
		return nullopt;
	return m->second(args);
}

optional<int> eval_arith(bind_value root, var_lookup &table)
{
	return visit(overloaded {
	[&table](p_structure s) -> optional<int> {
		return eval_arith_sub(s, table);
	},
	[] (int s) -> optional<int> {
		return s;
	} }, root);
}

optional<vector<uint64_t>> builtin_is(const vector<p_term> &args,
		uint64_t base, var_lookup &table)
{
	bind_value l = build_target(args[0], base, table);
	bind_value r = build_target(args[1], base, table);
	optional<int> result = eval_arith(r, table);
	if (!result)
		return nullopt;
	return unification_sub(l, bind_value{*result}, table);
}

unordered_map<string, pair<builtin_fn, uint32_t>> builtin_map = {
	{ "is", {builtin_is, 2}}
};

optional<vector<uint64_t>>
builtin(const p_term &src, uint64_t srcoff, var_lookup &table)
{
	bind_value srctgt = build_target(src, srcoff, table);
	if (!holds_alternative<p_structure>(srctgt))
		return nullopt;
	p_structure p = get<p_structure>(srctgt);
	const unique_ptr<token> &head = p->get_root()->get_first();
	const vector<p_term> &rest = p->get_root()->get_rest();
	if (head->get_type() != symbol::atom)
		return nullopt;
	auto m = builtin_map.find(head->get_text());
	if (m == builtin_map.end())
		return nullopt;
	builtin_fn f = m->second.first;
	if (m->second.second != rest.size())
		return nullopt;
	if (!f)
		return nullopt;
	return f(rest, srcoff, table);
}

optional<vector<uint64_t>>
unification(const p_term &src, const p_term &dst, uint64_t srcoff,
		uint64_t dstoff, var_lookup &table)
{
	bind_value srctgt, dsttgt;
	optional<vector<uint64_t>> r;
	bool done = false;
	srctgt = build_target(src, srcoff, table);
	dsttgt = build_target(dst, dstoff, table);
	if (holds_alternative<p_structure>(srctgt)) {
		p_structure p = get<p_structure>(srctgt);
		if (p->get_root()->get_first()->get_type() == symbol::variable){
			r = unification_sub(move(srctgt),move(dsttgt),table);
			done = true;
		}
	}
	if (!done)
		r = unification_sub(move(dsttgt), move(srctgt), table);

	return r;
}

// test
void
print_term(const bind_value &value, const unordered_map<uint64_t, string> v,
		const var_lookup &table)
{
	const bind_value &n = walk(value, table);

	if (holds_alternative<int>(n)) {
		cout << get<int>(n);
		return;
	}

	p_structure t = get<p_structure>(n);

	const unique_ptr<token> &first = t->get_root()->get_first();
	const vector<p_term> &rest = t->get_root()->get_rest();
	uint64_t offset = t->get_base();
	cout << first->get_text();
	if (t->get_type() == symbol::atom && !rest.empty()) {
		cout << "(";
		for (auto &i : rest) {
			p_structure a = make_shared<structure>(i, offset);
			print_term(a, v, table);
		}
		cout << ")";
	}
}

void
print_all(const unordered_map<uint64_t, string> &v, const var_lookup &table)
{
	for (auto &i :v) {
		auto n = table.find(i.first);
		if (n != table.end()) {
			auto &s = walk(n->second, table);
			cout << i.second << "=>";
			print_term(s, v, table);
			cout << endl;
		}
	}
}
