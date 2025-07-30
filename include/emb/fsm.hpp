#pragma once

#if __cplusplus < 201100
#include "../../src/fsm/fsm_cpp03.hpp"
#elif __cplusplus >= 202300
#include "../../src/fsm/fsm_cpp23.hpp"
#endif

// #include <optional>
// #include <variant>

// namespace emb {

// template<typename FsmType, typename... States>
// class fsm {
// private:
//     using state_variant_t = std::variant<States...>;
//     state_variant_t _state;
// public:
//     template<typename Event>
//     void dispatch(Event&& event) {
//         auto& child = static_cast<FsmType&>(*this);
//         const auto& visitor = [&](auto& state) -> std::optional<state_variant_t> {
//             return child.on_event(state, std::forward(event));
//         };
//         const auto& next_state = std::visit(visitor, _state);
//         if (next_state) {
//             _state = *std::move(next_state);
//         }
//     }
// };

// } // namespace emb
