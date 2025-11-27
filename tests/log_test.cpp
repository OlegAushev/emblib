#ifdef __cpp_constexpr

#include <emb/log.hpp>

namespace emb {
namespace internal {
namespace tests {

enum class FooError { foo1, foo2, foo3 };
enum class BarError { bar1, bar2, bar3 };
enum class BazError { baz1, baz2, baz3 };

constexpr bool test_systemlog() {
  emb::log::basic_logger<16, void*, FooError, BarError, BazError> log;
  log.log(emb::log::level::error, BarError::bar2, 42u);
  assert(log.test(FooError::foo1) == std::nullopt);
  assert(log.test(BarError::bar2) == emb::log::level::error);

  log.notice(FooError::foo1, 3.1416f);
  assert(log.test(FooError::foo1) == emb::log::level::notice);
  assert(log.test(BarError::bar2) == emb::log::level::error);

  log.warning(FooError::foo2);
  assert(log.test(FooError::foo1) == emb::log::level::notice);
  assert(log.test(FooError::foo2) == emb::log::level::warning);
  assert(log.test(BarError::bar2) == emb::log::level::error);

  log.info(BazError::baz3);
  assert(log.test(FooError::foo1) == emb::log::level::notice);
  assert(log.test(FooError::foo2) == emb::log::level::warning);
  assert(log.test(BarError::bar2) == emb::log::level::error);
  assert(log.test(BazError::baz3) == emb::log::level::info);

  auto msg = log.pop_message();
  assert(msg.has_value());
  assert(std::get<BarError>(msg->event) == BarError::bar2);
  assert(std::get<uint32_t>(msg->payload) == 42u);
  assert(log.test(BarError::bar2) == emb::log::level::error);

  msg = log.pop_message();
  assert(msg.has_value());
  assert(std::get<FooError>(msg->event) == FooError::foo1);
  assert(std::get<float>(msg->payload) == 3.1416f);
  assert(log.test(FooError::foo1) == emb::log::level::notice);

  msg = log.pop_message();
  assert(msg.has_value());
  assert(std::get<FooError>(msg->event) == FooError::foo2);
  assert(std::holds_alternative<std::monostate>(msg->payload));
  assert(log.test(FooError::foo2) == emb::log::level::warning);

  msg = log.pop_message();
  assert(msg.has_value());
  assert(std::get<BazError>(msg->event) == BazError::baz3);
  assert(std::holds_alternative<std::monostate>(msg->payload));
  assert(log.test(BazError::baz3) == std::nullopt);

  return true;
}

static_assert(test_systemlog());

} // namespace tests
} // namespace internal
} // namespace emb

#endif
