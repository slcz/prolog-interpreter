#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <functional>
#include <cmath>
#include "parser.h"
#include "unification.h"

using namespace std;

template<typename T> optional<vector<T>>
eval_extract(composite_t &c,
    function<optional<T>(p_bind_value &, var_lookup &)> access,
    var_lookup &table, const char *op, size_t nargs)
{
	auto &f = c.get_root()->get_first();
	auto &r = c.get_root()->get_rest();
	if (f->get_text() != string(op) || r.size() != nargs)
		return nullopt;
	vector<T> build;
	for (auto &item : r) {
		p_bind_value v = create_bind_value(item, c.get_base(), table);
		optional<T> a = access(v, table);
		if (!a)
			return vector<T>();
		build.push_back(*a);
	}
	return build;
}

template<typename T> optional<T> eval(composite_t &, function<optional<T>(
    p_bind_value &, var_lookup &)>, var_lookup &) { return nullopt; }

template<typename T, typename... Args>
optional<T> eval(composite_t &c,
    function<optional<T>(p_bind_value &, var_lookup &)> access,
    var_lookup &table, const char *op, function<T(T)> fn, Args... args)
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
    function<optional<T>(p_bind_value &, var_lookup &)> access,
    var_lookup &table, const char *op, function<T(T,T)> fn, Args... args)
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

optional<int> access_int(p_bind_value &p, var_lookup &table)
{
	return p->getint(table);
}

optional<float> access_decimal(p_bind_value &p, var_lookup &table)
{
	auto r = p->getdecimal(table);
	auto i = p->getint(table);
	if (r) return *r;
	if (i) return float(*i);
	return nullopt;
}

#define MATH_BIN_FUNCTOR(n, f) \
template<typename T = float> \
struct n { T operator() (T l, T r) { return f(l, r);}}

#define MATH_UNA_FUNCTOR(n, f) \
template<typename T = float> \
struct n { T operator() (T l) { return f(l);}}

MATH_BIN_FUNCTOR(power,     powf);
MATH_UNA_FUNCTOR(abs_f,     abs);
MATH_UNA_FUNCTOR(atan_f,    atanf);
MATH_UNA_FUNCTOR(ceiling_f, ceil);
MATH_UNA_FUNCTOR(cos_f,     cosf);
MATH_UNA_FUNCTOR(exp_f,     expf);
MATH_UNA_FUNCTOR(sqrt_f,    sqrtf);
MATH_UNA_FUNCTOR(floor_f,   floorf);
MATH_UNA_FUNCTOR(log_f,     logf);
MATH_UNA_FUNCTOR(sin_f,     sinf);
MATH_UNA_FUNCTOR(truncate_f,truncf);
MATH_UNA_FUNCTOR(round_f,   roundf);

template<typename T = int>
struct rshift { T operator() (T l, T r) { return l >> r;}};

template<typename T = int>
struct lshift { T operator() (T l, T r) { return l << r;}};

optional<int> composite_t::getint(var_lookup &table)
{
	return eval<int>(*this, access_int, table,
	"+", plus<>(),   "-", minus<>(), "rem",modulus<>(),
	"-", negate<>(), "*", multiplies<>(), "//", divides<>(),
	">>",rshift<>(), "<<",lshift<>(), "/\\", bit_and<>(),
	"\\/", bit_or<>(), "\\", bit_not<>());
}

optional<float> composite_t::getdecimal(var_lookup &table)
{
	return eval<float>(*this, access_decimal, table,
	"+", plus<>(),   "-", minus<>(), "-", negate<>(), "*", multiplies<>(),
	"/", divides<>(),  "**", power<>(), "abs", abs_f<>(),
	"atan", atan_f<>(), "ceiling", ceiling_f<>(), "cos", cos_f<>(),
	"exp", exp_f<>(), "sqrt", sqrt_f<>(), "floor", floor_f<>(),
	"log", log_f<>(), "sin", sin_f<>(), "truncate", truncate_f<>(),
	"round", round_f<>());
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
	return var_access<int>(*this, t,
	[&](const p_bind_value &p) {return p->getint(t);});
}

optional<float> variable_t::getdecimal(var_lookup &t)
{
	return var_access<float>(*this, t,
	[&](const p_bind_value &p){ return p->getdecimal(t);});
}

optional<builtin_t>
builtin_is(const vector<p_term> &args, uint64_t base, var_lookup &table,
		const string &, struct env &)
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

template<typename T, typename... Args>
optional<builtin_t> _compare(const vector<p_term> &, uint64_t, var_lookup &,
    const string &, function<optional<T>(p_bind_value &, var_lookup &)>)
{ return nullopt; }

template<typename T, typename... Args>
optional<builtin_t> _compare(const vector<p_term> &args, uint64_t base,
    var_lookup &table, const string &op,
    function<optional<T>(p_bind_value &, var_lookup &)> access,
    const char *candidate, function<bool(T,T)> fn, Args... a)
{
	if (string(candidate) != op)
		return _compare<T>(args, base, table, op, access, a...);
	p_bind_value l = create_bind_value(args[0], base, table);
	p_bind_value r = create_bind_value(args[1], base, table);
	optional<p_bind_value> lr = eval_arith(l, table);
	optional<p_bind_value> rr = eval_arith(r, table);
	if (!lr || !rr)
		return nullopt;
	optional<T> a0 = access(*lr, table);
	optional<T> a1 = access(*rr, table);
	if (a0 && a1 && fn(*a0, *a1))
		return make_pair(control::none, vector<uint64_t>());
	return nullopt;
}

optional<builtin_t> builtin_compare(const vector<p_term> &args, uint64_t base,
    var_lookup &table, const string &op, struct env &)
{
	auto rint = _compare<int>(args, base, table, op, access_int,
		"=:=", equal_to<>(), "=\\=", not_equal_to<>(),
		"<",   less<>(), ">", greater<>(), "=<", less_equal<>(),
		">=",  greater_equal<>());
	if (rint)
		return rint;
	auto dint = _compare<float>(args, base, table, op, access_decimal,
		"=:=", equal_to<>(), "=\\=", not_equal_to<>(),
		"<",   less<>(), ">", greater<>(), "=<", less_equal<>(),
		">=",  greater_equal<>());
	if (dint)
		return dint;
	return nullopt;
}

optional<builtin_t>
builtin_fail(const vector<p_term> &, uint64_t, var_lookup &, const string &,
		struct env &)
{
	return make_pair(control::fail, vector<uint64_t>());
}

optional<builtin_t>
builtin_cut(const vector<p_term> &, uint64_t, var_lookup &, const string &,
		struct env &)
{
	return make_pair(control::cut, vector<uint64_t>());
}

optional<builtin_t>
literal_compare(const vector<p_term> &args, uint64_t base, var_lookup &table,
		const string &, struct env &)
{
	/* unification but don't change variable bindings */
	auto r = unification(args[0], args[1], base, base, table, true);
	if (r)
		return make_pair(control::none, vector<uint64_t>());
	else
		return nullopt;
}

optional<string> composite_t::atom2chars(const var_lookup &table)
{
	if (root->get_first()->get_text() != ".")
		return nullopt;
	if (root->get_first()->get_text() == "[]")
		return string("");
	auto &rest = root->get_rest();
	if (rest.size() != 2)
		return nullopt;
	p_bind_value b0 = create_bind_value(rest[0], base, table);
	optional<string> a0 = b0->list2string(table);
	p_bind_value b1 = create_bind_value(rest[1], base, table);
	optional<string> a1 = b1->list2string(table);
	if (!a0 || !a1)
		return nullopt;
	if (a0->size() != 1)
		return nullopt;
	return *a0 + *a1;
}

using builtin_fn = optional<builtin_t>(*)(const vector<p_term> &,
        uint64_t, var_lookup &, const string &, struct env &);
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
composite_t::builtin(uint64_t base, var_lookup &table, struct env &env)
{
	auto m = builtin_map.find(get_root()->get_first()->get_text());
	if (m == builtin_map.end())
		return nullopt;
	builtin_fn f = m->second.first;
	if (m->second.second != get_root()->get_rest().size() || !f)
		return nullopt;
	auto r = f(get_root()->get_rest(), base, table, m->first, env);
	return r;
}

optional<builtin_t>
builtin(const p_term &term, uint64_t base, var_lookup &table, struct env &env)
{
	vector<uint64_t> nil = vector<uint64_t>();

	p_bind_value value = create_bind_value(term, base, table);
	return value->builtin(base, table, env);
}
