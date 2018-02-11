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

using clause_iter = vector<p_clause>::const_iterator;
using term_iter = vector<p_term>::const_iterator;
class node {
private:
	const vector<p_clause> &clauses;
	binding_t        &binding;
	clause_iter      first_clause;
	term_iter        goal;
	term_iter        last_child;
	vector<uint64_t> bound_vars;
	const uint64_t   base;
	uint64_t         children_base;
	uint64_t         &top;
	vector<node>     children;
	void expand(vector<uint64_t>);
public:
	node(const vector<p_clause> &_cls, binding_t &_binding,
	     clause_iter fst, term_iter _goal, uint64_t _base, uint64_t &_top) :
	     clauses{_cls}, binding{_binding}, first_clause{fst}, goal{_goal},
	     base{_base}, top{_top} {}
	node(const vector<p_clause> &_cls, binding_t &_binding,
	     clause_iter fst, term_iter _goal,uint64_t _base,uint64_t &_top,
	     term_iter b, node c):
	node(_cls, _binding, fst, _goal, _base, _top)
	{ last_child = b; children.push_back(move(c)); }
	bool solve();
	bool try_unification();
	optional<unique_ptr<node>> sibling(term_iter end) {
		term_iter n = goal + 1;
		if (n == end)
			return nullopt;
		return make_unique<node>(clauses, binding, clauses.begin(), n,
		       base, top);
	}
};

void node::expand(vector<uint64_t> vars)
{
	bound_vars = move(vars);
	vector<p_term> &body = (*first_clause)->body;
	children_base = top;
	top += (*first_clause)->nvars;

	if (!body.empty()) {
		last_child = body.end();
		node child {clauses, binding, clauses.begin(), body.begin(),
		children_base, top};
		children.push_back(move(child));
	}
	assert(first_clause != clauses.end());
	first_clause ++;
}

bool node::try_unification()
{
	auto &f = first_clause;
	uint64_t t = top;
	/* try unification */
	for (; f != clauses.end(); f ++) {
		assert(*goal);
		assert((*f)->head);
		auto u = unification((*f)->head, *goal, t, base, binding);
		if (u) {
			expand(move(*u));
			return true;
		}
	}
	return false;
}

bool node::solve()
{
	while (true) {
		if (children.empty()) {
			undo_bindings(binding, bound_vars);
			if (!try_unification())
				return false;
			else if (children.empty())
				return true;
		} // fall through

		while (!children.empty()) {
			node &last = children.back();
			if (last.solve()) {
				optional<unique_ptr<node>> next;
				if ((next = last.sibling(last_child)))
					children.push_back(move(**next));
				else
					return true;
			} else
				children.pop_back();
		}
	}
}

bool
solve(const vector<p_clause> &clauses, const vector<p_term> &query, uint64_t max_id)
{
	unordered_map<uint64_t, string> var_map;
	binding_t binding;
	uint64_t id = max_id + 1, top = id, m;
	bool solved = false;

	assert(!query.empty());
	for (auto &q : query) {
		scan_vars(q, id, var_map);
		m = find_max_ids(q);
		if (m > top)
			top = m;
	}
	top = top + id;

	node child {clauses, binding, clauses.begin(), query.begin(), id, top};
	node root  {clauses, binding, clauses.end(),   query.begin(), id, top,
	    query.end(), move(child)};
	while (root.solve()) {
		solved = true;
		print_all(var_map, binding);
		cout << "yes" << endl;
	}
	if (!solved) cout << "no"; else cout << "no-more";
	cout << endl;
	return solved;
}
