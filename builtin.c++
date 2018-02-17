#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <functional>
#include "parser.h"
#include "unification.h"

using namespace std;

const unordered_map<string, function<int(int,int)>> bi = {
	{ "+", plus<>()},
	{ "-", minus<>()},
	{ "*", multiplies<>()},
	{ "//",divides<>()},
};

const unordered_map<string, function<float(float,float)>> bf = {
	{ "+",  plus<>()},
	{ "-",  minus<>()},
	{ "*",  multiplies<>()},
	{ "/",  divides<>()}
};

const unordered_map<string, function<int(int)>> ui = {
	{ "-", negate<>()}
};
const unordered_map<string, function<float(float)>> uf = {
	{ "-", negate<>()}
};

template<typename T>
optional<T> composite_t::eval(
    const unordered_map<string, function<T(T,T)>> & binary_fns,
    const unordered_map<string, function<T(T)>> & unary_fns,
    vector<T> &leaves, var_lookup &table)
{
	auto &f = get_root()->get_first();
	auto &r = get_root()->get_rest();
	auto m = unary_fns.find(f->get_text());
	auto n = binary_fns.find(f->get_text());
	assert(r.size() == leaves.size());
	switch (r.size()) {
		case 1:
			if (m == unary_fns.end())
				return nullopt;
			return m->second(leaves[0]);
		case 2:
			if (n == binary_fns.end())
				return nullopt;
			return n->second(leaves[0], leaves[1]);
		default: return nullopt;
	}
}

optional<p_bind_value> eval_arith(p_bind_value node, var_lookup &table)
{
	optional<int> i = node->getint(table);
	if (i)
		return make_shared<primitive_t<int>>(*i);
	optional<float> f = node->getdecimal(table);
	if (f)
		return make_shared<primitive_t<float>>(*f);
	return nullopt;
}

optional<int> composite_t::getint(var_lookup &table)
{
	const vector<p_term> &r = get_root()->get_rest();
	vector<int> list;
	for (auto &i : r) {
		p_bind_value v = create_bind_value(i, get_base(), table);
		optional<int> t = v->getint(table);
		if (!t) return nullopt;
		list.push_back(*t);
	}
	return eval(bi, ui, list, table);
}

optional<float> composite_t::getdecimal(var_lookup &table)
{
	auto &r = get_root()->get_rest();
	vector<float> list;
	for (auto &i : r) {
		p_bind_value v = create_bind_value(i, get_base(), table);
		optional<float> t = v->getdecimal(table);
		if (!t) {
			optional<int> i = v->getint(table);
			if (!i)
				return nullopt;
			list.push_back((float)*i);
		} else
			list.push_back(*t);
	}
	return eval(bf, uf, list, table);
}

#define variable_get(name, type) \
optional<type> variable_t::name(var_lookup &table) \
{ \
	const p_bind_value & n = walk(p_bind_value {nullptr}, table); \
	if (!n) \
		return nullopt; \
	return n->getint(table); \
}

variable_get(getint, int)
variable_get(getdecimal, float)

optional<builtin_t>
builtin_is(const vector<p_term> &args, uint64_t base, var_lookup &table,
		const string &)
{
	p_bind_value l = create_bind_value(args[0], base, table);
	p_bind_value r = create_bind_value(args[1], base, table);
	optional<p_bind_value> result = eval_arith(r, table);
	if (!result)
		return nullopt;
	auto unify_rst = l->unification(*result, table, false);
	if (!unify_rst)
		return nullopt;
	else
		return make_pair(control::none, move(*unify_rst));
}

unordered_map<string,function<bool(int,int)>> compare_op_int = {
	{ "=:=",        equal_to<>(),    },
	{ "=\\=",       not_equal_to<>(),},
	{ "<",          less<>(),        },
	{ ">",          greater<>(),     },
	{ "=<",         less_equal<>(),  },
	{ ">=",         greater_equal<>()},
};

unordered_map<string,function<bool(float,float)>> compare_op_decimal = {
	{ "=:=",        equal_to<>(),    },
	{ "=\\=",       not_equal_to<>(),},
	{ "<",          less<>(),        },
	{ ">",          greater<>(),     },
	{ "=<",         less_equal<>(),  },
	{ ">=",         greater_equal<>()},
};

optional<builtin_t> builtin_compare(const vector<p_term> &args, uint64_t base,
    var_lookup &table, const string &op)
{
	p_bind_value l = create_bind_value(args[0], base, table);
	p_bind_value r = create_bind_value(args[1], base, table);
	optional<p_bind_value> lr = eval_arith(l, table);
	optional<p_bind_value> rr = eval_arith(r, table);
	if (!lr || !rr)
		return nullopt;
	optional<int> a = (*lr)->getint(table);
	optional<int> b = (*rr)->getint(table);
	auto m = compare_op_int.find(op);
	assert(m != compare_op_int.end());
	if (a && b) {
		if (m->second(*a, *b))
			return make_pair(control::none, vector<uint64_t>());
	}
	optional<float> fa = (*lr)->getdecimal(table);
	optional<float> fb = (*rr)->getdecimal(table);
	auto n = compare_op_decimal.find(op);
	assert(n != compare_op_decimal.end());
	if (fa && fb) {
		if (m->second(*fa, *fb))
			return make_pair(control::none, vector<uint64_t>());
	}
	return nullopt;
}

optional<builtin_t>
builtin_fail(const vector<p_term> &, uint64_t, var_lookup &, const string &)
{
	return make_pair(control::fail, vector<uint64_t>());
}

optional<builtin_t>
builtin_cut(const vector<p_term> &, uint64_t, var_lookup &, const string &)
{
	return make_pair(control::cut, vector<uint64_t>());
}

optional<builtin_t>
literal_compare(const vector<p_term> &args, uint64_t base, var_lookup &table, const string &)
{
	/* unification but don't change variable bindings */
	auto r = unification(args[0], args[1], base, base, table, true);
	if (r)
		return make_pair(control::none, vector<uint64_t>());
	else
		return nullopt;
}

using builtin_fn = optional<builtin_t>(*)(const vector<p_term> &,
        uint64_t, var_lookup &, const string &);
unordered_map<string, pair<builtin_fn, uint32_t>> builtin_map = {
	{ "is",         {builtin_is,      2}},
	{ "=:=",        {builtin_compare, 2}},
	{ "=\\=",       {builtin_compare, 2}},
	{ "<",          {builtin_compare, 2}},
	{ ">",          {builtin_compare, 2}},
	{ "=<",         {builtin_compare, 2}},
	{ ">=",         {builtin_compare, 2}},
	{ "==",         {literal_compare, 2}},
	{ "!",          {builtin_cut,     0}},
	{ "fail",       {builtin_fail,    0}},
};

optional<builtin_t>
composite_t::builtin(uint64_t base, var_lookup &table)
{
	auto m = builtin_map.find(get_root()->get_first()->get_text());
	if (m == builtin_map.end())
		return nullopt;
	builtin_fn f = m->second.first;
	if (m->second.second != get_root()->get_rest().size() || !f)
		return nullopt;
	auto r = f(get_root()->get_rest(), base, table, m->first);
	return r;
}

optional<builtin_t>
builtin(const p_term &term, uint64_t base, var_lookup &table)
{
	vector<uint64_t> nil = vector<uint64_t>();

	p_bind_value value = create_bind_value(term, base, table);
	return value->builtin(base, table);
}
