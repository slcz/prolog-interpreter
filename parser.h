#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <regex>
#include <cassert>
#include <cstdint>
#include <vector>
#include <set>
#include "unique-id.h"

namespace {
	using std::istream;
	using std::ostream;
	using std::string;
	using std::vector;
	using std::set;
	using std::unique_ptr;
	using std::regex;
	using std::optional;
}

enum class symbol { none, atom, ignore, append,
                    variable, error, lparen, rparen,
	            eof, query, rules, comma, period };
using position_t = std::pair<uint32_t, uint32_t>;

class token {
private:
	string text;
	symbol token_type;
	position_t position;
public:
	token(symbol type) : token_type { type } {}
	token() : token(symbol::error) {}
	bool match(string::const_iterator begin,
			string::const_iterator end,
			const regex &pat, symbol type) {
		std::smatch m;
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

class term {
private:
	unique_ptr<token> first;
	vector<unique_ptr<term>> rest;
public:
	term(unique_ptr<token> f, vector<unique_ptr<term>> r) :
		first{move(f)}, rest{move(r)} {}
	term(unique_ptr<token> f) : first{move(f)} {}
	term(const term& t) = delete;
	const unique_ptr<token> &get_first() const { return first; }
	auto &get_rest () { return rest;  }
	friend ostream& operator<<(ostream& os, const term& c);
};
using p_term = unique_ptr<term>;

class clause {
public:
	p_term head;
	vector<p_term> body;
	unique_id id;
	clause(p_term h, vector<p_term> b) : head{move(h)}, body{move(b)} {}
	clause(p_term h) : head{move(h)} {}
	friend ostream& operator<<(ostream& os, const clause& c);
};
using p_clause = unique_ptr<clause>;

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
};

bool program(interp_context &);
optional<p_term> parse_term(interp_context &);
