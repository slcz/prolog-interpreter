#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <optional>
#include "parser.h"

namespace {
	using std::unordered_map;
	using std::unique_ptr;
	using std::optional;
}

class bind_value {
private:
	uint64_t offset;
	p_term &root;
	uint64_t id() const { return root->get_first()->id; }
public:
	bind_value(p_term &t, uint64_t o): offset {o}, root {t} {}
	symbol get_type() const { return root->get_first()->get_type(); }
	uint64_t get_id() const { return get_type() == symbol::variable ?
		offset + id() : id(); }
	uint64_t get_base() const { return offset; }
	p_term &get_root() const { return root; }
};
using binding_t = unordered_map<uint64_t, unique_ptr<bind_value>>;
using p_bind_value = unique_ptr<bind_value>;

optional<vector<uint64_t>>
unification(p_term &, p_term &, uint64_t, uint64_t, binding_t &);
void undo_bindings(binding_t &, const vector<uint64_t> &);
void all_variables(p_term &, uint64_t, unordered_map<uint64_t, string> &);
void print_all(unordered_map<uint64_t, string> &, binding_t &);
