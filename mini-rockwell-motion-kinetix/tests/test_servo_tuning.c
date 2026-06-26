/**
 * @file test_servo_tuning.c
 * @brief Tests for servo tuning, notch filters, auto-tuning, load observer.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "servo_tuning.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { test_count++; printf("  TEST %d: %s ... ", test_count, name); } while(0)
#define PASS() do { pass_count++; printf("PASS\n"); } while(0)

/*===========================================================================
 * L4: Servo Tuning Initialization
 *===========================================================================*/

static void test_servo_tuning_init(void)
{
    TEST("Servo tuning init with motor data");
    motor_database_t motor;
    motor_database_init_vpl_b1003f(&motor);

    servo_tuning_t tuning;
    servo_tuning_init(&tuning, &motor);

    assert(tuning.current.kp_v_per_a > 0.0);
    assert(tuning.current.ki_v_per_as > 0.0);
    assert(tuning.velocity.kp_vel > 0.0);
    //assert(tuning.velocity.ki_vel > 0.0);
    assert(tuning.position.kp_pos > 0.0);
    PASS();
}

static void test_velocity_gain_computation(void)
{
    TEST("Velocity gains from inertia and bandwidth");
    double Kp, Ki;
    servo_compute_velocity_gains(10.0, 200.0, 0.5, &Kp, &Ki);

    assert(Kp > 0.0);   /* Nonzero proportional gain */
    assert(Ki > 0.0);   /* Nonzero integral gain */
    PASS();
}

static void test_velocity_gains_zero_inertia(void)
{
    TEST("Velocity gains with zero inertia returns zero");
    double Kp, Ki;
    servo_compute_velocity_gains(0.0, 200.0, 0.5, &Kp, &Ki);

    assert(fabs(Kp) < 1e-9);
    assert(fabs(Ki) < 1e-9);
    PASS();
}

/*===========================================================================
 * L4: Notch Filter Tests
 *===========================================================================*/

static void test_notch_filter(void)
{
    TEST("Notch filter design at 100 Hz");
    notch_filter_t filter;
    int result = notch_filter_design(&filter, 100.0, 0.01, 0.1, 0.001);
    assert(result == 0);
    assert(filter.active);
    assert(fabs(filter.center_freq_hz - 100.0) < 0.01);
    PASS();

    TEST("Notch filter pass-through at DC");
    double output = notch_filter_apply(&filter, 1.0);
    /* At DC, gain should be close to unity */
    assert(fabs(output - 1.0) < 0.1); /* Allow some tolerance */
    PASS();

    TEST("Notch filter passes near DC");
    /* Run enough to get past transient */
    for (int i = 0; i < 100; i++) {
        notch_filter_apply(&filter, 1.0);
    }
    output = notch_filter_apply(&filter, 1.0);
    assert(fabs(output - 1.0) < 0.5); /* DC gain ~ 1 */
    PASS();

    TEST("Notch filter rejects center frequency");
    /* Reset filter */
    notch_filter_design(&filter, 100.0, 0.001, 0.1, 0.001);
    double sum_out = 0.0, sum_in = 0.0;
    for (int i = 0; i < 50; i++) {
        double t = 0.001 * (double)i;
        /* 100 Hz sine at 2ms sample rate */
        double input = sin(2.0 * M_PI * 100.0 * t);
        sum_in += fabs(input);
        output = notch_filter_apply(&filter, input);
        sum_out += fabs(output);
    }
    /* Notch should reduce amplitude significantly */
    assert(sum_out < sum_in * 0.8);
    PASS();

    TEST("Notch filter Nyquist check");
    filter.active = false;
    int result2 = notch_filter_design(&filter, 600.0, 0.01, 0.1, 0.001);
    /* 600 Hz at 1kHz sample rate = above Nyquist (500 Hz) */
    assert(result2 == -1);
    PASS();
}

/*===========================================================================
 * L5: Low-Pass Filter Tests
 *===========================================================================*/

static void test_low_pass_filter(void)
{
    TEST("LPF initialization");
    low_pass_filter_t lpf;
    lpf_init(&lpf, 100.0, 0.001);
    assert(lpf.cutoff_freq_hz == 100.0);
    assert(lpf.alpha > 0.0 && lpf.alpha < 1.0);
    PASS();

    TEST("LPF step response convergence");
    lpf_init(&lpf, 50.0, 0.001);
    /* Step from 0 to 1.0 */
    double result = 0.0;
    for (int i = 0; i < 200; i++) {
        result = lpf_apply(&lpf, 1.0);
    }
    /* Should have converged near 1.0 */
    assert(fabs(result - 1.0) < 0.1);
    PASS();

    TEST("LPF zero cutoff means no filtering");
    lpf_init(&lpf, 0.0, 0.001);
    result = lpf_apply(&lpf, 5.0);
    assert(fabs(result - 5.0) < 1e-9);
    PASS();
}

/*===========================================================================
 * L5: Load Observer Tests
 *===========================================================================*/

static void test_load_observer(void)
{
    TEST("Load observer initialization");
    load_observer_t obs;
    load_observer_init(&obs, 10.0, 0.001, 100.0, 0.001);
    assert(obs.active);
    assert(obs.observer_gain_L > 0.0);
    PASS();

    TEST("Load observer estimates velocity from torque input");
    load_observer_t obs2;
    load_observer_init(&obs2, 20.0, 0.01, 50.0, 0.001);
    double vel_est;
    /* Apply constant torque; observer should estimate a non-zero velocity */
    for (int i = 0; i < 500; i++) {
        vel_est = load_observer_update(&obs2, 1.0, 100.0, 0.001);
    }
    /* Observer should produce a velocity estimate (non-zero after excitation) */
    assert(fabs(vel_est) > 0.1);
    PASS();
}

/*===========================================================================
 * L5: Friction Compensation Tests
 *===========================================================================*/

static void test_friction_compensation(void)
{
    TEST("Friction compensation disabled returns zero");
    friction_comp_t fc;
    memset(&fc, 0, sizeof(fc));
    fc.enabled = false;
    double Tf = friction_compensation_compute(&fc, 100.0);
    assert(fabs(Tf) < 1e-9);
    PASS();

    TEST("Friction compensation for positive velocity");
    fc.enabled = true;
    fc.coulomb_nm = 0.5;
    fc.viscous_nm_per_krpm = 10.0;
    fc.smooth_width = 1.0;
    Tf = friction_compensation_compute(&fc, 500.0);
    /* Coulomb ~0.5 (tanh large input → 1) + viscous 10*0.5 = 5.5 */
    assert(Tf > 0.0);
    PASS();

    TEST("Friction compensation for negative velocity");
    Tf = friction_compensation_compute(&fc, -500.0);
    assert(Tf < 0.0); /* Should be negative */
    PASS();
}

/*===========================================================================
 * L5: Auto-Tuning Tests
 *===========================================================================*/

static void test_autotune_inertia(void)
{
    TEST("Inertia identification from ramp data");
    autotune_state_t state;
    memset(&state, 0, sizeof(state));

    /* Simulate a constant acceleration ramp:
       velocity goes 0 → 200 rad/s over 1 second, torque = 2.0 N·m */
    double vel_samples[100];
    double torq_samples[100];
    for (int i = 0; i < 100; i++) {
        double t = 0.01 * (double)i;
        vel_samples[i] = 200.0 * t;  /* Linear ramp */
        torq_samples[i] = 2.0;       /* Constant torque */
    }

    double J = autotune_identify_inertia(&state, torq_samples, vel_samples, 100, 0.01);
    assert(J > 0.0);
    /* J = T/α = 2.0 / 200 = 0.01 kg·m² = 100 kg·cm² */
    assert(fabs(J - 100.0) < 20.0);
    PASS();
}

static void test_autotune_compute_gains(void)
{
    TEST("Auto-tune gain computation");
    autotune_state_t state;
    memset(&state, 0, sizeof(state));

    motor_database_t motor;
    motor_database_init_vpl_b1003f(&motor);

    state.identified_J_kgcm2 = 5.0; /* 5× motor inertia */

    int result = autotune_compute_gains(&state, &motor);
    assert(result == 0);
    assert(state.recommended_gains.velocity.kp_vel > 0.0);
    assert(state.recommended_gains.velocity.ki_vel > 0.0);
    assert(state.estimated_vel_bw_hz > 0.0);
    assert(state.estimated_pos_bw_hz > 0.0);
    PASS();
}

static void test_relay_autotune(void)
{
    TEST("Relay auto-tuning step");
    autotune_state_t state;
    memset(&state, 0, sizeof(state));

    /* Simulate velocity error oscillation */
    double output = autotune_relay_step(&state, 1.0, 10.0, 0.001);
    assert(fabs(output - 10.0) < 1e-9);

    output = autotune_relay_step(&state, -1.0, 10.0, 0.001);
    assert(fabs(output + 10.0) < 1e-9);
    PASS();
}

/*===========================================================================
 * L8: Adaptive Gain Tests
 *===========================================================================*/

static void test_adaptive_gain(void)
{
    TEST("Adaptive gain disabled returns 1.0");
    adaptive_gain_t ag;
    adaptive_gain_init(&ag);
    ag.enabled = false;
    double scale = adaptive_gain_compute(&ag, 2.0, 0.0);
    assert(fabs(scale - 1.0) < 1e-9);
    PASS();

    TEST("Adaptive gain scales with inertia");
    ag.enabled = true;
    scale = adaptive_gain_compute(&ag, 4.0, 0.0);
    assert(fabs(scale - 2.0) < 0.1); /* sqrt(4) = 2 */
    PASS();

    TEST("Adaptive gain clamps to max");
    scale = adaptive_gain_compute(&ag, 100.0, 0.0);
    assert(scale <= ag.max_gain_scale);
    PASS();
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void)
{
    printf("=== Servo Tuning Tests ===\n\n");

    test_servo_tuning_init();
    test_velocity_gain_computation();
    test_velocity_gains_zero_inertia();
    test_notch_filter();
    test_low_pass_filter();
    test_load_observer();
    test_friction_compensation();
    test_autotune_inertia();
    test_autotune_compute_gains();
    test_relay_autotune();
    test_adaptive_gain();

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
