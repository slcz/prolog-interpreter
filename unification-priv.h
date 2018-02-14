#pragma once
bind_value build_target(const p_term &, uint64_t, var_lookup &);
optional<vector<uint64_t>> builtin(const p_term &, uint64_t, var_lookup &);
optional<vector<uint64_t>> unification_sub(bind_value, bind_value, var_lookup &);

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
