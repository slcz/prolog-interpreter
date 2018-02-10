#include <iostream>
#include <vector>
#include <optional>
#include "parser.h"
#include "interpreter.h"
#include "unification.h"
using namespace std;

using clause_iter = vector<p_clause>::iterator;
using term_iter = vector<p_term>::iterator;
class runtime {
private:
	clause_iter first_clause, last_clause;
	vector<uint64_t> _variables;
	uint64_t _base;
	bool new_goal_pushed;
public:
	runtime(clause_iter begin, clause_iter end,
	vector<uint64_t> binding_vars, uint64_t off, bool new_goal) :
	first_clause{begin}, last_clause{end},
	_variables{move(binding_vars)}, _base{off}, new_goal_pushed{new_goal} {}

	clause_iter begin() const { return first_clause; }
	clause_iter end()   const { return last_clause; }
	const vector<uint64_t> & variables() const {return _variables; }
	uint64_t base() const { return _base; }
	bool subgoals_pushed() const { return new_goal_pushed; }
};

class goals {
public:
	term_iter first, last;
	goals(term_iter f, term_iter l) : first{f}, last{l} {}
};

optional<pair<clause_iter, vector<uint64_t>>>
try_unification(runtime &r, term_iter p_goal, binding_t &bnd)
{
	/* try unification */
	for (auto cls = r.begin(); cls != r.end(); cls ++) {
		auto u = unification((*cls)->head, *p_goal, 0, r.base(), bnd);
		if (u)
			return make_pair<clause_iter, vector<uint64_t>>(
					move(cls), move(*u));
	}
	return nullopt;
}

bool
build_stack_succ(clause_iter &iter, vector<uint64_t> &bind, runtime &r,
		vector<goals> &gstack, vector<runtime> &rstack)
{
	vector<p_term> &body = (*iter)->body;
	uint64_t max = *max_element(bind.begin(), bind.end());
	bool gstack_pushed;

	gstack.back().first ++;
	if (gstack.back().first == gstack.back().last)
		gstack.pop_back();
	if (!body.empty()) {
		gstack.push_back(goals{ body.begin(), body.end() });
		gstack_pushed = true;
	} else {
		gstack_pushed = false;
	}
	if (gstack.empty()) {
		return true;
	} else {
		rstack.push_back(runtime {iter + 1, r.end(), move(bind),
				max + 1, gstack_pushed});
		return false;
	}
}

bool
run(vector<goals> gstack, vector<runtime> rstack, binding_t binding,
    unordered_map<uint64_t, string> &var_map)
{
	bool found_one = false;
	while (!rstack.empty()) {
		/* pop from rstack, restore environment */
		runtime r = move(rstack.back());
		rstack.pop_back();
		undo_bindings(binding, r.variables());
		/* pop gstack to restore current goal */
		if (r.subgoals_pushed())
			gstack.pop_back();
		assert(!gstack.empty());
		goals &group = gstack.back();
		term_iter p_goal = group.first;
		p_goal --;
		auto u = try_unification(r, p_goal, binding);
		if (u) {
			if (build_stack_succ(u->first, u->second, r, gstack,
						rstack)) {
				print_all(var_map, binding);
			}
			found_one = true;
		}
	}

	return found_one;
}

bool
solve(vector<p_clause> &clauses, vector<p_term> &query,
      uint64_t max_id)
{
	vector<goals>   gstack;
	vector<runtime> rstack;
	binding_t       binding;
	unordered_map<uint64_t, string> var_map;

	assert(!query.empty());
	for (auto &q : query)
		all_variables(q, max_id + 1, var_map);
	gstack.push_back(goals(query.begin() + 1, query.end()));
	rstack.push_back(runtime {clauses.begin(), clauses.end(),
	    vector<uint64_t>(), max_id + 1, false});

	return run(move(gstack), move(rstack), move(binding), var_map);
}
