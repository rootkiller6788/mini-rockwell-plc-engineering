/**
 * @file bench_motion_perf.c
 * @brief Performance benchmark for motion profile generation.
 *
 * Measures execution time for:
 *   - Trapezoidal profile planning
 *   - S-curve profile planning
 *   - Profile step execution (batch)
 *   - Cam table evaluation
 *   - Notch filter throughput
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include "motion_planner.h"
#include "servo_tuning.h"

/* Simple high-res timer using clock() */
static double get_time_sec(void)
{
    return (double)clock() / (double)CLOCKS_PER_SEC;
}

#define BENCH_ITERATIONS 100000
#define BENCH_NAME(name) printf("\n  %s:\n", name)

int main(void)
{
    printf("=== Motion Performance Benchmarks ===\n");
    printf("Iterations per test: %d\n", BENCH_ITERATIONS);

    double t_start, t_end;

    /* 1. Trapezoidal profile planning */
    BENCH_NAME("Trapezoidal Profile Planning");
    {
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

        t_start = get_time_sec();
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            profile_gen_plan_trapezoidal(&pg, &params);
        }
        t_end = get_time_sec();
        printf("    Time: %.3f ms total, %.3f µs/plan\n",
               (t_end - t_start) * 1000.0,
               (t_end - t_start) / BENCH_ITERATIONS * 1e6);
    }

    /* 2. S-curve profile planning */
    BENCH_NAME("S-Curve Profile Planning");
    {
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

        t_start = get_time_sec();
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            profile_gen_plan_scurve(&pg, &params);
        }
        t_end = get_time_sec();
        printf("    Time: %.3f ms total, %.3f µs/plan\n",
               (t_end - t_start) * 1000.0,
               (t_end - t_start) / BENCH_ITERATIONS * 1e6);
    }

    /* 3. Profile step execution */
    BENCH_NAME("Profile Step Execution (batch of 1000 steps)");
    {
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

        int batch_size = 1000;
        int num_batches = BENCH_ITERATIONS / batch_size;
        double pos, vel, acc;

        t_start = get_time_sec();
        for (int b = 0; b < num_batches; b++) {
            profile_gen_reset(&pg);
            profile_gen_plan_trapezoidal(&pg, &params);
            for (int s = 0; s < batch_size; s++) {
                profile_gen_step(&pg, &pos, &vel, &acc);
            }
        }
        t_end = get_time_sec();
        double total_steps = (double)num_batches * batch_size;
        printf("    Time: %.3f ms total, %.3f µs/step\n",
               (t_end - t_start) * 1000.0,
               (t_end - t_start) / total_steps * 1e6);
    }

    /* 4. Cam table evaluation */
    BENCH_NAME("Cam Table Evaluation");
    {
        cam_table_t cam;
        cam_table_init(&cam, 10, true);

        /* Populate cam table */
        for (int i = 0; i < 10; i++) {
            cam_table_add_point(&cam, i * 36.0, i * 72.0);
        }

        double slave_pos, slave_vel;
        t_start = get_time_sec();
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            double master = fmod((double)i * 1.7, 360.0);
            cam_evaluate(&cam, master, &slave_pos, &slave_vel);
        }
        t_end = get_time_sec();
        printf("    Time: %.3f ms total, %.3f µs/eval\n",
               (t_end - t_start) * 1000.0,
               (t_end - t_start) / BENCH_ITERATIONS * 1e6);

        cam_table_free(&cam);
    }

    /* 5. Notch filter throughput */
    BENCH_NAME("Notch Filter Throughput");
    {
        notch_filter_t nf;
        notch_filter_design(&nf, 100.0, 0.01, 0.1, 0.001);

        double input = 0.0;
        double phase = 0.0;

        t_start = get_time_sec();
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            input = sin(phase);
            notch_filter_apply(&nf, input);
            phase += 0.1;
        }
        t_end = get_time_sec();
        printf("    Time: %.3f ms total, %.3f µs/filter\n",
               (t_end - t_start) * 1000.0,
               (t_end - t_start) / BENCH_ITERATIONS * 1e6);
    }

    /* 6. Low-pass filter throughput */
    BENCH_NAME("Low-Pass Filter Throughput");
    {
        low_pass_filter_t lpf;
        lpf_init(&lpf, 200.0, 0.001);

        double val = 0.0;
        t_start = get_time_sec();
        for (int i = 0; i < BENCH_ITERATIONS; i++) {
            val = lpf_apply(&lpf, (double)(i % 100));
        }
        t_end = get_time_sec();
        printf("    Time: %.3f ms total, %.3f µs/filter\n",
               (t_end - t_start) * 1000.0,
               (t_end - t_start) / BENCH_ITERATIONS * 1e6);
        (void)val; /* suppress unused warning */
    }

    printf("\n=== Benchmark Complete ===\n");
    return 0;
}
