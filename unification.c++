#include <iostream>
#include <string>
#include <cstdint>
#include "parser.h"
#include "unification.h"

using namespace std;

/*
 * unification.
 * unify 2 terms together, source variable ids are added by offset_source.
 * destination variable ids are incrementaed add offset_destination.
 */
void undo_bindings(binding_t &binding, const vector<uint64_t> &undo_list)
{
	if (undo_list.empty())
		return;
	for (auto undo : undo_list)
		assert(binding.erase(undo) == 1);
}

const bind_t &walk_variable(const bind_t &value, const binding_t &binding)
{
	if (!holds_alternative<p_bind_value>(value))
		return value;
	const p_bind_value &t = get<p_bind_value>(value);
	if (t->get_type() == symbol::variable) {
		auto n = binding.find(t->get_id());
		if (n != binding.end())
			return walk_variable(n->second, binding);
	}
	return value;
}

optional<vector<uint64_t>> unification(const p_term &, const p_term &,
		uint64_t, uint64_t, binding_t &);
optional<vector<uint64_t>>
unify_rest(p_bind_value &src, p_bind_value &dst, binding_t &binding)
{
	uint64_t srcoff = src->get_base(), dstoff = dst->get_base();
	auto &srcvec = src->get_root()->get_rest(),
	     &dstvec = dst->get_root()->get_rest();
	auto ss = srcvec.begin(), se = srcvec.end();
	auto ds = dstvec.begin(), de = dstvec.end();
	vector<uint64_t> all;
	optional<vector<uint64_t>> r;

	for (;ss != se && ds != de; ss ++, ds ++) {
		if ((r = unification(*ss, *ds, srcoff, dstoff, binding))) {
			if (!r->empty())
				all.insert(all.end(), r->begin(), r->end());
		} else
			goto failure;
	}
	if (ss == se && ds == de)
		return move(all);
failure:
	undo_bindings(binding, all);
	return nullopt;
}

inline bool
is_wildcard(const unique_ptr<token> &a)
{
	return a->get_type() == symbol::variable && a->get_text() == "_";
}

// returns false if variable binding loop is detected.
bool detect_loop(uint64_t id, const bind_t &value, const binding_t &binding)
{
	if (!holds_alternative<p_bind_value>(value))
		return true;
	const p_bind_value &t = get<p_bind_value>(value);
	const p_term &root = t->get_root();
	uint64_t offset = t->get_base();

	if (t->get_type() == symbol::variable) {
		if (t->get_id() == id)
			return false;
		const bind_t &tmp = walk_variable(value, binding);
		if (value == tmp)
			return true;
		return detect_loop(id, tmp, binding);
	} else {
		assert(t->get_type() == symbol::atom);
		for (const auto &i : root->get_rest()) {
			bind_t ptmp {make_shared<bind_value>(i, offset)};
			if (!detect_loop(id, ptmp, binding))
				return false;
		}
		return true;
	}
	return true;
}

optional<uint64_t> bind(p_bind_value &from, bind_t to, binding_t &binding)
{
	if (is_wildcard(from->get_root()->get_first()))
		return 0;
	uint64_t id = from->get_id();
	assert(id != 0);
	if (!holds_alternative<p_bind_value>(to)) {
		binding.insert(make_pair(id, move(to)));
		return id;
	}
	const p_bind_value &t = get<p_bind_value>(to);
	if (is_wildcard(t->get_root()->get_first()))
		return 0;
	if (!detect_loop(id, to, binding))
		return nullopt;
	binding.insert(make_pair(id, move(to)));
	return id;
}

optional<vector<uint64_t>>
unify_terms(p_bind_value &src, p_bind_value &dst, binding_t &binding)
{
	optional<vector<uint64_t>> r;
	vector<uint64_t> all;
	symbol stype = src->get_type(), dtype = dst->get_type();

	if (stype == symbol::atom && dtype == symbol::atom) {
		if (src->get_id() == dst->get_id() &&
		    (r = unify_rest(src, dst, binding))) {
			if (!r->empty())
				all.insert(all.end(), r->begin(), r->end());
			return all;
		} else
			goto failure;
	} else {
		p_bind_value &p1  = dtype == symbol::variable ? dst : src;
		bind_t p2 = dtype == symbol::variable ? src : dst;
		optional<uint64_t> key = bind(p1, move(p2), binding);
		if (key) {
			/* wildcard matching */
			if (*key != 0)
				all.push_back(*key);
		} else
			goto failure;
	}
	return all;
failure:
	undo_bindings(binding, all);
	return nullopt;
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

optional<vector<uint64_t>> unification_sub(bind_t src, bind_t dst,
        binding_t &binding)
{
	if (src.index() != dst.index())
		return nullopt;
	return visit(overloaded {
		[&binding, &dst](p_bind_value s) {
			p_bind_value &d = get<p_bind_value>(dst);
			return unify_terms(s, d, binding);
		},
		[&binding, &dst](int s) -> optional<vector<uint64_t>> {
			vector<uint64_t> v;
			int d = get<int>(dst);
			if (d == s)
				return move(v);
			else
				return nullopt;
		}
	}, src);
}

bind_t
build_target(const p_term &root, uint64_t offset, binding_t &binding)
{
	const bind_t newroot { make_shared<bind_value>(root, offset) };
	const bind_t tmp = walk_variable(newroot, binding);
	if (holds_alternative<int>(tmp))
		return (tmp);
	p_bind_value p = get<p_bind_value>(tmp);
	const unique_ptr<token> &t = p->get_root()->get_first();
	if (t->is_intvalue())
		return bind_t {t->get_intvalue()};
	return tmp;
}

optional<vector<uint64_t>>
unification(const p_term &src, const p_term &dst, uint64_t srcoff,
		uint64_t dstoff, binding_t &binding)
{
	bind_t srctgt, dsttgt;
	srctgt = build_target(src, srcoff, binding);
	dsttgt = build_target(dst, dstoff, binding);
	return unification_sub(move(srctgt), move(dsttgt), binding);
}

// test
void
print_term(const bind_t &value, const unordered_map<uint64_t, string> v,
		const binding_t &binding)
{
	const bind_t &n = walk_variable(value, binding);

	if (holds_alternative<int>(n))
		cout << get<int>(n) << endl;

	p_bind_value t = get<p_bind_value>(n);

	const unique_ptr<token> &first = t->get_root()->get_first();
	const vector<p_term> &rest = t->get_root()->get_rest();
	uint64_t offset = t->get_base();
	cout << first->get_text();
	if (t->get_type() == symbol::atom && !rest.empty()) {
		cout << "(";
		for (auto &i : rest) {
			p_bind_value a = make_shared<bind_value>(i, offset);
			print_term(a, v, binding);
		}
		cout << ")";
	}
}

void
print_all(const unordered_map<uint64_t, string> &v, const binding_t &binding)
{
	for (auto &i :v) {
		auto n = binding.find(i.first);
		if (n != binding.end()) {
			auto &s = walk_variable(n->second, binding);
			cout << i.second << "=>";
			print_term(s, v, binding);
			cout << endl;
		}
	}
}
