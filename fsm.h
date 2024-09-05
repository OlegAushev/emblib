#pragma once


#if defined(EMBLIB_C28X)
#include <emblib/fsm/fsm_c28x.h>
#endif


#ifdef EMBLIB_ARM
#include <emblib/fsm/fsm_arm.h>

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


// } // namespace fsm


#endif
