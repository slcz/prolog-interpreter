/*******************************************************************************
 *
 * A Toy Prolog Interpreter.
 *
 ******************************************************************************/
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <regex>
#include <cstdint>
#include <cassert>
#include <memory>
#include <set>
#include <optional>
#include <exception>
#include <sstream>
#include <unordered_map>

using namespace std;

enum class symbol {none, atom, ignore, append, variable, error,
	lparen, rparen, eof, query, rules, comma, period};
using position_t = pair<uint32_t, uint32_t>;

class syntax_error : public exception {
private:
	position_t position;
	string message;
public:
	syntax_error(const position_t &p, const string &m) :
		position{p}, message{m} {
		stringstream s;
		s << "<" << position.first << "," << position.second <<
			">: " << "Syntax error: " << message;
		message = s.str();
	}
	const char *what() const throw () {
		return message.c_str();
	}
};

class token {
private:
	string text;
	symbol token_type;
	position_t position;
public:
	token(symbol type) : token_type { type } {}
	token() : token(symbol::error) {}
	bool match(string::const_iterator begin, string::const_iterator end,
			const regex &pat, symbol type) {
		smatch m;
		if (regex_search(begin, end, m, pat)) {
			token_type = type;
			assert(m.size() == 1);
			text = m.begin()->str();
			return true;
		} else {
			token_type = symbol::error;
			return false;
		}
	}
	const string &get_text() const { return text; }
	symbol get_type() const { return token_type; }
	void set_type(const symbol type) { token_type = type; }
	void set_position(const position_t & p) { position = p; }
	const position_t & get_position() const { return position; }
	uint64_t id;
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
	{regex("^[[:lower:]_$][[:alnum:]_$]*"), symbol::atom    },
	{regex("^\\?-"),                        symbol::query   },
	{regex("^:-"),                          symbol::rules   },
	{regex("^[#&*+-./:<=>?@^~]+"),          symbol::atom    },
	{regex(R"(^'(\\.|[^'\\])*')"),          symbol::atom    },
	{regex(R"(^'.*)"),                      symbol::append  },
	{regex("^[[:upper:]][_$[:alnum:]]*"),   symbol::variable},
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

class unique_id {
private:
	unordered_map<string, uint64_t> id_map;
	uint64_t magic;
public:
	unique_id() : magic{0} {}
	void clear() { id_map.clear(); }
	uint64_t get_id(const string &name) {
		auto i = id_map.find(name);
		if (i == id_map.end()) {
			magic ++;
			id_map.insert(make_pair(name, magic));
			return magic;
		} else
			return i->second;
	}
};

class interp_context {
	using transformer_t = unique_ptr<token> (*)(unique_ptr<token>);
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
};

unique_ptr<token> interp_context::_get_token()
{
	unique_ptr<token> next;
	position_t token_position;

	if ((next = pop()) != nullptr)
		return next;

	next = make_unique<token>(symbol::none);
	do {
		if (str.begin() + offset == str.end()) {
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

class term {
private:
	unique_ptr<token> first;
	vector<unique_ptr<term>> rest;
public:
	term(unique_ptr<token> f, vector<unique_ptr<term>> r) :
		first{move(f)}, rest{move(r)} {}
	term(unique_ptr<token> f) : first{move(f)} {}
	term(const term& t) = delete;
	unique_ptr<token> &get_first() { return first; }
	auto &get_rest () { return rest;  }
	friend ostream& operator<<(ostream& os, const term& c);
};

ostream&
operator<<(ostream& os, const term& c)
{
	symbol type = c.first->get_type();
	position_t pos = c.first->get_position();
	string p = type == symbol::atom ? "A" :
	           type == symbol::variable ? "V" : "?";
	os << "<" << p << pos.first << "," << pos.second << ">" <<
	   c.first->get_text() << "." << c.first->id << "</" << p << ">";
	if (!c.rest.empty())
		os << "(";
	for (auto &i : c.rest)
		os << *i;
	if (!c.rest.empty())
		os << ")";
	return os;
}

template<typename T> optional<vector<T>>
many(interp_context &context, optional<T>(*unit)(interp_context &),
		const symbol delimiter = symbol::none, bool non_empty = true)
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
	if (non_empty && vec.empty())
		return nullopt;
	else
		return (vec);
}

optional<unique_ptr<term>> parse_term(interp_context &context)
{
	optional<unique_ptr<term>> r;
	unique_ptr<token> t;
	optional<vector<unique_ptr<term>>> rest;

	t = context.get_token();
	if (t->get_type() == symbol::atom) {
		uint64_t id = context.atom_id.get_id(t->get_text());
		t->id = id;
		unique_ptr<token> next = context.get_token();
		if (next->get_type() == symbol::lparen) {
			if (!(rest = many(context, parse_term, symbol::comma)))
				throw syntax_error(
				    context.get_position(), "term expected");
			if (context.get_token()->get_type() != symbol::rparen)
				throw syntax_error(
				    context.get_position(), ") expected");
			r = make_unique<term>(move(t), move(*rest));
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

class clause {
public:
	unique_ptr<term> head;
	vector<unique_ptr<term>> body;
	unique_id id;
	clause(unique_ptr<term> h, vector<unique_ptr<term>> b) :
		head{move(h)}, body{move(b)} {}
	clause(unique_ptr<term> h) : head{move(h)} {}
	friend ostream& operator<<(ostream& os, const clause& c);
};

ostream&
operator<<(ostream& os, const clause& c)
{
	os << "CLAUSE:" << endl << "HEAD" << endl;
	os << *c.head << endl;
	os << "BODY" << endl;
	for (auto &i : c.body)
		os << *i << endl;
	return os;
}

optional<unique_ptr<clause>> parse_clause(interp_context &context)
{
	optional<unique_ptr<clause>> rv;
	optional<unique_ptr<term>> head;
	optional<vector<unique_ptr<term>>> body;
	unique_ptr<token> t;

	// start a new scope
	context.var_id.clear();
	// head
	head = parse_term(context);
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
			body = many(context, parse_term, symbol::comma);
			if (!body)
				throw syntax_error(context.get_position(),
					"rule body expected");
			rv = make_unique<clause>(move(*head), move(*body));
			if (expect_period(context)->get_type() != symbol::period)
				throw syntax_error(context.get_position(),
					". expected");
		} else
			rv = make_unique<clause>(move(*head));
	}

	return rv;
}

optional<vector<unique_ptr<term>>> parse_query(interp_context &context)
{
	unique_ptr<token> t = context.get_token();
	optional<vector<unique_ptr<term>>> goals;

	if (t->get_type() != symbol::query) {
		context.push(t);
		return nullopt;
	}
	// start a new scope
	context.var_id.clear();
	goals = many(context, parse_term, symbol::comma);
	if (!goals)
		throw syntax_error(context.get_position(),
			"at least 1 goal is expected");
	if (expect_period(context)->get_type() != symbol::period)
		throw syntax_error(context.get_position(),
			"missing .");
	return goals;
}

bool parse_program(interp_context &context)
{
	optional<vector<unique_ptr<clause>>> c;
	optional<vector<vector<unique_ptr<term>>>> q;

	if (!(c = many(context, parse_clause, symbol::none, false)))
		throw syntax_error(context.get_position(),
			"rules parsing error");
	for (auto &i : *c)
		cout << *i << endl;
	if (!(q = many(context, parse_query, symbol::none, false)))
		throw syntax_error(context.get_position(),
			"query parsing error");
	for (auto &i : *q)
		for (auto &j : i)
			cout << *j << endl;
	unique_ptr<token> t = context.get_token();
	if (t->get_type() != symbol::eof)
		throw syntax_error(context.get_position(),
			"end of file expected");

	return true;
}

int main()
{
	interp_context context(cin);
	try {
		parse_program(context);
	} catch(syntax_error &e) {
		cerr << e.what() << endl;
		return 1;
	}
	return 0;
}
