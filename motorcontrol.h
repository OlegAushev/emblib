#pragma once


#include <emblib/core.h>
#include <emblib/math.h>

#if defined(EMBLIB_C28X)
#include <emblib/array.h>
#include <motorcontrol/math.h>
#include <motorcontrol/clarke.h>
#include <motorcontrol/park.h>
#include <motorcontrol/ipark.h>
#elif defined(EMBLIB_STM32)
#include <array>
#endif


namespace emb {


#if defined(EMBLIB_C28X)
typedef emb::array<float, 3> vec3;
#elif defined(EMBLIB_STM32)
typedef std::array<float, 3> vec3;
#endif


namespace traits {
struct from_rpm{};
struct from_radps{};
}


class motor_speed {
public:
    const int pole_pairs;
private:
    float _radps_elec;
public:
    explicit motor_speed(int pole_pairs_)
            : pole_pairs(pole_pairs_)
            , _radps_elec(0) {
    }

    motor_speed(int pole_pairs_, float radps_elec_, traits::from_radps)
            : pole_pairs(pole_pairs_) {
        from_radps(radps_elec_);
    }

    motor_speed(int pole_pairs_, float rpm_, traits::from_rpm)
            : pole_pairs(pole_pairs_) {
        from_rpm(rpm_);
    }

    float to_radps() const { return _radps_elec; }
    float to_rpm() const { return 60 * _radps_elec / (numbers::two_pi * float(pole_pairs)); }
    float to_radps_mech() const { return _radps_elec / float(pole_pairs); }

    void from_radps(float value) { _radps_elec = value; }
    void from_rpm(float value) { _radps_elec = numbers::two_pi * float(pole_pairs) * value / 60; }
};


inline float to_radps(float speed_rpm, int pole_pairs) { return numbers::two_pi * float(pole_pairs) * speed_rpm / 60; }


inline float to_radps(float speed_rpm) { return numbers::two_pi * speed_rpm / 60; }


inline float to_rpm(float speed_radps, int pole_pairs) { return 60 * speed_radps / (numbers::two_pi * float(pole_pairs)); }


inline emb::vec3 calculate_svpwm(float voltage_mag, float voltage_angle, float voltage_dc) {
    voltage_angle = rem_2pi(voltage_angle);
    voltage_mag = clamp<float>(voltage_mag, 0, voltage_dc / numbers::sqrt_3);

    int32_t sector = static_cast<int32_t>(voltage_angle / numbers::pi_over_3);
    float theta = voltage_angle - float(sector) * numbers::pi_over_3;

    // base vector times calculation
#if defined(EMBLIB_C28X)
    float tb1 = numbers::sqrt_3 * (voltage_mag / voltage_dc) * sinf(numbers::pi_over_3 - theta);
    float tb2 = numbers::sqrt_3 * (voltage_mag / voltage_dc) * sinf(theta);
#elif defined(EMBLIB_STM32)
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

    for(uint32_t i = 0; i < 3; ++i) {
        pulse_durations[i] = emb::clamp<float>(pulse_durations[i], 0.f, 1.f);
    }
    return pulse_durations;
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

