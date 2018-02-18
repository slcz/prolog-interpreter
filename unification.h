#pragma once
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>
#include <sstream>
#include "parser.h"

namespace {
	using std::unordered_map;
	using std::shared_ptr;
	using std::vector;
	using std::optional;
	using std::ostream;
	using std::stringstream;
	using std::pair;
	using std::nullopt;
	using std::function;
}

enum class control { none, fail, cut };

using maybe_ids = optional<vector<uint64_t>>;
class bind_value;
using p_bind_value = shared_ptr<bind_value>;
using var_lookup = unordered_map<uint64_t, p_bind_value>;
using builtin_t = pair<control, vector<uint64_t>>;

class bind_value {
public:
	virtual optional<builtin_t> builtin(uint64_t, var_lookup &, struct env &)
	{ return nullopt; }
	virtual string tostring(const var_lookup &) = 0;
	virtual bool loop(uint64_t id, const var_lookup &) { return false; }
	virtual maybe_ids unification(p_bind_value &, var_lookup &, bool) = 0;
	virtual const p_bind_value &walk(const p_bind_value &fail,
	const var_lookup &table) { return fail; }
	virtual optional<int> getint(var_lookup &) { return nullopt; }
	virtual optional<float> getdecimal(var_lookup&) { return nullopt; }
	virtual optional<string> getname(const var_lookup &) { return nullopt; }
	virtual optional<string> atom2chars(const var_lookup &) {return nullopt; }
	virtual optional<string> list2string(const var_lookup &) { return nullopt;}
};

class composite_t : public bind_value {
private:
	bool loop(uint64_t, const var_lookup &) override;
protected:
	const p_term &root;
	uint64_t base;
public:
	string tostring(const var_lookup &) override;
	composite_t(const p_term &t, uint64_t b) : root {t}, base {b} {}
	uint64_t get_base() { return base; }
	const p_term &get_root() const { return root; }
	maybe_ids unification(p_bind_value &, var_lookup &, bool) override;
	virtual uint64_t get_id() { return root->get_first()->id; }
	optional<builtin_t> builtin(uint64_t, var_lookup &, struct env&)override;
	optional<int> getint(var_lookup &t) override;
	optional<float> getdecimal(var_lookup&t) override;
	optional<string> list2string(const var_lookup &) override;
	optional<string> getname(const var_lookup &) override {
		return root->get_first()->get_text(); }
	optional<string> atom2chars(const var_lookup &) override;
};

class variable_t : public composite_t {
private:
	bool loop(uint64_t id, const var_lookup &) override
	{ return id == get_id(); }
public:
	string tostring(const var_lookup &m) override;
	maybe_ids unification(p_bind_value &, var_lookup &, bool) override;
	uint64_t get_id() override { return composite_t::get_id() + base; }
	const p_bind_value &walk(const p_bind_value &fail,
	     const var_lookup & table) override;
	variable_t(const p_term &t, uint64_t b) : composite_t{t,b} {}
	optional<int> getint(var_lookup &t) override;
	optional<float>getdecimal(var_lookup &t)override;
};

template <typename T>
class primitive_t : public bind_value {
private:
	const T value;
public:
	string tostring(const var_lookup &) override
	{ stringstream os; os << value; return os.str(); }
	primitive_t(const T v) : value {v} {}
	maybe_ids unification(p_bind_value &, var_lookup &, bool) override;
	const T get_value() { return value; }
	optional<int> getint(var_lookup &) override
	{ if (typeid(int) == typeid(T)) return value; return nullopt;}
	optional<float> getdecimal(var_lookup &) override
	{ if (typeid(float) == typeid(T)) return value; return nullopt; }
};

class bind_env {
private:
	const p_bind_value &node;
	const var_lookup &map;
public:
	bind_env(const p_bind_value &n, const var_lookup &m) : node {n}, map {m} {}
	friend ostream &operator<<(ostream &, const bind_env &);
};

maybe_ids unification(const p_term &, const p_term &, uint64_t,
    uint64_t, var_lookup &, bool compare_only = false);
void remove_from_table(var_lookup &, const vector<uint64_t> &);
p_bind_value create_bind_value(const p_term &, uint64_t, const var_lookup &);
optional<builtin_t> builtin(const p_term &, uint64_t, var_lookup &, struct env&);
