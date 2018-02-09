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

optional<p_term> parse_term(interp_context &context)
{
	optional<p_term> r;
	unique_ptr<token> t;
	optional<vector<p_term>> rest;

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

optional<p_clause> parse_clause(interp_context &context)
{
	optional<p_clause> rv;
	optional<p_term> head;
	optional<vector<p_term>> body;
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

optional<vector<p_term>> parse_query(interp_context &context)
{
	unique_ptr<token> t = context.get_token();
	optional<vector<p_term>> goals;

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

bool program(interp_context &context)
{
	optional<p_clause> c;
	vector<p_clause> cs;
	optional<vector<p_term>> q;
	bool quit = false;

	while (!quit) {
		try {
			if ((c = parse_clause(context))) {
				cs.push_back(move(*c));
			} else if ((q = parse_query(context))) {
				solve(cs, *q);
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
