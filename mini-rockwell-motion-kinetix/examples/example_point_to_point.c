/**
 * @file example_point_to_point.c
 * @brief Example: Kinetix point-to-point motion using MAM instruction.
 *
 * Simulates a pick-and-place operation where a Kinetix axis drives
 * a conveyor belt between two positions, repeating the cycle.
 *
 * This example demonstrates:
 *   L6: Point-to-point positioning (classic servo problem)
 *   L7: Kinetix MAM instruction emulation
 *   Full trapezoidal profile planning and execution
 */

#include <stdio.h>
#include <math.h>
#include "axis_types.h"
#include "motion_planner.h"
#include "servo_tuning.h"

/* External declarations */
void motor_database_init_vpl_b1653f(motor_database_t *motor);
void axis_config_init(axis_config_t *config);

int main(void)
{
    printf("=== Kinetix Point-to-Point Motion Example ===\n\n");
    printf("Application: Pick-and-Place Conveyor\n");
    printf("Motor: VPL-B1653F (Kinetix 5300)\n");
    printf("Cycle: Position 0 → 360° → 0° (repeating)\n\n");

    /* Initialize motor and axis */
    motor_database_t motor;
    motor_database_init_vpl_b1653f(&motor);
    printf("Motor: %s\n", motor.catalog_number);
    printf("  Rated: %.2f N·m @ %.0f RPM, J=%.2f kg·cm²\n",
           motor.rated_torque_nm, motor.rated_speed_rpm, motor.rotor_inertia_kgcm2);

    /* Configure axis */
    axis_config_t config;
    axis_config_init(&config);
    config.position_unit = POSITION_UNIT_DEGREE;
    config.position_rotary = true;
    config.position_unwind = 360.0;
    config.max_velocity = 1000.0;    /* deg/s */
    config.max_acceleration = 5000.0; /* deg/s² */
    printf("\nAxis Config: Rotary, max vel=%.0f deg/s, max accel=%.0f deg/s²\n",
           config.max_velocity, config.max_acceleration);

    /* Initialize servo tuning */
    servo_tuning_t tuning;
    servo_tuning_init(&tuning, &motor);
    printf("Servo Tuning: Kp_vel=%.3f, Ki_vel=%.3f, Kp_pos=%.1f\n",
           tuning.velocity.kp_vel, tuning.velocity.ki_vel, tuning.position.kp_pos);

    /* Profile generator */
    profile_generator_t pg;
    profile_gen_init(&pg, 0.002); /* 2ms servo cycle */
    printf("\nProfile Generator: Ts=2.0ms\n");

    /* Run pick-and-place cycles */
    int num_cycles = 3;
    for (int cycle = 0; cycle < num_cycles; cycle++) {
        printf("\n--- Cycle %d ---\n", cycle + 1);

        /* Forward move: 0° → 180° */
        trapezoidal_params_t fwd = {
            .start_position = 0.0,
            .target_position = 180.0,
            .cruise_velocity = 500.0,  /* deg/s */
            .acceleration = 2500.0,     /* deg/s² */
            .deceleration = 2500.0,
            .move_type = MOVE_TYPE_ABSOLUTE
        };

        int result = profile_gen_plan_trapezoidal(&pg, &fwd);
        if (result != 0) {
            printf("  ERROR: Failed to plan forward move\n");
            return 1;
        }

        printf("  Forward Move: 0° → 180°\n");
        printf("  Profile: t_acc=%.3fs, t_cruise=%.3fs, t_decel=%.3fs, total=%.3fs\n",
               pg.t_acc, pg.t_cruise, pg.t_decel, pg.t_total);

        /* Execute forward move */
        double pos, vel, acc;
        int step = 0;
        printf("  Execution: ");
        while (!profile_gen_is_done(&pg)) {
            profile_gen_step(&pg, &pos, &vel, &acc);
            if (step % 100 == 0) {
                printf("%.0f°@%.0f°/s ", pos, vel);
            }
            step++;
        }
        printf("\n  Done: final position = %.2f° (steps=%d)\n", pos, step);

        /* Return move: 180° → 0° */
        profile_gen_reset(&pg);
        trapezoidal_params_t ret = {
            .start_position = 180.0,
            .target_position = 0.0,
            .cruise_velocity = 500.0,
            .acceleration = 2500.0,
            .deceleration = 2500.0,
            .move_type = MOVE_TYPE_ABSOLUTE
        };

        profile_gen_plan_trapezoidal(&pg, &ret);
        printf("  Return Move: 180° → 0°\n");
        printf("  Profile: total=%.3fs\n", pg.t_total);

        step = 0;
        printf("  Execution: ");
        while (!profile_gen_is_done(&pg)) {
            profile_gen_step(&pg, &pos, &vel, &acc);
            if (step % 100 == 0) {
                printf("%.0f° ", pos);
            }
            step++;
        }
        printf("\n  Done: final position = %.2f° (steps=%d)\n", pos, step);

        profile_gen_reset(&pg);
    }

    printf("\n=== Pick-and-Place Complete: %d cycles ===\n", num_cycles);
    return 0;
}
