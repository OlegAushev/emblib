#include <string_view>
#include <type_traits>

#include <emb/meta/fixed_string.hpp>

namespace {

using namespace emb;

// -- Construction and deduction --

static_assert(std::is_same_v<decltype(fixed_string{"abc"}), fixed_string<4>>);

static_assert(fixed_string{"motor.p"}.size() == 7);
static_assert(fixed_string{"motor.p"}.view() == std::string_view{"motor.p"});
static_assert(fixed_string{""}.size() == 0);
static_assert(fixed_string{""}.empty());
static_assert(!fixed_string{"a"}.empty());

// data() is NUL-terminated.
static_assert(std::string_view{fixed_string{"abc"}.data()} == "abc");

// A default-constructed string is all NULs; its size is the capacity, so
// only literal-built strings carry a meaningful size.
static_assert(fixed_string<4>{}.chars[0] == '\0');

// -- Non-type template parameter --

template<fixed_string Name>
consteval auto name_size()
{
  return Name.size();
}

static_assert(name_size<"drive.runout_speed">() == 18);

template<fixed_string Name>
struct tag {};

// Equal strings name the same template parameter object, different ones do
// not — the property the whole by-name lookup rests on.
static_assert(std::is_same_v<tag<"a">, tag<"a">>);
static_assert(!std::is_same_v<tag<"a">, tag<"b">>);

// -- Concatenation --

static_assert(
    std::is_same_v<decltype(fixed_string{"ab"} + fixed_string{"cd"}),
                   fixed_string<5>>);

static_assert((fixed_string{"ab"} + fixed_string{"cd"}).size() == 4);
static_assert((fixed_string{"ab"} + fixed_string{"cd"}).view() == "abcd");
static_assert(("ab" + fixed_string{"cd"}).view() == "abcd");
static_assert((fixed_string{"ab"} + "cd").view() == "abcd");
static_assert((fixed_string{""} + fixed_string{"cd"}).view() == "cd");
static_assert((fixed_string{"ab"} + fixed_string{""}).view() == "ab");

// The terminating NUL survives concatenation.
static_assert(std::string_view{(fixed_string{"ab"} + "cd").data()} == "abcd");

static_assert(
    ("unknown parameter '" + fixed_string{"motor.p"} + "'").view()
    == "unknown parameter 'motor.p'");

// -- Use as a static_assert message --

template<fixed_string Name>
consteval bool check_name()
{
  static_assert(!Name.empty(), "empty name: " + Name);
  return true;
}

static_assert(check_name<"motor.p">());

} // namespace
