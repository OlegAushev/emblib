#pragma once


#include <emblib/core.h>


namespace emb {
namespace units {

template<typename T, typename Unit>
class named_unit {
private:
    T _v;
public:
    named_unit() : _v(T(0)) {}
    explicit named_unit(const T& v) : _v(v) {}
    const T& get() const { return _v; }

    named_unit& operator+=(const named_unit& rhs) {
        _v += rhs._v;
        return *this;
    }

    named_unit& operator-=(const named_unit& rhs) {
        _v -= rhs._v;
        return *this;
    }
};

template<typename T, typename Unit>
inline bool operator<(const named_unit<T, Unit>& lhs,
                      const named_unit<T, Unit>& rhs) {
    return lhs.get() < rhs.get();
}

template<typename T, typename Unit>
inline bool operator>(const named_unit<T, Unit>& lhs,
                      const named_unit<T, Unit>& rhs) {
    return rhs < lhs;
}

template<typename T, typename Unit>
inline bool operator<=(const named_unit<T, Unit>& lhs,
                       const named_unit<T, Unit>& rhs) {
    return !(lhs > rhs);
}

template<typename T, typename Unit>
inline bool operator>=(const named_unit<T, Unit>& lhs,
                       const named_unit<T, Unit>& rhs) {
    return !(lhs < rhs);
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator+(const named_unit<T, Unit>& lhs,
                                     const named_unit<T, Unit>& rhs) {
    named_unit<T, Unit> tmp = lhs;
    return tmp += rhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator-(const named_unit<T, Unit>& lhs,
                                     const named_unit<T, Unit>& rhs) {
    named_unit<T, Unit> tmp = lhs;
    return tmp -= rhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator*(const named_unit<T, Unit>& lhs,
                                     float rhs) {
    return named_unit<T, Unit>(lhs.get() * rhs);
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator*(float lhs,
                                     const named_unit<T, Unit>& rhs) {
    return rhs * lhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator/(const named_unit<T, Unit>& lhs,
                                     float rhs) {
    return named_unit<T, Unit>(lhs.get() / rhs);
}

namespace impl {
struct rpm {};
struct eradps {};
} // namespace impl

typedef named_unit<float, impl::rpm> rpm_t;
typedef named_unit<float, impl::eradps> eradps_t;


namespace impl {
struct rad_t{};
struct deg_t{};
struct elec_rad_t{};
struct mech_rad_t{};
struct elec_deg_t{};
struct mech_deg_t{};
//struct radps_t{};
//struct rpm_t{};
}


const impl::rad_t rad;
const impl::deg_t deg;
const impl::elec_rad_t elec_rad;
const impl::mech_rad_t mech_rad;
const impl::elec_deg_t elec_deg;
const impl::mech_deg_t mech_deg;
//const impl::radps_t radps;
//const impl::rpm_t rpm;


} // namespace units
} // namespace emb
