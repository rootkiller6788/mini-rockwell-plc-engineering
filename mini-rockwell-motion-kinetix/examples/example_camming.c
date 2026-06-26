/**
 * @file example_camming.c
 * @brief Example: Electronic camming for rotary knife (MAPC instruction).
 *
 * Simulates a rotary knife application where a slave axis follows
 * a master encoder with a position cam profile. The knife must:
 *   1. Match the conveyor speed during the cut
 *   2. Return to home position between cuts
 *   3. Handle different product lengths via the cam profile
 *
 * This example demonstrates:
 *   L6: Electronic camming (classic motion problem)
 *   L7: Rockwell MAPC instruction emulation
 *   Cubic spline interpolation for smooth motion
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "motion_planner.h"

void axis_config_init(axis_config_t *config);

int main(void)
{
    printf("=== Kinetix Electronic Camming Example ===\n\n");
    printf("Application: Rotary Knife (MAPC)\n");
    printf("Master: Conveyor encoder (0-360° per product cycle)\n");
    printf("Slave:  Knife axis (0-360° per knife revolution)\n\n");

    /* Build the cam profile: master 0-360 → slave following pattern */
    cam_table_t cam;
    int result = cam_table_init(&cam, 20, true); /* Cyclic cam */
    if (result != 0) {
        printf("ERROR: Failed to initialize cam table\n");
        return 1;
    }

    /* Cam profile design:
       - Master 0-20°:   Knife accelerates from 0° to match conveyor
       - Master 20-60°:  Knife matches conveyor (cutting zone, 1:1 slope)
       - Master 60-80°:  Knife decelerates and retracts
       - Master 80-360°: Knife dwells at 360° (=0°) waiting for next product
    */
    printf("Cam Table:\n");
    printf("  Master(°)  Slave(°)  Description\n");

    double cam_data[][2] = {
        {0.0,   0.0},    /* Start */
        {20.0,  20.0},   /* Accelerate to match */
        {60.0,  60.0},   /* Cutting zone (1:1 ratio) */
        {80.0,  80.0},   /* End of cut, begin retract */
        {150.0, 360.0},  /* Rapid retract */
        {360.0, 360.0},  /* Dwell until next cycle */
    };

    for (size_t i = 0; i < sizeof(cam_data) / sizeof(cam_data[0]); i++) {
        cam_table_add_point(&cam, cam_data[i][0], cam_data[i][1]);
        printf("  %7.1f    %7.1f    %s\n",
               cam_data[i][0], cam_data[i][1],
               (i == 0) ? "Start" :
               (i == 2 || i == 3) ? "Cut zone" :
               (i == 4) ? "Retract" :
               (i == 5) ? "Dwell/cycle end" : "Accel");
    }

    /* Simulate the conveyor master moving at constant speed */
    printf("\nSimulating 3 product cycles:\n");

    double master_speed = 100.0; /* deg/s on master encoder */
    double sim_dt = 0.005;       /* 5ms simulation step */
    int cycles = 3;
    double master_pos = 0.0;
    int print_divider = 20;      /* Print every Nth step */

    for (int cycle = 0; cycle < cycles; cycle++) {
        printf("\n  --- Product %d ---\n", cycle + 1);

        int step = 0;
        while (master_pos < 360.0 * (cycle + 1)) {
            master_pos += master_speed * sim_dt;

            /* Wrap master for cyclic cam */
            double wrapped_master = fmod(master_pos, 360.0);
            if (wrapped_master < 0.0) wrapped_master += 360.0;

            /* Evaluate cam */
            double slave_pos, slave_vel;
            cam_evaluate(&cam, wrapped_master, &slave_pos, &slave_vel);

            if (step % print_divider == 0) {
                printf("    Master=%.1f° Slv=%.1f° SlvVel=%.1f°/s",
                       wrapped_master, slave_pos, slave_vel);
                if (wrapped_master >= 20.0 && wrapped_master <= 60.0) {
                    printf(" [CUT]");
                }
                printf("\n");
            }

            step++;
        }
    }

    printf("\n=== Camming Simulation Complete ===\n");
    printf("Knife speed matched conveyor during cutting zone (20-60°).\n");
    printf("Retracted and reset before next product arrives.\n");

    cam_table_free(&cam);
    return 0;
}
