#include <iostream>
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include "parser.h"
#include "unification.h"

using namespace std;

void remove_from_table(var_lookup &table, const vector<uint64_t> &list)
{
	for (auto item : list)
		assert(table.erase(item) == 1);
}

const p_bind_value &
variable_t::walk(const p_bind_value &fail, const var_lookup & table)
{
	auto entry = table.find(get_id());
	if (entry == table.end())
		return fail;
	return entry->second->walk(entry->second, table);
}

p_bind_value create_bind_value(const p_term &term, uint64_t base,
    const var_lookup &table)
{
	const unique_ptr<token> &tok = term->get_first();
	if (tok->get_type() == symbol::integer)
		return make_shared<primitive_t<int>>(tok->get_int_value());
	if (tok->get_type() == symbol::decimal)
		return make_shared<primitive_t<float>>(tok->get_decimal_value());
	if (tok->get_type() == symbol::atom)
		return make_shared<composite_t>(term, base);
	p_bind_value t  = make_shared<variable_t>(term, base);
	return t->walk(t, table);
}

bool composite_t::loop(uint64_t id, const var_lookup &table)
{
	for (auto &child : root->get_rest()) {
		auto n = create_bind_value(child, get_base(), table);
		if (n->loop(id, table))
			return true;
	}
	return false;
}

maybe_ids variable_t::unification(p_bind_value &tgt,var_lookup &table, bool cmp)
{
	if (tgt->loop(get_id(), table))
		return nullopt;
	if (!cmp) {
		table.insert(make_pair(get_id(), move(tgt)));
		vector<uint64_t> v { get_id() };
		return v;
	} else
		return vector<uint64_t>();
}

maybe_ids composite_t::unification(p_bind_value &tgt, var_lookup &table, bool cmp)
{
	shared_ptr<composite_t> c = dynamic_pointer_cast<composite_t>(tgt);
	if (!c)
		return nullopt;
	if (c->get_id() != get_id())
		return nullopt;
	auto &a1 = get_root()->get_rest();
	auto &a2 = c->get_root()->get_rest();
	auto ss = a1.begin(), se = a1.end();
	auto ds = a2.begin(), de = a2.end();
	uint64_t srcoff = get_base(), dstoff = c->get_base();
	vector<uint64_t> vars;
	for (; ss != se && ds != de; ss ++, ds ++) {
		maybe_ids r;
		if ((r = ::unification(*ss,*ds, srcoff, dstoff, table, cmp))) {
			vars.insert(vars.end(), r->begin(), r->end());
		} else {
			remove_from_table(table, vars);
			return nullopt;
		}
	}
	if (ss == se && ds == de)
		return move(vars);
	remove_from_table(table, vars);
	return nullopt;
}

template<typename T>
maybe_ids primitive_t<T>::unification(p_bind_value &tgt, var_lookup &table,
    bool cmp)
{
	auto c = dynamic_pointer_cast<primitive_t<T>>(tgt);
	if (!c)
		return nullopt;
	if (c->value == value)
		return vector<uint64_t>();
	else
		return nullopt;
}

maybe_ids unification(const p_term &src, const p_term &dst, uint64_t src_base,
    uint64_t dst_base, var_lookup &table, bool cmp_only)
{
	p_bind_value src_value = create_bind_value(src, src_base, table);
	p_bind_value dst_value = create_bind_value(dst, dst_base, table);

	auto r = src_value->unification(dst_value, table, cmp_only);
	if (!r)
		r = dst_value->unification(src_value, table, cmp_only);
	return r;
}

string variable_t::tostring(const var_lookup &table)
{
	stringstream os;
	auto n = walk(p_bind_value {nullptr}, table);
	if (!n)
		os << get_root()->get_first()->get_text();
	else
		os << bind_env {n, table };
	return os.str();
}

optional<string> composite_t::list2string(const var_lookup &table)
{
	string s1, s2;
	const vector<p_term> &rest = get_root()->get_rest();
	if (get_root()->get_first()->get_text() != "." || rest.size() != 2)
		return nullopt;
	const p_bind_value l = create_bind_value(rest[0], get_base(), table);
	const p_bind_value r = create_bind_value(rest[1], get_base(), table);
	s1 = "[";
	s1 += l->tostring(table);
	s2 = r->tostring(table);
	if (s2 == "[]")
		return s1 + "]";
	else if (s2[0] == '[' && s2[s2.size() - 1] == ']')
		return s1 + ", " + string(s2.begin() + 1, s2.end() - 1) + "]";
	else
		return s1 + "| " + s2 + "]";
}

string composite_t::tostring(const var_lookup &table)
{
	stringstream os;
	auto &rest = get_root()->get_rest();
	auto a = list2string(table);
	if (a) {
		os << *a;
		return os.str();
	}
	os << get_root()->get_first()->get_text();
	if (rest.empty())
		return os.str();
	os << "(";
	auto item = rest.begin();
	while (item != rest.end()) {
		p_bind_value n = create_bind_value(*item, get_base(), table);
		os << bind_env { n, table };
		item ++;
		if (item != rest.end())
			os << ", ";
	}
	os << ")";
	return os.str();
}

ostream &operator<<(ostream &os, const bind_env &b)
{
	os << b.node->tostring(b.map);
	return os;
}

void
print_all(const unordered_map<uint64_t, string> &v, const var_lookup table)
{
	for (auto &i :v) {
		auto n = table.find(i.first);
		if (n == table.end())
			continue;
		cout << i.second << "=>" << bind_env{n->second, table} << endl;
	}
}
