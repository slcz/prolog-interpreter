#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <functional>
#include "parser.h"
#include "unification.h"

using namespace std;

template<typename T> optional<vector<T>>
eval_extract(composite_t &c, function<optional<T>(p_bind_value &)> access,
   var_lookup &table, const char *op, size_t nargs)
{
	auto &f = c.get_root()->get_first();
	auto &r = c.get_root()->get_rest();
	if (f->get_text() != string(op) || r.size() != nargs)
		return nullopt;
	vector<T> build;
	for (auto &item : r) {
		p_bind_value v = create_bind_value(item, c.get_base(), table);
		optional<T> a = access(v);
		if (!a)
			return vector<T>();
		build.push_back(*a);
	}
	return build;
}

template<typename T> optional<T> eval(composite_t &,
    function<optional<T>(p_bind_value &)>, var_lookup &) { return nullopt; }

template<typename T, typename... Args>
optional<T> eval(composite_t &c,
    function<optional<T>(p_bind_value &)> access, var_lookup &table,
    const char *op, function<T(T)> fn, Args... args)
{
	const size_t fnargs = 1;
	optional<vector<T>> v = eval_extract(c, access, table, op, fnargs);
	if (!v)
		return eval<T>(c, access, table, args...);
	if (v->size() != fnargs)
		return nullopt;
	return fn((*v)[0]);
}

template<typename T, typename... Args>
optional<T> eval(composite_t &c,
    function<optional<T>(p_bind_value &)> access, var_lookup &table,
    const char *op, function<T(T,T)> fn, Args... args)
{
	const size_t fnargs = 2;
	optional<vector<T>> v = eval_extract(c, access, table, op, fnargs);
	if (!v)
		return eval<T>(c, access, table, args...);
	if (v->size() != fnargs)
		return nullopt;
	return fn((*v)[0], (*v)[1]);
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
	return eval<int>(*this, [&](p_bind_value &p)
	{ return p->getint(table); }, table,
	"+", plus<>(),   "-", minus<>(),
	"-", negate<>(), "*", multiplies<>(), "//", divides<>());
}

optional<float> composite_t::getdecimal(var_lookup &table)
{
	return eval<float>(*this, [&](p_bind_value &p) -> optional<float> {
		auto r = p->getdecimal(table);
		auto i = p->getint(table);
		if (r) return *r;
		if (i) return float(*i);
		return nullopt;
	}, table,
	"+", plus<>(),   "-", minus<>(),
	"-", negate<>(), "*", multiplies<>(), "/", divides<>());
}

template<typename T> optional<T> var_access(variable_t &c, var_lookup &table,
    function<optional<T>(const p_bind_value &)> access)
{
	const p_bind_value & n = c.walk(p_bind_value {nullptr}, table);
	if (!n)
		return nullopt;
	return access(n);
}

optional<int> variable_t::getint(var_lookup &t)
{
	return
	var_access<int>(*this, t, [&](const p_bind_value &p) { return p->getint(t);});
}

optional<float> variable_t::getdecimal(var_lookup &t)
{
	return
	var_access<float>(*this,t,[&](const p_bind_value &p){return p->getdecimal(t);});
}

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
