#pragma once


#include <emblib/core.h>
#include <emblib/math.h>
#include <emblib/units.h>

#if defined(EMBLIB_C28X)
#include <emblib/array.h>
#include <motorcontrol/math.h>
#include <motorcontrol/clarke.h>
#include <motorcontrol/park.h>
#include <motorcontrol/ipark.h>
#elif defined(EMBLIB_ARM)
#include <array>
#include <utility>
#endif


namespace emb {


#if defined(EMBLIB_C28X)
SCOPED_ENUM_UT_DECLARE_BEGIN(phase3, uint32_t) {
    a,
    b,
    c
} SCOPED_ENUM_DECLARE_END(phase3)
#elif defined(EMBLIB_ARM)
enum class phase3 : uint32_t {
    a,
    b,
    c
};
#endif


template<typename T>
struct vec3 {
#if defined(EMBLIB_C28X)
    emb::array<T, 3> vec;
#elif defined(EMBLIB_ARM)
    std::array<T, 3> vec;
#endif

    T& a() { return vec[0]; }
    T& b() { return vec[1]; }
    T& c() { return vec[2]; }

    const T& a() const { return vec[0]; }
    const T& b() const { return vec[1]; }
    const T& c() const { return vec[2]; }
    
    T& operator[](size_t pos) { return vec[pos]; }
    const T& operator[](size_t pos) const { return vec[pos]; }

#if defined(EMBLIB_C28X)
    T& get(phase3 phase) { return vec[phase.underlying_value()]; }
    const T& get(phase3 phase) const { return vec[phase.underlying_value()]; }
#elif defined(EMBLIB_ARM)
    T& get(phase3 phase) { return vec[std::to_underlying(phase)]; }
    const T& get(phase3 phase) const { return vec[std::to_underlying(phase)]; }
#endif
};


class motor_speed {
private:
    int _p;
    float _w;
public:
    explicit motor_speed(int p) : _p(p), _w(0) {}
    motor_speed(int p, float w, units::impl::radps_t unit_tag) : _p(p) { set(w, unit_tag); }
    motor_speed(int p, float n, units::impl::rpm_t unit_tag) : _p(p) { set(n, unit_tag); }

    int pole_pairs() const { return _p; }

    float get(units::impl::radps_t unit_tag) const { return _w; }
    float get(units::impl::rpm_t unit_tag) const { return 60.f * _w / (numbers::two_pi * float(_p)); }

    void set(float w, units::impl::radps_t unit_tag) { _w = w; }
    void set(float n, units::impl::rpm_t unit_tag) { _w = numbers::two_pi * float(_p) * n / 60.f; }
};


class motor_angle {
private:
    int _p;
    float _rad;
public:
    explicit motor_angle(int p) : _p(p), _rad(0) {}
    
    motor_angle(int p, float v, units::impl::elec_rad_t unit_tag) : _p(p) { set(v, unit_tag); }
    motor_angle(int p, float v, units::impl::mech_rad_t unit_tag) : _p(p) { set(v, unit_tag); }
    motor_angle(int p, float v, units::impl::elec_deg_t unit_tag) : _p(p) { set(v, unit_tag); }
    motor_angle(int p, float v, units::impl::mech_deg_t unit_tag) : _p(p) { set(v, unit_tag); }

    int pole_pairs() const { return _p; }

    float get(units::impl::elec_rad_t unit_tag) const { return _rad; }
    float get(units::impl::mech_rad_t unit_tag) const { return _rad / float(_p); }
    float get(units::impl::elec_deg_t unit_tag) const { return to_deg(_rad); }
    float get(units::impl::mech_deg_t unit_tag) const { return to_deg(_rad) / float(_p); }

    void set(float v, units::impl::elec_rad_t unit_tag) { _rad = v; }
    void set(float v, units::impl::mech_rad_t unit_tag) { _rad = v * float(_p); }
    void set(float v, units::impl::elec_deg_t unit_tag) { _rad = to_rad(v); }
    void set(float v, units::impl::mech_deg_t unit_tag) { _rad = to_rad(v) * float(_p); }
};


inline float to_radps(float speed_rpm, int pole_pairs) { return numbers::two_pi * float(pole_pairs) * speed_rpm / 60.f; }


inline float to_radps(float speed_rpm) { return numbers::two_pi * speed_rpm / 60.f; }


inline float to_rpm(float speed_radps, int pole_pairs) { return 60.f * speed_radps / (numbers::two_pi * float(pole_pairs)); }


struct dq_pair {
    float d;
    float q;
    dq_pair() {}
    dq_pair(float d_, float q_) : d(d_), q(q_) {}
};


struct alphabeta_pair {
    float alpha;
    float beta;
    alphabeta_pair() {}
    alphabeta_pair(float alpha_, float beta_) : alpha(alpha_), beta(beta_) {}
};


inline dq_pair park_transform(float alpha, float beta, float sine, float cosine) {
    float d = (alpha * cosine) + (beta * sine);
    float q = (beta * cosine) - (alpha * sine);
    return dq_pair(d, q);
}


inline alphabeta_pair invpark_transform(float d, float q, float sine, float cosine) {
    float alpha = (d * cosine) - (q * sine);
    float beta = (q * cosine) + (d * sine);
    return alphabeta_pair(alpha, beta);
}


inline alphabeta_pair clarke_transform(float a, float b, float c) {
    float alpha = a;
    float beta = (b - c) * numbers::inv_sqrt3;
    return alphabeta_pair(alpha, beta);
}


inline alphabeta_pair clarke_transform(const emb::vec3<float>& vec3_) {
    float alpha = vec3_[0];
    float beta = (vec3_[1] - vec3_[2]) * numbers::inv_sqrt3;
    return alphabeta_pair(alpha, beta);
}


inline alphabeta_pair clarke_transform(float a, float b) {
    float alpha = a;
    float beta = (a + 2*b) * numbers::inv_sqrt3;
    return alphabeta_pair(alpha, beta);
}


inline emb::vec3<float> invclarke_transform(float alpha, float beta) {
    emb::vec3<float> ret;
    ret.a() = alpha;
    ret.b() = (-alpha + emb::numbers::sqrt_3 * beta) * 0.5f;
    ret.c() = (-alpha - emb::numbers::sqrt_3 * beta) * 0.5f;
    return ret;
}


inline emb::vec3<emb::unsigned_perunit> calculate_sinpwm(float voltage_alpha, float voltage_beta, float voltage_dc) {
    emb::vec3<float> voltages = invclarke_transform(voltage_alpha, voltage_beta);
    const float voltage_base = voltage_dc / 1.5f;
    emb::vec3<emb::unsigned_perunit> duty_cycles;

    for (size_t i = 0; i < 3; ++i) {
        duty_cycles[i].set(voltages[i] / voltage_base);
    }

    return duty_cycles;
}


inline emb::vec3<emb::unsigned_perunit> calculate_svpwm(float voltage_mag, float voltage_angle, float voltage_dc) {
    voltage_angle = rem_2pi(voltage_angle);
    voltage_mag = clamp<float>(voltage_mag, 0, voltage_dc / numbers::sqrt_3);

    int32_t sector = static_cast<int32_t>(voltage_angle / numbers::pi_over_3);
    float theta = voltage_angle - float(sector) * numbers::pi_over_3;

    // base vector times calculation
#if defined(EMBLIB_C28X)
    float tb1 = numbers::sqrt_3 * (voltage_mag / voltage_dc) * sinf(numbers::pi_over_3 - theta);
    float tb2 = numbers::sqrt_3 * (voltage_mag / voltage_dc) * sinf(theta);
#elif defined(EMBLIB_ARM)
    float tb1 = numbers::sqrt_3 * (voltage_mag / voltage_dc) * arm_sin_f32(numbers::pi_over_3 - theta);
    float tb2 = numbers::sqrt_3 * (voltage_mag / voltage_dc) * arm_sin_f32(theta);
#endif
    float tb0 = (1.f - tb1 - tb2) / 2.f;

    emb::vec3<float> pulse_durations;
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

    emb::vec3<emb::unsigned_perunit> duty_cycles;
    for (size_t i = 0; i < 3; ++i) {
        duty_cycles[i] = emb::unsigned_perunit(pulse_durations[i]);
    }

    return duty_cycles;
}


inline emb::vec3<unsigned_perunit> compensate_deadtime_v1(const emb::vec3<unsigned_perunit>& dutycycles,
                                                          const emb::vec3<float>& currents,
                                                          float current_threshold,
                                                          float pwm_period,
                                                          float deadtime) {
    emb::vec3<unsigned_perunit> dc;
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
inline emb::vec3<unsigned_perunit> compensate_deadtime_v2(const emb::vec3<unsigned_perunit>& dutycycles,
                                                          const emb::vec3<float>& currents,
                                                          float current_threshold,
                                                          float pwm_period,
                                                          float deadtime) {
#ifdef EMBLIB_C28X
    return dutycycles;
#else
    auto dc = dutycycles;
    const float deadtime_dutycycle = deadtime / pwm_period;

    const auto [min, max] = std::minmax_element(currents.vec.begin(), currents.vec.end());

    // use Kirchhoff's current law to determine if there is one positive or one negative current
    if (*min + *max > 0) {
        const auto idx = std::distance(currents.vec.begin(), max);
        dc[size_t(idx)].set(dc[size_t(idx)].get() + 2 * deadtime_dutycycle);
    } else if (*min + *max < 0) {
        const auto idx = std::distance(currents.vec.begin(), min);
        dc[size_t(idx)].set(dc[size_t(idx)].get() - 2 * deadtime_dutycycle);
    }

    return dc;
#endif
}


} // namespace emb










#ifdef OBSOLETE
/**
 * @brief
 */
void CompensatePwm(const ArrayN<float, 3>& phase_currents)
{
    float uznam __attribute__((unused));
    uznam = pwm_compensation.udc - pwm_compensation.uvt + pwm_compensation.uvd;
    float dt2 = pwm_compensation.dt;

    if(phase_currents.data[PHASE_A] > 0){
        pulse_times.data[0] += dt2;
    }else{
        pulse_times.data[0] -= dt2;
    }
    if(phase_currents.data[PHASE_B] > 0){
        pulse_times.data[1] += dt2;
    }else{
        pulse_times.data[1] -= dt2;
    }
    if(phase_currents.data[PHASE_C] > 0){
        pulse_times.data[2] += dt2;
    }else{
        pulse_times.data[2] -= dt2;
    }
    if(pulse_times.data[0] < 0.f){
        switch_times.data[0] = 0.f;
    }else {
        if(pulse_times.data[0] > 1.0f){
            switch_times.data[0] = 1.0f;
        }
    }
    if(pulse_times.data[1] < 0.f){
        pulse_times.data[1] = 0.f;
    }else {
        if(pulse_times.data[1] > 1.0f){
            pulse_times.data[1] = 1.0f;
        }
    }
    if(pulse_times.data[2] < 0.f){
        pulse_times.data[2] = 0.f;
    }else {
        if(pulse_times.data[2] > 1.0f){
            pulse_times.data[2] = 1.0f;
        }
    }
    for(int i = 0; i < 3; i++){
        switch_times.data[i] = (uint32_t)(pulse_times.data[i]*pwm_counter_period_);
    }
}
#endif

