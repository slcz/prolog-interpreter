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

class bind_value {
private:
	uint64_t offset;
	const p_term &root;
	uint64_t id() const { return root->get_first()->id; }
public:
	bind_value(const p_term &t, uint64_t o): offset {o}, root {t} {}
	symbol get_type() const { return root->get_first()->get_type(); }
	uint64_t get_id() const { return get_type() == symbol::variable ?
		offset + id() : id(); }
	uint64_t get_base() const { return offset; }
	const p_term &get_root() const { return root; }
};
using p_bind_value = shared_ptr<bind_value>;
using bind_t = variant<int, p_bind_value>;
using binding_t = unordered_map<uint64_t, bind_t>;

optional<vector<uint64_t>>
unification(const p_term &, const p_term &, uint64_t, uint64_t, binding_t &);
void undo_bindings(binding_t &, const vector<uint64_t> &);
void print_all(const unordered_map<uint64_t, string> &, const binding_t &);
