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

const p_bind_value & walk_variable(const p_bind_value &t, const binding_t &binding)
{
	if (t->get_type() == symbol::variable) {
		auto n = binding.find(t->get_id());
		if (n != binding.end()) {
			assert(n->second);
			return walk_variable(n->second, binding);
		}
	}
	return t;
}

optional<vector<uint64_t>> unification(const p_term &, const p_term &,
		uint64_t, uint64_t, binding_t &);
optional<vector<uint64_t>> unify_rest(p_bind_value &src, p_bind_value &dst,
		binding_t &binding)
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
bool detect_loop(uint64_t id, const p_bind_value &t, const binding_t &binding)
{
	const p_term &root = t->get_root();
	uint64_t offset = t->get_base();

	if (t->get_type() == symbol::variable) {
		if (t->get_id() == id)
			return false;
		const p_bind_value &tmp = walk_variable(t, binding);
		if (t == tmp)
			return true;
		return detect_loop(id, tmp, binding);
	} else {
		assert(t->get_type() == symbol::atom);
		for (const auto &i : root->get_rest()) {
			auto tmp = make_unique<bind_value>(i, offset);
			if (!detect_loop(id, tmp, binding))
				return false;
		}
		return true;
	}
	return true;
}

optional<uint64_t> bind(p_bind_value &from, p_bind_value to, binding_t &binding)
{
	uint64_t id = from->get_id();
	assert(id != 0);
	if (is_wildcard(from->get_root()->get_first()) ||
	    is_wildcard(to->get_root()->get_first()))
		return 0;
	if (!detect_loop(id, to, binding))
		return nullopt;
	assert(to);
	binding.insert(make_pair(id, move(to)));
	return id;
}

optional<vector<uint64_t>> unification_sub(p_bind_value src,
    p_bind_value dst, binding_t &binding)
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
		p_bind_value &p1 = dtype == symbol::variable ? dst : src;
		p_bind_value &p2 = dtype == symbol::variable ? src : dst;
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

p_bind_value
build_target(const p_term &root, uint64_t offset, binding_t &binding)
{
	p_bind_value newroot;
	newroot = make_unique<bind_value>(root, offset);
	auto &tmp = walk_variable(newroot, binding);
	if (tmp != newroot)
		newroot = make_unique<bind_value>(tmp->get_root(),
				tmp->get_base());
	return newroot;
}

optional<vector<uint64_t>>
unification(const p_term &src, const p_term &dst, uint64_t srcoff,
		uint64_t dstoff, binding_t &binding)
{
	p_bind_value srctgt, dsttgt;
	srctgt = build_target(src, srcoff, binding);
	dsttgt = build_target(dst, dstoff, binding);
	return unification_sub(move(srctgt), move(dsttgt), binding);
}

// test
void
print_term(const p_bind_value &t, const unordered_map<uint64_t, string> v,
		const binding_t &binding)
{
	const p_bind_value &n = walk_variable(t, binding);
	const unique_ptr<token> &first = n->get_root()->get_first();
	const vector<p_term> &rest = n->get_root()->get_rest();
	uint64_t offset = t->get_base();
	cout << first->get_text();
	if (n->get_type() == symbol::atom && !rest.empty()) {
		cout << "(";
		for (auto &i : rest) {
			p_bind_value a = make_unique<bind_value>(i, offset);
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
			assert(n->second);
			auto &s = walk_variable(n->second, binding);
			cout << i.second << "=>";
			print_term(s, v, binding);
			cout << endl;
		}
	}
}
