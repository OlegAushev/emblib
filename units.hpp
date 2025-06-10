#pragma once

#include <emblib/core.hpp>
#include <emblib/math.hpp>

namespace emb {
namespace units {

template<typename T, typename Unit>
class named_unit {
private:
    T v_;
public:
    named_unit() : v_(T(0)) {}
    explicit named_unit(const T& v) : v_(v) {}
    const T& numval() const { return v_; }

    named_unit& operator+=(const named_unit& rhs) {
        v_ += rhs.v_;
        return *this;
    }

    named_unit& operator-=(const named_unit& rhs) {
        v_ -= rhs.v_;
        return *this;
    }
};

template<typename T, typename Unit>
inline bool operator==(const named_unit<T, Unit>& lhs,
                       const named_unit<T, Unit>& rhs) {
    return lhs.numval() == rhs.numval();
}

template<typename T, typename Unit>
inline bool operator!=(const named_unit<T, Unit>& lhs,
                       const named_unit<T, Unit>& rhs) {
    return lhs.numval() != rhs.numval();
}

template<typename T, typename Unit>
inline bool operator<(const named_unit<T, Unit>& lhs,
                      const named_unit<T, Unit>& rhs) {
    return lhs.numval() < rhs.numval();
}

template<typename T, typename Unit>
inline bool operator>(const named_unit<T, Unit>& lhs,
                      const named_unit<T, Unit>& rhs) {
    return lhs.numval() > rhs.numval();
}

template<typename T, typename Unit>
inline bool operator<=(const named_unit<T, Unit>& lhs,
                       const named_unit<T, Unit>& rhs) {
    return lhs.numval() <= rhs.numval();
}

template<typename T, typename Unit>
inline bool operator>=(const named_unit<T, Unit>& lhs,
                       const named_unit<T, Unit>& rhs) {
    return lhs.numval() >= rhs.numval();
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
    return named_unit<T, Unit>(lhs.numval() * rhs);
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator*(float lhs,
                                     const named_unit<T, Unit>& rhs) {
    return rhs * lhs;
}

template<typename T, typename Unit>
inline named_unit<T, Unit> operator/(const named_unit<T, Unit>& lhs,
                                     float rhs) {
    return named_unit<T, Unit>(lhs.numval() / rhs);
}

namespace impl {
// speed
struct rpm {};
struct eradps {};
// angle
struct erad {};
struct edeg {};
struct rad {};
struct deg {};
} // namespace impl

typedef named_unit<float, impl::rpm> rpm_t;
typedef named_unit<float, impl::eradps> eradps_t;

typedef named_unit<float, impl::erad> erad_t;
typedef named_unit<float, impl::edeg> edeg_t;
typedef named_unit<float, impl::rad> rad_t;
typedef named_unit<float, impl::deg> deg_t;

class eangle_t {
private:
    units::erad_t erad_;
public:
    eangle_t() : erad_(0) {}

    eangle_t(units::erad_t v) { set(v); }

    eangle_t(units::edeg_t v) { set(v); }

    template<typename Unit>
    eangle_t& operator=(Unit v) {
        set(v);
        return *this;
    }

    units::erad_t erad() const { return erad_; }

    units::edeg_t edeg() const { return units::edeg_t(to_deg(erad_.numval())); }
private:
    void set(units::erad_t v) { erad_ = v; }

    void set(units::edeg_t v) { erad_ = units::erad_t(to_rad(v.numval())); }
};

class angle_t {
private:
    units::rad_t rad_;
public:
    angle_t() : rad_(0) {}

    angle_t(units::rad_t v) { set(v); }

    angle_t(units::deg_t v) { set(v); }

    template<typename Unit>
    angle_t& operator=(Unit v) {
        set(v);
        return *this;
    }

    units::rad_t rad() const { return rad_; }

    units::deg_t deg() const { return units::deg_t(to_deg(rad_.numval())); }
private:
    void set(units::rad_t v) { rad_ = v; }

    void set(units::deg_t v) { rad_ = units::rad_t(to_rad(v.numval())); }
};

} // namespace units
} // namespace emb

template<typename T, typename Unit>
inline emb::units::named_unit<T, Unit>
abs(const emb::units::named_unit<T, Unit>& v) {
    return emb::units::named_unit<T, Unit>(abs(v.numval()));
}
