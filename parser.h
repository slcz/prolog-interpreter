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
#include <unordered_map>
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
	using std::unordered_map;
}

enum class symbol { none, atom, ignore, append, number, string,
                    variable, error, lparen, rparen,
	            eof, query, rules, comma, period };
using position_t = std::pair<uint32_t, uint32_t>;

class token {
private:
	string text;
	symbol token_type;
	position_t position;
	int value;
	bool isint;
public:
	token(symbol type) : token_type { type } {}
	token() : token(symbol::error) {}
	void set_value(int v) { value = v;}
	int  get_value() { return value; }
	bool match(string::const_iterator begin,
			string::const_iterator end,
			const regex &pat, symbol type) {
		std::smatch m;
		if (regex_search(begin, end, m, pat)) {
			token_type = type;
			text = m.begin()->str();
			return true;
		} else {
			token_type = symbol::error;
			return false;
		}
	}
	void set_text(string t) { text = t; }
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
	int ident;
public:
	term(unique_ptr<token> f, vector<unique_ptr<term>> r) :
		first{move(f)}, rest{move(r)} {}
	term(unique_ptr<token> f) : first{move(f)} {}
	term(const term& t) = delete;
	const unique_ptr<token> &get_first() { return first; }
	const auto &get_rest () { return rest;  }
	void set_ident(int idt) { ident = idt;  }
	friend ostream& operator<<(ostream& os, const term& c);
};
using p_term  = unique_ptr<term>;

void scan_vars(const p_term&, uint64_t, unordered_map<uint64_t, string>&);
uint64_t find_max_ids(const p_term&);

class clause {
public:
	p_term head;
	vector<p_term> body;
	unique_id id;
	uint64_t  nvars;
	clause(p_term h, vector<p_term> b) : head{move(h)}, body{move(b)} {
		uint64_t mx = find_max_ids(head);
		for (auto &i : body) {
			uint64_t tmp;
			if ((tmp = find_max_ids(i)) > mx)
				mx = tmp;
		}
		nvars = mx;
	}
	clause(p_term h) : head{move(h)} { nvars = find_max_ids(head); }
	friend ostream& operator<<(ostream& os, const clause& c);
};
using p_clause = unique_ptr<clause>;

bool program();
