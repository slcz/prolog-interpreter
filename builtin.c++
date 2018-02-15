#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <functional>
#include "parser.h"
#include "unification.h"
#include "unification-priv.h"

using namespace std;

using builtin_fn = optional<pair<control, vector<uint64_t>>> (*)(
      const vector<p_term> &, uint64_t, var_lookup &);

using arith_type = variant<int, float>;

optional<arith_type> unary_op(arith_type a0,
    function<int(int)> intf, function<float(float)> floatf)
{
	if (!intf) {
		if (!holds_alternative<float>(a0))
			return nullopt;
	}
	if (!floatf) {
		if (!holds_alternative<int>(a0))
			return nullopt;
	}
	return visit(overloaded {
	[&] (int a0)   -> arith_type { return intf(a0);   },
	[&] (float a0) -> arith_type { return floatf(a0); }
	}, a0);
}

template <typename R1, typename R2>
optional<arith_type> binary_op(arith_type a0, arith_type a1,
    function<R1(int,int)> intf, function<R2(float,float)> floatf)
{
	arith_type t0 = a0, t1 = a1;
	if (!intf) {
		if (holds_alternative<int>(a0)) {
			auto f = get<int>(a0);
			float f1 = (float)f;
			t0 = arith_type(f1);
		}
		if (!holds_alternative<float>(a1)) {
			auto f = get<int>(a1);
			float f1 = (float)f;
			t1 = arith_type(f1);
		}
	}
	if (!floatf) {
		if (!holds_alternative<int>(a0) ||
		    !holds_alternative<int>(a1))
			return nullopt;
	}
	return visit(overloaded {
	[&] (int t0) -> arith_type {
		return visit(overloaded {
			[&] (int t1) { return arith_type {intf(t0,t1)}; },
			[&] (float t1){return arith_type {floatf(t0,t1)}; }},
			t1);
	},
	[&] (float t0) -> arith_type {
		return visit(overloaded {
			[&] (int t1) { return arith_type { floatf(t0,t1) }; },
			[&] (float t1){ return arith_type { floatf(t0,t1)}; }},
			t1);
	} }, t0);
}

optional<arith_type> add(vector<arith_type> args) {
	return binary_op<int,float>(args[0], args[1], plus<>(), plus<>()); }

optional<arith_type> neg(vector<arith_type> args) {
	return unary_op(args[0], negate<>(), negate<>()); }

optional<arith_type> sub(vector<arith_type> args) {
	return binary_op<int,float>(args[0], args[1], minus<>(), minus<>());
}

optional<arith_type> mul(vector<arith_type> args) {
	return binary_op<int,float>(args[0], args[1], multiplies<>(), multiplies<>()); }

optional<arith_type> div(vector<arith_type> args) {
	return binary_op<int,float>(args[0], args[1], nullptr, divides<>()); }

optional<arith_type> idiv(vector<arith_type> args) {
	return binary_op<int,float>(args[0], args[1], divides<>(), nullptr); }

optional<arith_type> mod(vector<arith_type> args) {
	return binary_op<int,float>(args[0], args[1], modulus<>(), nullptr); }

using mtype = unordered_multimap<string,
      pair<optional<arith_type>(*)(vector<arith_type>), uint32_t>>;

mtype arith_ops = {
	{ "+",   {add, 2}},
	{ "-",   {sub, 2}},
	{ "-",   {neg, 1}},
	{ "*",   {mul, 2}},
	{ "/",   {div, 2}},
	{ "//",  {idiv,2}},
	{ "mod", {mod, 2}},
};

optional<arith_type> eval_arith(bind_value, var_lookup &);
optional<arith_type> eval_arith_sub(p_structure s, var_lookup &table)
{
	const auto &head = s->get_root()->get_first();
	const auto &body = s->get_root()->get_rest();
	if (head->get_type() == symbol::integer)
		return arith_type{head->get_int_value()};
	if (head->get_type() == symbol::decimal)
		return arith_type{head->get_decimal_value()};
	string op = head->get_text();
	uint64_t base = s->get_base();
	vector<arith_type> args;
	for (auto &i : body) {
		auto a = eval_arith(build_target(i, base, table), table);
		if (!a)
			return nullopt;
		args.push_back(*a);
	}
	auto m = arith_ops.equal_range(op);
	auto p = m.first;
	while (p != m.second) {
		if (p->second.second == args.size())
			return p->second.first(args);
		p ++;
	}
	return nullopt;
}

optional<arith_type> eval_arith(bind_value root, var_lookup &table)
{
	return visit(overloaded {
	[&table](p_structure s) -> optional<arith_type> {
		return eval_arith_sub(s, table);
	},
	[] (int s) -> optional<arith_type> {
		return arith_type {s};
	},
	[] (float s) -> optional<arith_type> {
		return arith_type {s};
	} }, root);
}

optional<pair<control, vector<uint64_t>>>
builtin_compare(const vector<p_term> &args, uint64_t base, var_lookup &table,
    function<bool(int,int)> intf, function<bool(float,float)> floatf)
{
	bind_value l = build_target(args[0], base, table);
	bind_value r = build_target(args[1], base, table);
	optional<arith_type> result1 = eval_arith(l, table);
	if (!result1)
		return nullopt;
	optional<arith_type> result2 = eval_arith(r, table);
	if (!result2)
		return nullopt;
	auto comp = binary_op<bool,bool>(*result1, *result2, intf, floatf);
	if (!comp)
		return nullopt;
	if (holds_alternative<int>(*comp)) {
		int res = get<int>(*comp);
		if (res)
			return make_pair(control::none, vector<uint64_t>());
	}
	return nullopt;
}

optional<pair<control, vector<uint64_t>>>
builtin_eq(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return builtin_compare(args, base, table, equal_to<>(), equal_to<>());
}

optional<pair<control, vector<uint64_t>>>
builtin_ne(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return builtin_compare(args, base, table, not_equal_to<>(),
			not_equal_to<>());
}

optional<pair<control, vector<uint64_t>>>
builtin_lt(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return builtin_compare(args, base, table, less<>(), less<>());
}

optional<pair<control, vector<uint64_t>>>
builtin_le(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return builtin_compare(args, base, table, less_equal<>(),
			less_equal<>());
}

optional<pair<control, vector<uint64_t>>>
builtin_gt(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return builtin_compare(args, base, table, greater<>(), greater<>());
}

optional<pair<control, vector<uint64_t>>>
builtin_ge(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return builtin_compare(args, base, table, greater_equal<>(),
			greater_equal<>());
}

optional<pair<control, vector<uint64_t>>>
builtin_is(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	vector<uint64_t> nil = vector<uint64_t>();
	bind_value l = build_target(args[0], base, table);
	bind_value r = build_target(args[1], base, table);
	optional<arith_type> result = eval_arith(r, table);
	if (!result)
		return nullopt;
	bind_value b = visit(overloaded {
	[](int v)   { return bind_value {v}; },
	[](float v) { return bind_value {v}; } }, *result);
	auto unify_rst = unification_sub(l, b, table);
	if (!unify_rst)
		return nullopt;
	else
		return make_pair(control::none, move(*unify_rst));
}

optional<pair<control, vector<uint64_t>>>
builtin_fail(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return make_pair(control::none, vector<uint64_t>());
}

optional<pair<control, vector<uint64_t>>>
builtin_cut(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	return make_pair(control::cut, vector<uint64_t>());
}

optional<pair<control, vector<uint64_t>>>
literal_compare(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	bool r = compare_terms(args[0], args[1], base, base, table);
	if (r)
		return make_pair(control::none, vector<uint64_t>());
	else
		return nullopt;
}

string chars2atom(string src)
{
	if (src == "")
		return "[]";
	string t;
	t.push_back(src[0]);
	return ".(" + t + "," + chars2atom(string(src.begin() + 1, src.end())) + ")";
}

optional<string> atom2chars(const p_term &src)
{
	if (src->get_first()->get_type() != symbol::atom)
		return nullopt;
	string text = src->get_first()->get_text();
	if (text == "[]")
		return "";
	if (text != ".")
		return nullopt;
	auto &r = src->get_rest();
	if (r.size() != 2)
		return nullopt;
	if (r[0]->get_first()->get_type() != symbol::atom)
		return nullopt;
	if (r[0]->get_rest().size() != 0)
		return nullopt;
	string build = r[0]->get_first()->get_text();
	if (build.size() != 1)
		return nullopt;
	auto k = atom2chars(r[1]);
	if (!k)
		return nullopt;
	return build + *k;
}

vector<p_term> generated_terms;

optional<pair<control, vector<uint64_t>>>
atom_chars(const vector<p_term> &args, uint64_t base, var_lookup &table)
{
	p_term atom;

	bind_value left  = build_target(args[0], base, table);
	auto p = get_structure(left);
	bind_value right = build_target(args[1], base, table);
	auto q = get_structure(right);
	if (!p && !q)
		return nullopt;

	if (p) {
		auto &r = (*p)->get_root();
		if (r->get_first()->get_type() != symbol::atom)
			return nullopt;
		if (!r->get_rest().empty())
			return nullopt;
		auto s = chars2atom(r->get_first()->get_text());
		optional<p_term> a = get_term(s);
		if (!a)
			return nullopt;
		if (!a)
			return nullopt;
		generated_terms.push_back(move(*a));
		left = build_target(generated_terms.back(), base, table);
		auto rtn = unification_sub(right, left, table);
		if (!rtn)
			return nullopt;
		return make_pair(control::none, move(*rtn));
	}
	auto t = atom2chars((*q)->get_root());
	if (!t)
		return nullopt;
	cout << *t << endl;
	auto x = get_term(*t);
	if (!x)
		return nullopt;
	generated_terms.push_back(move(*x));
	right = build_target(generated_terms.back(), base, table);
	return make_pair(control::none, *unification_sub(left, right, table));
}

unordered_map<string, pair<builtin_fn, uint32_t>> builtin_map = {
	{ "is",         {builtin_is,      2}},
	{ "=:=",        {builtin_eq,      2}},
	{ "=\\=",       {builtin_ne,      2}},
	{ "<",          {builtin_lt,      2}},
	{ ">",          {builtin_gt,      2}},
	{ "=<",         {builtin_le,      2}},
	{ ">=",         {builtin_ge,      2}},
	{ "==",         {literal_compare, 2}},
	{ "!",          {builtin_cut,     0}},
	{ "fail",       {builtin_fail,    0}},
	{ "atom_chars", {atom_chars,      2}},
};

optional<pair<control, vector<uint64_t>>>
builtin(const p_term &src, uint64_t srcoff, var_lookup &table)
{
	vector<uint64_t> nil = vector<uint64_t>();

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
	if (m->second.second != rest.size() || !f)
		return nullopt;
	auto r = f(rest, srcoff, table);
	return r;
}
