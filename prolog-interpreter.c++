/*******************************************************************************
 *
 * A Toy Prolog Interpreter.
 * 2018, cubistolabs, inc.
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
	void clear() { magic = 0; id_map.clear(); }
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
	const unique_ptr<token> &get_first() const { return first; }
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

/*
 * unification.
 * unify 2 terms together, source variable ids are added by offset_source.
 * destination variable ids are incrementaed add offset_destination.
 */
class binding_target {
private:
	unique_ptr<term> dummy;
public:
	symbol type;
	uint64_t var_id;
	unique_ptr<term> &root;
	binding_target(uint64_t id): type {symbol::variable}, var_id{id}, root {dummy}
	{}
	binding_target(unique_ptr<term> &t) : type {symbol::atom}, root {t} {}
};
using binding_t = unordered_map<uint64_t, unique_ptr<binding_target>>;

void undo_bindings(binding_t &binding, const vector<uint64_t> &undo_list)
{
	for (auto undo : undo_list)
		assert(binding.erase(undo) == 1);
}

unique_ptr<binding_target> &
walk_variable(unique_ptr<binding_target> &t, uint64_t offset, binding_t &binding)
{
	if (t->type == symbol::atom) {
		return t;
	} else if (t->type == symbol::variable) {
		auto n = binding.find(t->var_id);
		if (n == binding.end())
			return t;
		else {
			assert(n->second);
			return walk_variable(n->second, offset, binding);
		}
	}
	assert(false);
}

optional<vector<uint64_t>> unification(unique_ptr<term> &, unique_ptr<term> &,
    uint64_t, uint64_t, binding_t &);
optional<vector<uint64_t>> unify_rest(
	vector<unique_ptr<term>> &src, vector<unique_ptr<term>> &dst, uint64_t srcoff,
	uint64_t dstoff, binding_t &binding)
{
	auto ss = src.begin(), se = src.end();
	auto ds = dst.begin(), de = dst.end();
	vector<uint64_t> all;
	optional<vector<uint64_t>> r;

	for (;ss != se && ds != de; ss ++, ds ++) {
		if ((r = unification(*ss, *ds, srcoff, dstoff, binding))) {
			if (!r->empty())
				all.insert(all.end(), *r->begin(), *r->end());
		} else
			goto failure;
	}
	if (ss == se && ds == de)
		return move(all);
failure:
	undo_bindings(binding, all);
	return nullopt;
}

bool detect_loop(uint64_t id,
		unique_ptr<binding_target> &t, uint64_t off, binding_t &binding)
{
	if (t->type == symbol::variable)
		return t->var_id != id;
	unique_ptr<term> & pterm = t->root;
	const unique_ptr<token> &head = pterm->get_first();
	unique_ptr<binding_target> tmp;

	if (head->get_type() == symbol::variable) {
		if (head->id + off == id)
			return false;
		tmp = make_unique<binding_target>(head->id);
		unique_ptr<binding_target> &t = walk_variable(tmp, off, binding);
		if (t == tmp)
			return true;
		return detect_loop(id, t, off, binding);
	} else {
		assert(head->get_type() == symbol::atom);
		auto & rest = pterm->get_rest();
		for (auto &i : rest) {
			auto tmp = make_unique<binding_target>(i);
			if (!detect_loop(id, tmp, off, binding))
				return false;
		}
		return true;
	}
	return true;
}

optional<uint64_t> bind(unique_ptr<binding_target> &from,
    unique_ptr<binding_target> to, uint64_t to_off, binding_t &binding)
{
	if (!detect_loop(from->var_id, to, to_off, binding))
		return nullopt;
	assert(to);
	binding.insert(make_pair(from->var_id, move(to)));
	return from->var_id;
}

optional<vector<uint64_t>>
unification_sub(unique_ptr<binding_target> src, unique_ptr<binding_target> dst,
		uint64_t srcoff, uint64_t dstoff, binding_t &binding)
{
	optional<vector<uint64_t>> r;
	vector<uint64_t> all;

	if (src->type == symbol::atom && dst->type == symbol::atom) {
		if (src->root->get_first()->id == dst->root->get_first()->id) {
			if ((r = unify_rest(src->root->get_rest(),
			     dst->root->get_rest(), srcoff, dstoff, binding))) {
				if (!r->empty())
					all.insert(all.end(), *r->begin(), *r->end());
				return all;
			} else
				goto failure;
		}
	} else if (src->type == symbol::variable && dst->type == symbol::atom) {
		optional<uint64_t> key = bind(src, move(dst), dstoff, binding);
		if (key)
			all.push_back(*key);
		else
			goto failure;
	} else if (dst->type == symbol::variable) {
		optional<uint64_t> key = bind(dst, move(src), srcoff, binding);
		if (key)
			all.push_back(*key);
		else
			goto failure;
	} else
		assert(false);
	return all;
failure:
	undo_bindings(binding, all);
	return nullopt;
}

unique_ptr<binding_target>
copy_binding(unique_ptr<binding_target> &src)
{
	if (src->type == symbol::variable)
		return make_unique<binding_target>(src->var_id);
	else
		return make_unique<binding_target>(src->root);
}

optional<vector<uint64_t>>
unification(unique_ptr<term> &src, unique_ptr<term> &dst,
    uint64_t srcoff, uint64_t dstoff, binding_t &binding)
{
	unique_ptr<binding_target> srctgt, dsttgt;
	if (src->get_first()->get_type() == symbol::atom) {
		srctgt = make_unique<binding_target>(src);
	} else {
		srctgt = make_unique<binding_target>(src->get_first()->id + srcoff);
		auto &tmp = walk_variable(srctgt, srcoff, binding);
		srctgt = copy_binding(tmp);
	}
	if (dst->get_first()->get_type() == symbol::atom)
		dsttgt = make_unique<binding_target>(dst);
	else {
		dsttgt = make_unique<binding_target>(dst->get_first()->id + dstoff);
		auto &tmp = walk_variable(dsttgt, dstoff, binding);
		dsttgt = copy_binding(tmp);
	}
	return unification_sub(move(srctgt), move(dsttgt), srcoff, dstoff, binding);
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

void
all_variables(unique_ptr<term> &t, uint64_t offset, unordered_map<uint64_t, string> &m)
{
	const unique_ptr<token> &head = t->get_first();

	if (head->get_type() == symbol::variable) {
		uint64_t id = head->id + offset;
		string s = head->get_text();
		auto i = m.find(id);
		if (i == m.end()) {
			cout << id << ":" << s << endl;
			m.insert(make_pair<uint64_t, string>(move(id), move(s)));
		}
	} else {
		for (auto &i : t->get_rest())
			all_variables(i, offset, m);
	}
}

void
print_term(unique_ptr<term> &t)
{
	const unique_ptr<token> &first = t->get_first();
	vector<unique_ptr<term>> &rest = t->get_rest();
	if (first->get_type() == symbol::atom) {
		cout << first->get_text();
		if (!rest.empty()) {
			cout << "(";
			for (auto &i : rest)
				print_term(i);
			cout << ")";
		}
	} else {
		cout << first->get_text();
	}
}

void
print_all(uint64_t offset, unordered_map<uint64_t, string>v1,
    unordered_map<uint64_t, string>v2, binding_t &binding)
{
	unique_ptr<binding_target> b;

	for (auto &i :v1) {
		cout << i.second << "=>";
		b = make_unique<binding_target>(i.first);
		unique_ptr<binding_target> &n = walk_variable(b, offset, binding);
		if (n->type == symbol::variable) {
			auto m = v1.find(n->var_id);
			if (m == v1.end()) {
				m = v2.find(n->var_id);
				if (m == v2.end())
					cout << n->var_id << endl;
			}
			cout << m->second << endl;
		} else {
			print_term(n->root);
			cout << endl;
		}
	}
}

void
test_unification(interp_context &context)
{
	optional<unique_ptr<term>> term1, term2;
	binding_t binding;
	optional<vector<uint64_t>> binding_list;
	unordered_map<uint64_t, string> v1, v2;

	assert((term1 = parse_term(context)));
	assert((term2 = parse_term(context)));
	if (!(binding_list = unification(*term1, *term2, 0, 0, binding)))
		cout << "unification fails" << endl;
	else {
		cout << "unification succeeds" << endl;
		all_variables(*term1, 0, v1);
		all_variables(*term2, 0, v2);
		cout << "term 1 binding" << endl;
		print_all(0, v1, v2, binding);
		cout << "term 2 binding" << endl;
		print_all(0, v2, v1, binding);
	}
}

bool parse_program(interp_context &context)
{
	test_unification(context);
#if 0
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
#endif

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
