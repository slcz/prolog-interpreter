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
	var_lookup       &table;
	clause_iter      first_clause;
	term_iter        goal;
	term_iter        last_child;
	vector<uint64_t> bound_vars;
	const uint64_t   base;
	uint64_t         children_base;
	uint64_t         &top;
	vector<node>     children;
	void expand(vector<uint64_t>);
	control          flags;
	void             do_cut();
public:
	node(const vector<p_clause> &_cls, var_lookup &_table,
	     clause_iter fst, term_iter _goal, uint64_t _base, uint64_t &_top) :
	     clauses{_cls}, table{_table}, first_clause{fst}, goal{_goal},
	     base{_base}, children_base{0}, top{_top}, flags{control::none} {}
	node(const vector<p_clause> &_cls, var_lookup &_table,
	     clause_iter fst, term_iter _goal,uint64_t _base,uint64_t &_top,
	     term_iter b, node c):
	node(_cls, _table, fst, _goal, _base, _top)
	{ last_child = b; children.push_back(move(c)); }
	void stop_backtracking() { first_clause = clauses.end(); }
	bool solve();
	bool try_unification();
	control get_flags() { return flags; }
	optional<unique_ptr<node>> sibling(term_iter end) {
		term_iter n = goal + 1;
		if (n == end)
			return nullopt;
		return make_unique<node>(clauses, table, clauses.begin(), n,
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
		node child {clauses, table, clauses.begin(), body.begin(),
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

	if (first_clause == clauses.end())
		return false;
	auto u = builtin(*goal, base, table);
	if (u) {
		flags = u->first;
		auto u2 = u->second;
		if (!u2.empty())
			bound_vars = move(u2);
		first_clause = clauses.end();
		return true;
	}
	/* try unification */
	for (; f != clauses.end(); f ++) {
		assert(*goal);
		assert((*f)->head);
		auto u = unification((*f)->head, *goal, t, base, table);
		if (u) {
			expand(move(*u));
			return true;
		}
	}
	return false;
}

void node::do_cut()
{
	for (auto &child: children)
		child.stop_backtracking();
	stop_backtracking();
}

bool node::solve()
{
	while (true) {
		if (children.empty()) {
			remove_from_table(table, bound_vars);
			if (!try_unification())
				return false;
			else if (children.empty())
				return true;
		} // fall through

		while (!children.empty()) {
			node &last = children.back();
			if (last.solve()) {
				if (last.flags == control::cut)
					do_cut();
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
	var_lookup table;
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

	node child {clauses, table, clauses.begin(), query.begin(), id, top};
	node root  {clauses, table, clauses.end(),   query.begin(), id, top,
	    query.end(), move(child)};
	while (root.solve()) {
		solved = true;
		print_all(var_map, table);
		cout << "yes" << endl;
	}
	if (!solved) cout << "no"; else cout << "no-more";
	cout << endl;
	return solved;
}
