#include <iostream>
#include <string>
#include <cstdint>
#include <functional>
#include "parser.h"
#include "unification.h"
#include "unification-priv.h"

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
		},
		[&table, &dst](float s) -> optional<vector<uint64_t>> {
			vector<uint64_t> v;
			int d = get<float>(dst);
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
	if (holds_alternative<float>(tmp))
		return (tmp);
	p_structure p = get<p_structure>(tmp);
	const unique_ptr<token> &t = p->get_root()->get_first();
	if (t->get_type() == symbol::integer)
		return bind_value {t->get_int_value()};
	if (t->get_type() == symbol::decimal)
		return bind_value {t->get_decimal_value()};
	return tmp;
	return tmp;
}

bool
can_unwrap(const p_term &t, uint64_t base, var_lookup &table)
{
	auto r = build_target(t, base, table);

	if (!holds_alternative<p_structure>(r))
		return false;
	auto m = get<p_structure>(r);
	if (m->get_root()->get_first()->get_type() != symbol::atom)
		return false;
	auto & n = m->get_root()->get_rest();
	return n.size() == 1;
}

const p_term &
unwrap(const p_term &t, uint64_t base, var_lookup &table)
{
	auto r = build_target(t, base, table);

	auto m = get<p_structure>(r);
	auto & n = m->get_root()->get_rest();
	return n[0];
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
	if (holds_alternative<float>(n)) {
		cout << get<float>(n);
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
