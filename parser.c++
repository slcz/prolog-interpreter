#include <iostream>
#include <exception>
#include <optional>
#include <string>
#include "parser.h"
#include "interpreter.h"

using namespace std;

class syntax_error : public exception {
private:
	position_t position;
	std::string message;
public:
	syntax_error(const position_t &p, const std::string &m)
		: position{p}, message{m} {
		std::stringstream s;
		s << "<" << position.first << "," << position.second <<
			">: " << "Syntax error: " << message;
		message = s.str();
	}
	const char *what() const throw () {
		return message.c_str();
	}
};

enum class assoc_t { xfy, yfx, xfx, fx, fy, xf, yf, x };

class op_t {
private:
	const assoc_t assoc;
	int           pred;
public:
	op_t(const assoc_t a, int p) : assoc {a}, pred {p} {}
	int  get_pred()  const { return pred;  }
	bool unary() const { return assoc == assoc_t::fx ||
	assoc == assoc_t::fy || assoc == assoc_t::xf || assoc == assoc_t::yf;}
	bool binary() const { return !unary() && !null(); }
	bool prefix() const { return assoc == assoc_t::fy ||
		assoc == assoc_t::fx; }
	bool infix()  const { return assoc == assoc_t::xfy ||
		assoc == assoc_t::yfx || assoc == assoc_t::xfx; }
	bool postfix() const { return assoc == assoc_t::yf ||
		assoc == assoc_t::xf; }
	bool lassoc() const { return assoc == assoc_t::yfx; }
	bool rassoc() const { return assoc == assoc_t::xfy; }
	bool noassoc() const { return assoc == assoc_t::xfx ||
	assoc == assoc_t::fx || assoc == assoc_t::xf;}
	bool xassoc() const { return !noassoc(); }
	bool null() const { return assoc == assoc_t::x; }
};

class operator_t {
private:
	op_t dummy;
	unordered_map<string, op_t> operators;
	set<int, greater<int>> pred_set;
public:
	void insert(string o, op_t op) {
		operators.insert(make_pair<string, op_t>(move(o), move(op)));
		pred_set.insert(op.get_pred());
	}
	op_t &getop(const string key) {
		auto m = operators.find(key);
		if (m == operators.end())
			return dummy;
		else
			return (*m).second;
	}
	int lowest() { return *pred_set.begin(); }
	optional<int> higher(int last) {
		auto m = pred_set.upper_bound(last);
		if (m == pred_set.end())
			return nullopt;
		else
			return *m;
	}
	operator_t() : dummy { op_t {assoc_t::x, 0} }, operators {
		{ ":",   { assoc_t::xfx,  50 }},
		{ "@",   { assoc_t::xfx, 100 }},
		{ "\\",  { assoc_t::fy,  200 }},
		{ "-",   { assoc_t::fy,  200 }},
		{ "^",   { assoc_t::xfy, 200 }},
		{ "**",  { assoc_t::xfx, 200 }},
		{ "*",   { assoc_t::yfx, 400 }},
		{ "/",   { assoc_t::yfx, 400 }},
		{ "//",  { assoc_t::yfx, 400 }},
		{ "rem", { assoc_t::yfx, 400 }},
		{ "mod", { assoc_t::yfx, 400 }},
		{ "<<",  { assoc_t::yfx, 400 }},
		{ ">>",  { assoc_t::yfx, 400 }},
		{ "+",   { assoc_t::yfx, 500 }},
		{ "-",   { assoc_t::yfx, 500 }},
		{ "/\\", { assoc_t::yfx, 500 }},
		{ "\\/", { assoc_t::yfx, 500 }},
		{ "=",   { assoc_t::xfx, 700 }},
		{ "\\=", { assoc_t::xfx, 700 }},
		{ "==",  { assoc_t::xfx, 700 }},
		{ "\\==",{ assoc_t::xfx, 700 }},
		{ "@<",  { assoc_t::xfx, 700 }},
		{ "@=<", { assoc_t::xfx, 700 }},
		{ "@>",  { assoc_t::xfx, 700 }},
		{ "@>=", { assoc_t::xfx, 700 }},
		{ "is",  { assoc_t::xfx, 700 }},
		{ "=:=", { assoc_t::xfx, 700 }},
		{ "=\\=",{ assoc_t::xfx, 700 }},
		{ "<",   { assoc_t::xfx, 700 }},
		{ "=<",  { assoc_t::xfx, 700 }},
		{ ">",   { assoc_t::xfx, 700 }},
		{ ">=",  { assoc_t::xfx, 700 }},
		{ "=..", { assoc_t::xfx, 700 }},
		{ ",",   { assoc_t::xfy,1000 }},
		{ "->",  { assoc_t::xfy,1050 }},
		{ ";",   { assoc_t::xfy,1100 }},
		{ ":-",  { assoc_t::fx, 1200 }},
		{ ":-",  { assoc_t::xfx,1200 }},
		{ "-->", { assoc_t::xfx,1200 }},
	} {
		for_each(operators.begin(), operators.end(),
		    [this](const pair<string, op_t>& ent) {
		    	pred_set.insert(ent.second.get_pred()); });
	}
};

struct token_parser_entry {
	regex pat;
	symbol sym;
} token_parser_table[] = {
	{regex("^[[:space:]]+"),                symbol::ignore  },
	{regex("^%.*$"),                        symbol::ignore  },
	{regex(R"(^/\*[\S\s]*?\*/)"),           symbol::ignore  },
	{regex(R"(^/\*.*)"),                    symbol::append  },
	{regex("^,"),                           symbol::comma   },
	{regex("^\\("),                         symbol::lparen  },
	{regex("^\\)"),                         symbol::rparen  },
	{regex("^[[:lower:]][[:alnum:]_]*"),    symbol::atom    },
	{regex("^\\?-"),                        symbol::query   },
	{regex("^[0-9]+"),                      symbol::atom    },
	{regex("^:-"),                          symbol::rules   },
	{regex("^[#$&*+-./:<=>?@^~\\\\]+"),     symbol::atom    },
	{regex(R"(^'(\\.|[^'\\])*')"),          symbol::atom    },
	{regex(R"(^'.*)"),                      symbol::append  },
	{regex("^[_$[:upper:]][_$[:alnum:]]*"), symbol::variable},
	{regex("^."),                           symbol::error   }
};

template <typename T, size_t N> T* table_begin(T(&arr)[N]) { return &arr[0];   }
template <typename T, size_t N> T* table_end(T(&arr)[N])   { return &arr[0]+N; }

unique_ptr<token>
parse_token(const string::iterator begin, const string::iterator end)
{
	auto t = make_unique<token>(symbol::error);

	for (auto entry = table_begin(token_parser_table);
	          entry != table_end(token_parser_table); entry ++) {
		if (t->match(begin, end, entry->pat, entry->sym))
			break;
	}
	return t;
}

class interp_context {
	using transformer_t = unique_ptr<token> (*)(std::unique_ptr<token>);
private:
	istream &in;
	string str;
	size_t offset;
	vector<unique_ptr<token>> token_stack;
	set<transformer_t> transformers;
	unique_ptr<token> _get_token();
	unique_ptr<token> pop() {
		if (token_stack.empty())
			return unique_ptr<token>(nullptr);
		auto x = move(token_stack.back());
		token_stack.pop_back();
		return x;
	}
	position_t position;
public:
	interp_context(istream &is): in{is}, offset{0}, position{0,0} {}
	unique_id atom_id;
	unique_id var_id;
	unique_ptr<token> get_token();
	void ins_transformer(transformer_t t) { transformers.insert(t); }
	void rmv_transformer(transformer_t t) { transformers.erase(t);  }
	void push(unique_ptr<token> &t) { token_stack.push_back(move(t)); }
	const position_t & get_position() const { return position; }
	operator_t ops;
};

unique_ptr<token> interp_context::_get_token()
{
	unique_ptr<token> next;
	position_t token_position;

	if ((next = pop()) != nullptr)
		return next;

	next = make_unique<token>(symbol::none);
	do {
		while (str.begin() + offset == str.end()) {
			if (!getline(in, str))
				next = make_unique<token>(symbol::eof);
			else {
				offset = 0;
				position.first ++;
				position.second = 1;
			}
		}
		token_position = position;
		if (next->get_type() != symbol::eof) {
			token_position = position;
			next = parse_token(str.begin() + offset, str.end());
			while (next->get_type() == symbol::append) {
				string line_continue;
				if (!getline(in, line_continue))
					next->set_type(symbol::error);
				else {
					str += '\n' + line_continue;
					position.first ++;
					position.second = 1;
					next = parse_token(
					       str.begin() + offset, str.end());
				}
			}
			auto length = next->get_text().length();
			offset += length;
			position.second += length;
		}
	} while (next->get_type() == symbol::ignore);

	next->set_position(token_position);
	return (next);
}

unique_ptr<token> interp_context::get_token()
{
	auto t = _get_token();
	for (auto transformer : transformers)
		t = transformer(move(t));
	return t;
}

unique_ptr<token> period_transformer(unique_ptr<token> t)
{
	if (t->get_type() == symbol::atom && t->get_text() == ".")
		t->set_type(symbol::period);
	return t;
}

unique_ptr<token> expect_period(interp_context &context)
{
	unique_ptr<token> t;
	context.ins_transformer(period_transformer);
	t = context.get_token();
	context.rmv_transformer(period_transformer);
	return t;
}

ostream&
operator<<(ostream& os, const term& c)
{
	symbol type = c.first->get_type();
	string p = type == symbol::atom ? "A" :
	           type == symbol::variable ? "V" : "?";
	int ident = c.ident;
	for (int i = 0; i < ident; i ++)
		os << " ";
	os << "<" << p << ">" << c.first->get_text() << "</" << p << ">" <<endl;
	for (auto &i : c.rest) {
		(*i).set_ident(c.ident + 1);
		os << *i;
	}
	return os;
}

template<typename T> vector<T>
many(interp_context &context, optional<T>(*unit)(interp_context &),
		const symbol delimiter = symbol::none)
{
	unique_ptr<token> tok;
	vector<T> vec;
	optional<T> t;
	bool next = false, done = false;

	while (!done && (t = unit(context))) {
		vec.push_back(move(*t));
		if (delimiter != symbol::none) {
			if ((tok = context.get_token())->get_type() == delimiter) {
				next = true;
			} else {
				context.push(tok);
				next = false;
				done = true;
			}
		}
	}
	if (next)
		throw syntax_error(context.get_position(), "unexpected char");
	return (vec);
}

optional<p_term> parse_exp(interp_context &, int);
optional<p_term> parse_term(interp_context &);
optional<p_term> parse_expression(interp_context &);
optional<p_term> parse_exp_next(interp_context &context, int priority)
{
	auto new_prio = context.ops.higher(priority);
	optional<p_term> r;
	if (new_prio)
		r = parse_exp(context, *new_prio);
	else {
		unique_ptr<token> t;

		t = context.get_token();
		if (t->get_type() == symbol::lparen) {
			r = parse_expression(context);
			if (context.get_token()->get_type() != symbol::rparen)
				throw syntax_error(context.get_position(),
						") expected");
		} else {
			context.push(t);
			r = parse_term(context);
		}
	}
	return (r);
}

class exp_return {
public:
	p_term            exp; // can be empty
	unique_ptr<token> tok; // can be empty
	bool              ok;
	bool              cont;
};

class exp_param {
public:
	interp_context &context;
	int            priority;
	exp_param(interp_context &c, int prio) : context{c}, priority {prio} {}
};

exp_return check_op(exp_param &param,
    exp_return (*f)(exp_param &, op_t &, exp_return &))
{
	exp_return r;
	r.tok = param.context.get_token();
	if (r.tok->get_type() == symbol::atom) {
		op_t &op = param.context.ops.getop(r.tok->get_text());
		if (!op.null() && op.get_pred() == param.priority)
			return f(param, op, r);
	}
	param.context.push(r.tok);
	r.cont = true;
	r.ok   = false;
	return r;
}

exp_return parse_exp_prefix(exp_param &param, op_t &op, exp_return &r)
{
	optional<p_term> p;
	if (op.noassoc())
		p = parse_exp_next(param.context, param.priority);
	else
		p = parse_exp(param.context, param.priority);
	if (!p)
		throw syntax_error(param.context.get_position(),
				"expression expected.");
	vector<p_term> v;
	v.push_back(move(*p));
	r.exp  = make_unique<term>(move(r.tok), move(v));
	r.ok   = true;
	r.cont = !op.noassoc();
	return move(r);
}

exp_return parse_exp_infix_postfix(exp_param &param, op_t &op, exp_return &r)
{
	optional<p_term> p;
	if (op.infix()) {
		if (op.lassoc())
			p = parse_exp_next(param.context, param.priority);
		else
			p = parse_exp(param.context, param.priority);
		if (!p)
			goto fail;
		r.exp  = move(*p);
		r.ok   = true;
		r.cont = op.lassoc();
		return move(r);
	}
	if (op.postfix()) {
		r.exp = unique_ptr<term>(nullptr);
		r.ok  = true;
		r.cont = !op.noassoc();
		return move(r);
	}
fail:
	throw syntax_error(param.context.get_position(), "expression expected.");
}

optional<p_term> parse_exp(interp_context &context, int priority)
{
	exp_param param {context, priority};
	exp_return r1, r2;
	p_term exp;

	r1 = check_op(param, parse_exp_prefix);
	if (r1.ok) {
		exp = move(r1.exp);
		if (!r1.cont)
			return move(exp);
	} else {
		auto p = parse_exp_next(param.context, param.priority);
		if (!p)
			return nullopt;
		exp = move(*p);
	}

	while (true) {
		exp_param param {context, priority};
		r2 = check_op(param, parse_exp_infix_postfix);
		if (!r2.ok)
			return move(exp);
		vector<p_term> v;
		v.push_back(move(exp));
		if (r2.exp)
			v.push_back(move(r2.exp));
		assert(r2.tok);
		exp = make_unique<term>(move(r2.tok), move(v));
		if (!r2.cont)
			return move(exp);
	}
	return unique_ptr<term>{nullptr};
}

optional<p_term>
parse_expression(interp_context &context)
{
	int low = context.ops.lowest();
	return parse_exp(context, low);
}

optional<p_term> parse_term(interp_context &context)
{
	optional<p_term> r;
	unique_ptr<token> t;
	vector<p_term> rest;

	t = context.get_token();
	if (t->get_type() == symbol::atom) {
		uint64_t id = context.atom_id.get_id(t->get_text());
		t->id = id;
		unique_ptr<token> next = context.get_token();
		if (next->get_type() == symbol::lparen) {
			if ((rest = many(context, parse_expression,
					symbol::comma)).empty())
				throw syntax_error(
				    context.get_position(), "term expected");
			if (context.get_token()->get_type() != symbol::rparen)
				throw syntax_error(
				    context.get_position(), ") expected");
			r = make_unique<term>(move(t), move(rest));
		} else {
			context.push(next);
			r = make_unique<term>(move(t));
		}
	} else if (t->get_type() == symbol::variable) {
		uint64_t id = context.var_id.get_id(t->get_text());
		t->id = id;
		r = make_unique<term>(move(t));
	} else {
		context.push(t);
		r = nullopt;
	}

	return r;
}

ostream&
operator<<(ostream& os, const clause& c)
{
	os << "CLAUSE:" << endl;
	c.head->set_ident(0);
	os << *c.head;
	os << ":-" << endl;
	for (auto &i : c.body) {
		i->set_ident(0);
		os << *i;
	}
	return os;
}

optional<p_clause> parse_clause(interp_context &context)
{
	optional<p_clause> rv;
	optional<p_term> head;
	vector<p_term> body;
	unique_ptr<token> t;

	// start a new scope
	context.var_id.clear();
	// head
	head = parse_expression(context);
	if (!head) {
		rv = nullopt;
	} else {
		if ((*head)->get_first()->get_type() != symbol::atom)
			throw syntax_error(context.get_position(),
					"predicate expected");
		t = expect_period(context);
		if (t->get_type() != symbol::period) {
			if (t->get_type() != symbol::rules)
				throw syntax_error(context.get_position(),
					". or :- expected");
			// body
			body = many(context, parse_expression, symbol::comma);
			if (body.empty())
				throw syntax_error(context.get_position(),
					"rule body expected");
			rv = make_unique<clause>(move(*head), move(body));
			if (expect_period(context)->get_type() != symbol::period)
				throw syntax_error(context.get_position(),
					". expected");
		} else
			rv = make_unique<clause>(move(*head));
	}

	return rv;
}

vector<p_term> parse_query(interp_context &context)
{
	unique_ptr<token> t = context.get_token();
	vector<p_term> goals;

	if (t->get_type() != symbol::query) {
		if (t->get_type() != symbol::atom)
			throw syntax_error(context.get_position(),
				"unexpected character");
		context.push(t);
		return goals;
	}
	// start a new scope
	context.var_id.clear();
	goals = many(context, parse_expression, symbol::comma);
	if (goals.empty())
		throw syntax_error(context.get_position(),
			"at least 1 goal is expected");
	if (expect_period(context)->get_type() != symbol::period)
		throw syntax_error(context.get_position(),
			"missing .");
	return goals;
}

uint64_t find_max_ids(const p_term &t)
{
	uint64_t max_id = 0;
	const unique_ptr<token> &head = t->get_first();
	if (head->get_type() == symbol::variable) {
		if (head->id > max_id)
			max_id = head->id;
	} else {
		for (auto &i : t->get_rest()) {
			uint64_t local = find_max_ids(i);
			if (local > max_id)
				max_id = local;
		}
	}
	return (max_id);
}

// find the number of variables
void scan_vars(const p_term &t, uint64_t base,
		unordered_map<uint64_t, string> &m)
{
	const unique_ptr<token> &head = t->get_first();
	if (head->get_type() == symbol::variable) {
		uint64_t id = head->id + base;
		string s = head->get_text();
		auto i = m.find(id);
		if (i == m.end())
			m.insert(make_pair<uint64_t,string>(move(id), move(s)));
	} else {
		for (auto &i : t->get_rest())
			scan_vars(i, base, m);
	}
}

bool program()
{
	interp_context context {cin};
	optional<p_clause> c;
	vector<p_clause> cs;
	vector<p_term> q;
	bool quit = false;

	while (!quit) {
		try {
			if ((c = parse_clause(context))) {
				cs.push_back(move(*c));
			} else if (!(q = parse_query(context)).empty()) {
				solve(cs, q, context.var_id.max());
			}
			unique_ptr<token> t = context.get_token();
			if (t->get_type() != symbol::eof)
				context.push(t);
			else
				quit = true;
		} catch(syntax_error &e) {
			cerr << e.what() << endl;
		}
	}
	return true;
}
