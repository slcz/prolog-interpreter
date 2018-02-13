#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <optional>
#include <variant>
#include "parser.h"

namespace {
	using std::unordered_map;
	using std::shared_ptr;
	using std::optional;
	using std::variant;
}

class structure {
private:
	uint64_t base;
	const p_term &root;
	uint64_t id() const { return root->get_first()->id; }
public:
	structure(const p_term &t, uint64_t b): base {b}, root {t} {}
	symbol get_type() const { return root->get_first()->get_type(); }
	uint64_t get_id() const { return get_type() == symbol::variable ?
		base + id() : id(); }
	uint64_t get_base() const { return base; }
	const p_term &get_root() const { return root; }
};
using p_structure = shared_ptr<structure>;
using bind_value = variant<int, p_structure>;
using var_lookup = unordered_map<uint64_t, bind_value>;

optional<vector<uint64_t>>
unification(const p_term &, const p_term &, uint64_t, uint64_t, var_lookup &);
void remove_from_table(var_lookup &, const vector<uint64_t> &);
void print_all(const unordered_map<uint64_t, string> &, const var_lookup &);

bool execute_is(p_term &);
