#pragma once
#include <optional>
#include <vector>
#include "unification.h"

namespace {
	using std::vector;
	using std::optional;
	using std::get;
	using std::holds_alternative;
	using std::nullopt;
}

bind_value build_target(const p_term &, uint64_t, var_lookup &);
optional<vector<uint64_t>>unification_sub(bind_value, bind_value,var_lookup &, bool compare_only = false);

static inline optional<int> get_int(const bind_value b)
{
	if (holds_alternative<int>(b))
		return get<int>(b);
	return nullopt;
}

static inline optional<float> get_decimal(const bind_value b)
{
	if (holds_alternative<float>(b))
		return get<float>(b);
	return nullopt;
}

static inline optional<p_structure> get_structure(const bind_value b)
{
	if (holds_alternative<p_structure>(b))
		return get<p_structure>(b);
	return nullopt;
}

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
