/**
 * @example example_heat_exchanger_cascade.c
 * @brief Heat Exchanger Temperature Cascade Control ? L6 Canonical Problem
 *
 * A shell-and-tube heat exchanger with:
 *   - Primary loop: Outlet temperature control (slow dynamics)
 *   - Secondary loop: Steam flow control (fast dynamics)
 *
 * Cascade control provides superior disturbance rejection because
 * the secondary steam flow loop compensates for steam pressure
 * variations before they affect the outlet temperature.
 *
 * This example demonstrates:
 *   - Cascade PID configuration
 *   - Primary-secondary coordination
 *   - Bumpless cascade enable/disable
 *   - Disturbance rejection comparison
 */
#include <stdio.h>
#include <math.h>
#include "plantpax_control_loop.h"

#define DT_PRIMARY_S    1.0
#define DT_SECONDARY_S  0.2
#define SIM_TIME_S      200.0

/* Heat exchanger process model */
#define PROCESS_GAIN     2.5     /* degC / (kg/s steam) */
#define PROCESS_TAU      30.0    /* Primary time constant (s) */
#define PROCESS_DEADTIME 5.0     /* Dead time (s) */
#define STEAM_TAU        2.0     /* Steam flow time constant (s) */

int main(void)
{
    printf("==================================================\n");
    printf("  PlantPAx DCS ? Heat Exchanger Cascade Control\n");
    printf("==================================================\n\n");

    /* --- Setup Primary Loop (Temperature Control) --- */
    pax_pid_loop_t primary;
    pax_pid_loop_init(&primary, 1, "TIC101", PAX_LOOP_TYPE_CASCADE,
                       PAX_ACTION_REVERSE, 0.0, 100.0);
    /* Slow primary loop: moderate gain, integral-dominated */
    pax_pid_params_init(&primary.params, PAX_PID_FORM_VELOCITY,
                         1.5, 60.0, 0.0, DT_PRIMARY_S);
    primary.mode = PAX_LOOP_MODE_AUTO;
    primary.setpoint = 80.0;  /* Desired outlet temperature */

    /* --- Setup Secondary Loop (Steam Flow Control) --- */
    pax_pid_loop_t secondary;
    pax_pid_loop_init(&secondary, 2, "FIC101", PAX_LOOP_TYPE_CASCADE,
                       PAX_ACTION_DIRECT, 0.0, 100.0);
    /* Fast secondary loop: higher gain, short integral */
    pax_pid_params_init(&secondary.params, PAX_PID_FORM_VELOCITY,
                         3.0, 5.0, 0.0, DT_SECONDARY_S);

    /* --- Setup Cascade --- */
    pax_cascade_t cascade;
    cascade.primary = &primary;
    cascade.secondary = &secondary;
    cascade.cascade_enabled = true;
    cascade.primary_sp_min = 0.0;
    cascade.primary_sp_max = 100.0;
    cascade.secondary_sp_ratio = 1.0;
    cascade.secondary_sp_bias = 0.0;

    /* --- Process State --- */
    double outlet_temp = 25.0;   /* Initial: cold fluid */
    double steam_flow = 0.0;
    double steam_pressure = 10.0; /* bar */

    printf("%-8s %-10s %-10s %-10s %-10s %-12s\n",
           "Time", "SP(C)", "PV(C)", "PriCO", "SecCO", "SteamFlow");
    printf("--------------------------------------------------------------\n");

    double t;
    uint32_t sec_steps = (uint32_t)(DT_PRIMARY_S / DT_SECONDARY_S);

    for (t = 0.0; t <= SIM_TIME_S; t += DT_PRIMARY_S) {
        /* SP ramp at start */
        if (t >= 30.0 && primary.setpoint < 80.0) {
            primary.setpoint = 80.0;
        }

        /* Steam pressure disturbance at t=100s */
        if (t >= 100.0 && t < 120.0) {
            steam_pressure = 7.0;  /* Reduced steam pressure */
        } else {
            steam_pressure = 10.0;
        }

        /* --- Execute cascade control --- */
        uint32_t s;
        for (s = 0; s < sec_steps; s++) {
            pax_cascade_execute(&cascade, outlet_temp, steam_flow,
                                 DT_PRIMARY_S, DT_SECONDARY_S);
        }

        /* --- Process dynamics (first-order + deadtime approximation) --- */
        /* Steam flow responds to secondary CO */
        double steam_flow_target = secondary.output / 100.0 * 10.0; /* 0-10 kg/s */
        steam_flow += (DT_PRIMARY_S / STEAM_TAU) *
                      (steam_flow_target - steam_flow);

        /* Actual heat input proportional to steam flow * pressure */
        double heat_input = steam_flow * (steam_pressure / 10.0) * PROCESS_GAIN;

        /* Outlet temperature: first-order response */
        outlet_temp += (DT_PRIMARY_S / PROCESS_TAU) *
                       (heat_input - outlet_temp + 25.0);

        /* Print every 10 seconds */
        if ((int)t % 10 == 0) {
            printf("%-8.0f %-10.1f %-10.1f %-10.1f %-10.1f %-12.2f\n",
                   t, primary.setpoint, outlet_temp,
                   primary.output, secondary.output, steam_flow);
        }
    }

    printf("\n--- Cascade Control Summary ---\n");
    printf("  Final Temperature: %.1f degC (SP: %.1f)\n",
           outlet_temp, primary.setpoint);
    printf("  Final Steam Flow:  %.2f kg/s\n", steam_flow);

    return 0;
}
