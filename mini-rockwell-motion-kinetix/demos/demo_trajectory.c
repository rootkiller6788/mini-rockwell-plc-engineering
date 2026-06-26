/**
 * @file demo_trajectory.c
 * @brief Demonstration: CSV output of motion trajectories for visualization.
 *
 * Generates CSV data comparing trapezoidal vs S-curve profiles,
 * suitable for plotting with gnuplot, matplotlib, or Excel.
 *
 * Output columns:
 *   time, trap_pos, trap_vel, trap_acc, scurve_pos, scurve_vel, scurve_acc, scurve_jerk
 */

#include <stdio.h>
#include <math.h>
#include "motion_planner.h"

int main(void)
{
    printf("time,trap_pos,trap_vel,trap_acc,scurve_pos,scurve_vel,scurve_acc,scurve_jerk\n");

    /* Trapezoidal profile */
    profile_generator_t pg_trap;
    profile_gen_init(&pg_trap, 0.001);

    trapezoidal_params_t trap_params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 20.0,
        .acceleration = 10.0,
        .deceleration = 10.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_trapezoidal(&pg_trap, &trap_params);

    /* S-curve profile */
    profile_generator_t pg_scurve;
    profile_gen_init(&pg_scurve, 0.001);

    scurve_params_t scurve_params = {
        .start_position = 0.0,
        .target_position = 100.0,
        .cruise_velocity = 20.0,
        .max_jerk = 200.0,
        .max_acceleration = 10.0,
        .max_deceleration = 10.0,
        .move_type = MOVE_TYPE_ABSOLUTE
    };
    profile_gen_plan_scurve(&pg_scurve, &scurve_params);

    /* Generate CSV data */
    double time = 0.0;
    double dt = 0.001;
    int max_steps = 15000;

    for (int step = 0; step < max_steps; step++) {
        double trap_pos, trap_vel, trap_acc;
        double scurve_pos, scurve_vel, scurve_acc;

        profile_phase_t trap_phase = profile_gen_step(&pg_trap, &trap_pos, &trap_vel, &trap_acc);
        profile_phase_t scurve_phase = profile_gen_step(&pg_scurve, &scurve_pos, &scurve_vel, &scurve_acc);

        printf("%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
               time, trap_pos, trap_vel, trap_acc,
               scurve_pos, scurve_vel, scurve_acc, pg_scurve.cmd_jerk);

        if (trap_phase == PROFILE_PHASE_COMPLETE &&
            scurve_phase == PROFILE_PHASE_COMPLETE) {
            break;
        }

        time += dt;
    }

    return 0;
}
