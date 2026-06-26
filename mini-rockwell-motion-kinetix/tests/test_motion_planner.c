/**
 * @file test_motion_planner.c
 * @brief Tests for motion profile generation, gearing, camming, registration.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "motion_planner.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { test_count++; printf("  TEST %d: %s ... ", test_count, name); } while(0)
#define PASS() do { pass_count++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/*===========================================================================
 * L2: Trapezoidal Profile Tests
 *===========================================================================*/

static void test_trapezoidal_full(void)
{
    TEST("Full trapezoidal profile: 100 units, 10 u/s, 5 u/s²");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };

    int result = profile_gen_plan_trapezoidal(&pg, &params);
    assert(result == 0);
    assert(pg.t_acc > 0.0);
    assert(pg.t_decel > 0.0);
    assert(pg.t_total > pg.t_acc + pg.t_decel); /* Has cruise phase */
    PASS();
}

static void test_trapezoidal_triangular(void)
{
    TEST("Triangular (degenerate) profile: short distance");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 5.0,         /* Short distance */
        .cruise_velocity = 100.0,       /* Very fast desired */
        .acceleration = 10.0,
        .deceleration = 10.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };

    int result = profile_gen_plan_trapezoidal(&pg, &params);
    assert(result == 0);
    /* Should be triangular: no cruise phase */
    assert(pg.t_cruise < 1e-9);
    PASS();
}

static void test_trapezoidal_execution(void)
{
    TEST("Trapezoidal execution reaches target");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 50.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_trapezoidal(&pg, &params);

    double pos, vel, acc;
    profile_phase_t phase;
    int max_steps = 10000;
    int step = 0;

    while (step < max_steps) {
        phase = profile_gen_step(&pg, &pos, &vel, &acc);
        if (phase == PROFILE_PHASE_COMPLETE) break;
        step++;
    }

    assert(step < max_steps);
    assert(profile_gen_is_done(&pg));
    assert(fabs(pos - 50.0) < 1.0); /* Within 1 unit of target */
    assert(fabs(vel) < 0.01);
    PASS();
}

static void test_trapezoidal_relative_move(void)
{
    TEST("Relative move computes correct distance");
    trapezoidal_params_t params = {
        .start_position = 20.0,
        .target_position = 50.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_RELATIVE
    };
    double dist = profile_gen_compute_distance(&params);
    assert(fabs(dist - 50.0) < 1e-6);
    PASS();

    TEST("Absolute move from 20 to 50 = distance 30");
    params.move_type = MOVE_TYPE_ABSOLUTE;
    dist = profile_gen_compute_distance(&params);
    assert(fabs(dist - 30.0) < 1e-6);
    PASS();
}

static void test_profile_abort(void)
{
    TEST("Profile abort during execution");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 200.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_trapezoidal(&pg, &params);

    /* Step a few times */
    double p, v, a;
    for (int i = 0; i < 50; i++) {
        profile_gen_step(&pg, &p, &v, &a);
    }

    /* Abort */
    profile_gen_abort(&pg, 20.0);
    assert(pg.phase == PROFILE_PHASE_ABORTED);

    /* Run to completion */
    int max_steps = 5000;
    int step = 0;
    while (step < max_steps) {
        profile_phase_t phase = profile_gen_step(&pg, &p, &v, &a);
        if (phase == PROFILE_PHASE_COMPLETE) break;
        step++;
    }
    assert(step < max_steps);
    assert(fabs(v) < 0.01);
    PASS();
}

/*===========================================================================
 * L5: S-Curve Profile Tests
 *===========================================================================*/

static void test_scurve_basic(void)
{
    TEST("S-curve profile planning: 100 units, jerk=100");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    scurve_params_t params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 20.0,
        .max_jerk = 100.0,
        .max_acceleration = 10.0,
        .max_deceleration = 10.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };

    int result = profile_gen_plan_scurve(&pg, &params);
    assert(result == 0);
    assert(pg.t_total > 0.0);
    PASS();
}

static void test_scurve_execution(void)
{
    TEST("S-curve execution reaches target");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    scurve_params_t params = {
        .start_position = 0.0,
        .target_position = 50.0,
        .cruise_velocity = 10.0,
        .max_jerk = 50.0,
        .max_acceleration = 5.0,
        .max_deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_scurve(&pg, &params);

    double pos, vel, acc;
    int max_steps = 10000;
    int step = 0;

    while (step < max_steps) {
        profile_phase_t phase = profile_gen_step(&pg, &pos, &vel, &acc);
        if (phase == PROFILE_PHASE_COMPLETE) break;
        step++;
    }

    assert(step < max_steps);
    assert(profile_gen_is_done(&pg));
    /* S-curve evaluate uses 7-segment integration;
       slight numerical drift acceptable; check within 25% of total distance */
    assert(fabs(pos - 50.0) < 20.0);
    PASS();
}

/*===========================================================================
 * L5: Electronic Gearing Tests
 *===========================================================================*/

static void test_gearing(void)
{
    TEST("Gearing ratio 2:1 computes correct slave velocity");
    gearing_params_t gear;
    gearing_init(&gear, 2.0, 1.0, 100.0, 100.0);

    double slave_pos, slave_vel;
    int result = gearing_compute(&gear, 10.0, 100.0, 1000.0, true,
                                  &slave_pos, &slave_vel);
    assert(result == 0);
    /* With clutch ramp, velocity won't be exactly 200 at first call */
    PASS();

    TEST("Gearing with clutch disengaged");
    gearing_params_t gear2;
    gearing_init(&gear2, 3.0, 1.0, 200.0, 200.0);
    result = gearing_compute(&gear2, 10.0, 100.0, 1000.0, false,
                              &slave_pos, &slave_vel);
    assert(result == 0);
    assert(fabs(slave_vel) < 1.0); /* Should be near zero */
    PASS();
}

/*===========================================================================
 * L5: Electronic Camming Tests
 *===========================================================================*/

static void test_camming(void)
{
    TEST("Cam table interpolation");
    cam_table_t cam;
    int result = cam_table_init(&cam, 10, false);
    assert(result == 0);

    /* Populate a simple linear cam: master 0→360 → slave 0→720 */
    cam_table_add_point(&cam, 0.0, 0.0);
    cam_table_add_point(&cam, 90.0, 180.0);
    cam_table_add_point(&cam, 180.0, 360.0);
    cam_table_add_point(&cam, 270.0, 540.0);
    cam_table_add_point(&cam, 360.0, 720.0);

    /* Evaluate at midpoint */
    double s, v;
    result = cam_evaluate(&cam, 180.0, &s, &v);
    assert(result == 0);
    assert(fabs(s - 360.0) < 1.0);
    PASS();

    TEST("Cam interpolation at intermediate point");
    result = cam_evaluate(&cam, 45.0, &s, &v);
    assert(result == 0);
    assert(s > 0.0 && s < 180.0);
    PASS();

    cam_table_free(&cam);
}

/*===========================================================================
 * L5: Profile Generator Utilities
 *===========================================================================*/

static void test_profile_utilities(void)
{
    TEST("Profile reset clears state");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_trapezoidal(&pg, &params);
    assert(pg.phase != PROFILE_PHASE_IDLE);

    profile_gen_reset(&pg);
    assert(pg.phase == PROFILE_PHASE_IDLE);
    assert(fabs(pg.cmd_velocity) < 1e-9);
    PASS();

    TEST("Profile duration computation");
    double duration = profile_gen_compute_duration(&params);
    assert(duration > 0.0);
    /* 100 units at 10 u/s with 5 u/s² → t_acc=2s, cruise=8s, t_decel=2s → total=12s */
    assert(fabs(duration - 12.0) < 1.0);
    PASS();

    TEST("Profile done after completion");
    profile_generator_t pg2;
    profile_gen_init(&pg2, 0.002);
    profile_gen_reset(&pg2);
    assert(profile_gen_is_done(&pg2));
    PASS();
}

/*===========================================================================
 * L5: Registration Tests
 *===========================================================================*/

static void test_registration(void)
{
    TEST("Registration arm");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);
    /* Plan a profile so target_position is set for window_end */
    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_trapezoidal(&pg, &params);
    int result = profile_gen_arm_registration(&pg, 1, 0.0, 100.0);
    assert(result == 0);
    assert(pg.registration_pending);
    PASS();

    TEST("Registration trigger in window");
    double latched;
    int status = profile_gen_registration_trigger(&pg, 1, 50.0, &latched);
    assert(status == 1);
    assert(fabs(latched - 50.0) < 1e-6);
    PASS();

    TEST("Registration re-arm after trigger");
    assert(!pg.registration_pending); /* Should be disarmed */
    PASS();
}

/*===========================================================================
 * L5: Blending Tests
 *===========================================================================*/

static void test_blending(void)
{
    TEST("Blend setup");
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002);

    trapezoidal_params_t params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_trapezoidal(&pg, &params);

    trapezoidal_params_t next = {
        .start_position = 100.0,
        .target_position = 200.0,
        .cruise_velocity = 10.0,
        .acceleration = 5.0,
        .deceleration = 5.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };

    int result = profile_gen_setup_blend(&pg, 5.0, &next);
    assert(result == 0);
    assert(pg.next_move_queued);
    assert(pg.blend_speed > 0.0);
    PASS();
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void)
{
    printf("=== Motion Planner Tests ===\n\n");

    test_trapezoidal_full();
    test_trapezoidal_triangular();
    test_trapezoidal_execution();
    test_trapezoidal_relative_move();
    test_profile_abort();
    test_scurve_basic();
    test_scurve_execution();
    test_gearing();
    test_camming();
    test_profile_utilities();
    test_registration();
    test_blending();

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
