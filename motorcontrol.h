#pragma once

#include <emblib/array.h>
#include <emblib/core.h>
#include <emblib/math.h>
#include <emblib/units.h>

#if defined(EMBLIB_C28X)
#include <motorcontrol/clarke.h>
#include <motorcontrol/ipark.h>
#include <motorcontrol/math.h>
#include <motorcontrol/park.h>
#elif defined(EMBLIB_ARM)
#include <utility>
#endif

namespace emb {

#if defined(EMBLIB_C28X)
// clang-format off
SCOPED_ENUM_UT_DECLARE_BEGIN(phase3, uint32_t) {
    a,
    b,
    c
} SCOPED_ENUM_DECLARE_END(phase3);
// clang-format on
#elif defined(EMBLIB_ARM)
enum class phase3 : uint32_t { a, b, c };
#endif

class motor_speed {
private:
    int _p;
    float _w;
public:
    explicit motor_speed(int p) : _p(p), _w(0) {}
    motor_speed(int p, float w, units::impl::radps_t unit_tag) : _p(p) {
        set(w, unit_tag);
    }
    motor_speed(int p, float n, units::impl::rpm_t unit_tag) : _p(p) {
        set(n, unit_tag);
    }

    int p() const { return _p; }

    float get(units::impl::radps_t unit_tag) const { return _w; }
    float get(units::impl::rpm_t unit_tag) const {
        return 60.f * _w / (numbers::two_pi * float(_p));
    }

    void set(float w, units::impl::radps_t unit_tag) { _w = w; }
    void set(float n, units::impl::rpm_t unit_tag) {
        _w = numbers::two_pi * float(_p) * n / 60.f;
    }
};

inline motor_speed operator*(const motor_speed& lhs, float rhs) {
    return motor_speed(lhs.p(),
                       lhs.get(emb::units::radps) * rhs,
                       emb::units::radps);
}

inline motor_speed operator*(float lhs, const motor_speed& rhs) {
    return rhs * lhs;
}

inline motor_speed operator/(const motor_speed& lhs, float rhs) {
    return motor_speed(lhs.p(),
                       lhs.get(emb::units::radps) / rhs,
                       emb::units::radps);
}

class motor_angle {
private:
    int _p;
    float _rad;
public:
    explicit motor_angle(int p) : _p(p), _rad(0) {}

    motor_angle(int p, float v, units::impl::elec_rad_t unit_tag) : _p(p) {
        set(v, unit_tag);
    }
    motor_angle(int p, float v, units::impl::mech_rad_t unit_tag) : _p(p) {
        set(v, unit_tag);
    }
    motor_angle(int p, float v, units::impl::elec_deg_t unit_tag) : _p(p) {
        set(v, unit_tag);
    }
    motor_angle(int p, float v, units::impl::mech_deg_t unit_tag) : _p(p) {
        set(v, unit_tag);
    }

    int p() const { return _p; }

    float get(units::impl::elec_rad_t unit_tag) const { return _rad; }
    float get(units::impl::mech_rad_t unit_tag) const {
        return _rad / float(_p);
    }
    float get(units::impl::elec_deg_t unit_tag) const { return to_deg(_rad); }
    float get(units::impl::mech_deg_t unit_tag) const {
        return to_deg(_rad) / float(_p);
    }

    void set(float v, units::impl::elec_rad_t unit_tag) { _rad = v; }
    void set(float v, units::impl::mech_rad_t unit_tag) {
        _rad = v * float(_p);
    }
    void set(float v, units::impl::elec_deg_t unit_tag) { _rad = to_rad(v); }
    void set(float v, units::impl::mech_deg_t unit_tag) {
        _rad = to_rad(v) * float(_p);
    }
};

inline float to_radps(float speed_rpm, int pole_pairs) {
    return numbers::two_pi * float(pole_pairs) * speed_rpm / 60.f;
}

inline float to_radps(float speed_rpm) {
    return numbers::two_pi * speed_rpm / 60.f;
}

inline float to_rpm(float speed_radps, int pole_pairs) {
    return 60.f * speed_radps / (numbers::two_pi * float(pole_pairs));
}

struct vec_alpha {
    float mag;
    float theta;
};
struct vec_alphabeta {
    float alpha;
    float beta;
};
struct vec_dq {
    float d;
    float q;
};

inline vec_dq park_transform(vec_alphabeta v, float sine, float cosine) {
    vec_dq retv;
    retv.d = (v.alpha * cosine) + (v.beta * sine);
    retv.q = (v.beta * cosine) - (v.alpha * sine);
    return retv;
}

inline vec_alphabeta invpark_transform(vec_dq v, float sine, float cosine) {
    vec_alphabeta retv;
    retv.alpha = (v.d * cosine) - (v.q * sine);
    retv.beta = (v.q * cosine) + (v.d * sine);
    return retv;
}

inline vec_alphabeta clarke_transform(float a, float b, float c) {
    vec_alphabeta retv;
    retv.alpha = a;
    retv.beta = (b - c) * numbers::inv_sqrt3;
    return retv;
}

inline vec_alphabeta clarke_transform(const emb::array<float, 3>& v) {
    vec_alphabeta retv;
    retv.alpha = v[0];
    retv.beta = (v[1] - v[2]) * numbers::inv_sqrt3;
    return retv;
}

inline vec_alphabeta clarke_transform(float a, float b) {
    vec_alphabeta retv;
    retv.alpha = a;
    retv.beta = (a + 2 * b) * numbers::inv_sqrt3;
    return retv;
}

inline emb::array<float, 3> invclarke_transform(vec_alphabeta v) {
    emb::array<float, 3> retv;
    retv[0] = v.alpha;
    retv[1] = (-v.alpha + emb::numbers::sqrt_3 * v.beta) * 0.5f;
    retv[2] = (-v.alpha - emb::numbers::sqrt_3 * v.beta) * 0.5f;
    return retv;
}

inline emb::array<emb::unsigned_perunit, 3> calculate_sinpwm(vec_alphabeta v_s,
                                                             float v_dc) {
    emb::array<float, 3> voltages = invclarke_transform(v_s);
    const float voltage_base = v_dc / 1.5f;
    emb::array<emb::unsigned_perunit, 3> duty_cycles;

    for (size_t i = 0; i < 3; ++i) {
        duty_cycles[i].set(voltages[i] / voltage_base);
    }

    return duty_cycles;
}

inline emb::array<emb::unsigned_perunit, 3> calculate_svpwm(vec_alpha v_s,
                                                            float v_dc) {
    v_s.theta = rem_2pi(v_s.theta);
    v_s.mag = clamp<float>(v_s.mag, 0, v_dc / numbers::sqrt_3);

    int32_t sector = static_cast<int32_t>(v_s.theta / numbers::pi_over_3);
    float theta = v_s.theta - float(sector) * numbers::pi_over_3;

    // base vector times calculation
#if defined(EMBLIB_C28X)
    float tb1 =
        numbers::sqrt_3 * (v_s.mag / v_dc) * sinf(numbers::pi_over_3 - theta);
    float tb2 = numbers::sqrt_3 * (v_s.mag / v_dc) * sinf(theta);
#elif defined(EMBLIB_ARM)
    float tb1 = numbers::sqrt_3 * (v_s.mag / v_dc) *
                arm_sin_f32(numbers::pi_over_3 - theta);
    float tb2 = numbers::sqrt_3 * (v_s.mag / v_dc) * arm_sin_f32(theta);
#endif
    float tb0 = (1.f - tb1 - tb2) / 2.f;

    emb::array<float, 3> pulse_durations;
    switch (sector) {
    case 0:
        pulse_durations[0] = tb1 + tb2 + tb0;
        pulse_durations[1] = tb2 + tb0;
        pulse_durations[2] = tb0;
        break;
    case 1:
        pulse_durations[0] = tb1 + tb0;
        pulse_durations[1] = tb1 + tb2 + tb0;
        pulse_durations[2] = tb0;
        break;
    case 2:
        pulse_durations[0] = tb0;
        pulse_durations[1] = tb1 + tb2 + tb0;
        pulse_durations[2] = tb2 + tb0;
        break;
    case 3:
        pulse_durations[0] = tb0;
        pulse_durations[1] = tb1 + tb0;
        pulse_durations[2] = tb1 + tb2 + tb0;
        break;
    case 4:
        pulse_durations[0] = tb2 + tb0;
        pulse_durations[1] = tb0;
        pulse_durations[2] = tb1 + tb2 + tb0;
        break;
    case 5:
        pulse_durations[0] = tb1 + tb2 + tb0;
        pulse_durations[1] = tb0;
        pulse_durations[2] = tb1 + tb0;
        break;
    default:
        break;
    }

    emb::array<emb::unsigned_perunit, 3> duty_cycles;
    for (size_t i = 0; i < 3; ++i) {
        duty_cycles[i] = emb::unsigned_perunit(pulse_durations[i]);
    }

    return duty_cycles;
}

inline emb::array<unsigned_perunit, 3>
compensate_deadtime_v1(const emb::array<unsigned_perunit, 3>& dutycycles,
                       const emb::array<float, 3>& currents,
                       float current_threshold,
                       float pwm_period,
                       float deadtime) {
    emb::array<unsigned_perunit, 3> dc;
    const float deadtime_dutycycle = deadtime / pwm_period;

    for (size_t i = 0; i < 3; ++i) {
        if (currents[i] > current_threshold) {
            dc[i].set(dutycycles[i].get() + deadtime_dutycycle);
        } else if (currents[i] < -current_threshold) {
            dc[i].set(dutycycles[i].get() - deadtime_dutycycle);
        } else {
            dc[i] = dutycycles[i];
        }
    }

    return dc;
}

/// @brief DOI: 10.4028/www.scientific.net/AMM.416-417.536
inline emb::array<unsigned_perunit, 3>
compensate_deadtime_v2(const emb::array<unsigned_perunit, 3>& dutycycles,
                       const emb::array<float, 3>& currents,
                       float current_threshold,
                       float pwm_period,
                       float deadtime) {
#ifdef EMBLIB_C28X
    return dutycycles;
#else
    auto dc = dutycycles;
    const float deadtime_dutycycle = deadtime / pwm_period;

    const auto [min, max] =
        std::minmax_element(currents.begin(), currents.end());

    // use Kirchhoff's current law to determine if there is one positive or one negative current
    if (*min + *max > 0) {
        const auto idx = std::distance(currents.begin(), max);
        dc[size_t(idx)].set(dc[size_t(idx)].get() + 2 * deadtime_dutycycle);
    } else if (*min + *max < 0) {
        const auto idx = std::distance(currents.begin(), min);
        dc[size_t(idx)].set(dc[size_t(idx)].get() - 2 * deadtime_dutycycle);
    }

    return dc;
#endif
}

} // namespace emb
