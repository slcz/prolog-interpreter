/*******************************************************************************
 * Prolog Interpreter
 ******************************************************************************/

#include <iostream>
#include <vector>
#include <optional>
#include "parser.h"
#include "interpreter.h"
#include "unification.h"
using namespace std;

using clause_iter = vector<p_clause>::iterator;
using term_iter = vector<p_term>::iterator;
class node {
private:
	vector<p_clause> &clauses;
	binding_t        &binding;
	clause_iter      first_clause;
	term_iter        goal;
	term_iter        last_child;
	vector<uint64_t> bound_vars;
	const uint64_t   base;
	uint64_t         top;
	vector<node>     children;
	void expand(vector<uint64_t>);
public:
	node(vector<p_clause> &_cls, binding_t &_binding, clause_iter fst,
	     term_iter _goal, uint64_t off) :
	     clauses{_cls}, binding{_binding}, first_clause{fst}, goal{_goal},
	     base{off}, top{off} {}
	node(vector<p_clause> &_cls, binding_t &_binding, clause_iter fst,
	     term_iter _goal, uint64_t off, term_iter b, node c) :
	node(_cls, _binding, fst, _goal, off)
	{ last_child = b; children.push_back(move(c)); }
	bool solve();
	bool try_unification();
	optional<unique_ptr<node>> sibling(term_iter end) {
		term_iter n = last_child + 1;
		if (n == end)
			return nullopt;
		return make_unique<node>(clauses, binding, clauses.begin(), n, top);
	}
	uint64_t get_top() const { return top; }
};

void node::expand(vector<uint64_t> vars)
{
	uint64_t m = *max_element(vars.begin(), vars.end()) + 1;
	bound_vars = move(vars);
	vector<p_term> &body = (*first_clause)->body;
	if (!body.empty()) {
		last_child = body.end();
		node child { clauses, binding, clauses.begin(), body.begin(),m};
		children.push_back(move(child));
	}
	assert(first_clause != clauses.end());
	first_clause ++;
	top = m;
}

bool node::try_unification()
{
	auto &f = first_clause;
	/* try unification */
	for (; f != clauses.end(); f ++) {
		auto u =unification((*f)->head, *goal, 0, base, binding);
		if (u) {
			expand(move(*u));
			return true;
		}
	}
	return false;
}

bool node::solve()
{
	if (children.empty()) {
		undo_bindings(binding, bound_vars);
		top = base;
		if (!try_unification())
			return false;
		else if (children.empty())
			return true;
	} // fall through

	while (!children.empty()) {
		node &last = children.back();
		if (last.solve()) {
			optional<unique_ptr<node>> next;
			assert(last.get_top() >= top);
			top = last.get_top();
			if ((next = sibling(last_child)))
				children.push_back(move(**next));
			else
				return true;
		} else {
			children.pop_back();
			top = children.empty()? base: children.back().get_top();
		}
	}
	return false;
}

bool
solve(vector<p_clause> &clauses, vector<p_term> &query, uint64_t max_id)
{
	unordered_map<uint64_t, string> var_map;
	binding_t binding;
	uint64_t id = max_id + 1;
	bool solved = false;

	assert(!query.empty());
	for (auto &q : query)
		all_variables(q, id, var_map);

	node child {clauses, binding, clauses.begin(), query.begin(), id};
	node root  {clauses, binding, clauses.end(),   query.begin(), id,
	    query.end(), move(child)};
	while (root.solve()) {
		solved = true;
		print_all(var_map, binding);
	}
	return solved;
}
