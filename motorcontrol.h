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


#if defined(EMBLIB_C28X)
typedef emb::array<float, 3> vec3;
#elif defined(EMBLIB_ARM)
typedef std::array<float, 3> vec3;
#endif


class motor_speed {
private:
    const int _p;
    float _w;
public:
    explicit motor_speed(int p) : _p(p), _w(0) {}
    motor_speed(int p, units::radps<units::angle_type::electrical> w) : _p(p) { set(w); }
    motor_speed(int p, units::rpm n) : _p(p) { set(n); }

    int pole_pairs() const { return _p; }
    float radps() const { return _w; }
    float rpm() const { return 60 * _w / (numbers::two_pi * float(_p)); }
    float radps_mech() const { return _w / float(_p); }

    void set(units::radps<units::angle_type::electrical> w) { _w = w.get(); }
    void set(units::rpm n) { _w = numbers::two_pi * float(_p) * n.get() / 60; }
};


class motor_angle {
private:
    const int _p;
    float _rad;
public:
    explicit motor_angle(int p) : _p(p), _rad(0) {}
    
    motor_angle(int p, units::rad<units::angle_type::electrical> rad_elec) : _p(p) { set(rad_elec); }
    motor_angle(int p, units::rad<units::angle_type::mechanical> rad_mech) : _p(p) { set(rad_mech); }
    
    motor_angle(int p, units::deg<units::angle_type::electrical> deg_elec) : _p(p) { set(deg_elec); }
    motor_angle(int p, units::deg<units::angle_type::mechanical> deg_mech) : _p(p) { set(deg_mech); }

    int pole_pairs() const { return _p; }
    float rad() const { return _rad; }
    float rad_mech() const { return _rad / float(_p); }
    float deg() const { return to_deg(_rad); }
    float deg_mech() const { return to_deg(_rad) / float(_p); }

    void set(units::rad<units::angle_type::electrical> rad_elec) { _rad = rad_elec.get(); }
    void set(units::rad<units::angle_type::mechanical> rad_mech) { _rad = rad_mech.get() * float(_p); }
    
    void set(units::deg<units::angle_type::electrical> deg_elec) { _rad = to_rad(deg_elec.get()); }
    void set(units::deg<units::angle_type::mechanical> deg_mech) { _rad = to_rad(deg_mech.get()) * float(_p); }
};


inline float to_radps(float speed_rpm, int pole_pairs) { return numbers::two_pi * float(pole_pairs) * speed_rpm / 60.f; }


inline float to_radps(float speed_rpm) { return numbers::two_pi * speed_rpm / 60.f; }


inline float to_rpm(float speed_radps, int pole_pairs) { return 60.f * speed_radps / (numbers::two_pi * float(pole_pairs)); }


inline emb::vec3 calculate_svpwm(float voltage_mag, float voltage_angle, float voltage_dc) {
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

    emb::vec3 pulse_durations;
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

    for (size_t i = 0; i < 3; ++i) {
        pulse_durations[i] = emb::clamp<float>(pulse_durations[i], 0.0f, 1.0f);
    }
    return pulse_durations;
}


inline emb::vec3 compensate_deadtime_v1(const emb::vec3& dutycycles, const emb::vec3& currents,
                                        float current_threshold, float pwm_period, float deadtime) {
    emb::vec3 dc;
    const float deadtime_dutycycle = deadtime / pwm_period;

    for (size_t i = 0; i < 3; ++i) {
        if (currents[i] > current_threshold) {
            dc[i] = dutycycles[i] + deadtime_dutycycle;
        } else if (currents[i] < -current_threshold) {
            dc[i] = dutycycles[i] - deadtime_dutycycle;
        } else {
            dc[i] = dutycycles[i];
        }
        dc[i] = emb::clamp(dc[i], 0.0f, 1.0f);
    }

    return dc;
}


/// @brief DOI: 10.4028/www.scientific.net/AMM.416-417.536
inline emb::vec3 compensate_deadtime_v2(const emb::vec3& dutycycles, const emb::vec3& currents,
                                        float current_threshold, float pwm_period, float deadtime) {
#ifdef EMBLIB_C28X
    return dutycycles;
#else
    emb::vec3 dc = dutycycles;
    const float deadtime_dutycycle = deadtime / pwm_period;

    const auto [min, max] = std::minmax_element(currents.begin(), currents.end());

    // use Kirchhoff's current law to determine if there is one positive or one negative current
    if (*min + *max > 0) {
        const auto idx = std::distance(currents.begin(), max);
        dc[size_t(idx)] = std::clamp(dc[size_t(idx)] + 2 * deadtime_dutycycle, 0.0f, 1.0f);
    } else if (*min + *max < 0) {
        const auto idx = std::distance(currents.begin(), min);
        dc[size_t(idx)] = std::clamp(dc[size_t(idx)] - 2 * deadtime_dutycycle, 0.0f, 1.0f);
    }

    return dc;
#endif
}


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


inline alphabeta_pair clarke_transform(const emb::vec3& vec3_) {
    float alpha = vec3_[0];
    float beta = (vec3_[1] - vec3_[2]) * numbers::inv_sqrt3;
    return alphabeta_pair(alpha, beta);
}


inline alphabeta_pair clarke_transform(float a, float b) {
    float alpha = a;
    float beta = (a + 2*b) * numbers::inv_sqrt3;
    return alphabeta_pair(alpha, beta);
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

