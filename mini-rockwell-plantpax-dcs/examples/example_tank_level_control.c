/**
 * @example example_tank_level_control.c
 * @brief Tank Level Control with PID ? L6 Canonical Problem
 *
 * A water tank with inlet flow controlled by PID and outlet flow
 * as a disturbance. This is the classic level control problem
 * demonstrating:
 *   - PID auto/manual modes
 *   - Setpoint tracking
 *   - Disturbance rejection
 *   - Output clamping
 */
#include <stdio.h>
#include <math.h>
#include "plantpax_control_loop.h"
#include "plantpax_io_subsystem.h"
#include "plantpax_alarm_manager.h"

#define SIMULATION_TIME_S  120.0
#define DT_S               0.5
#define TANK_AREA_M2       2.0
#define INLET_GAIN          0.02   /* m3/h per % CO */
#define OUTLET_GAIN         0.015  /* m3/h per sqrt(m level) */

int main(void)
{
    printf("===========================================\n");
    printf("  PlantPAx DCS ? Tank Level PID Control\n");
    printf("===========================================\n\n");

    /* --- Setup PID Loop --- */
    pax_pid_loop_t level_loop;
    pax_pid_loop_init(&level_loop, 1, "LIC101", PAX_LOOP_TYPE_SIMPLE_PID,
                       PAX_ACTION_DIRECT, 0.0, 100.0);

    /* Tune PID (Ziegler-Nichols based on process model) */
    pax_pid_params_init(&level_loop.params, PAX_PID_FORM_VELOCITY,
                         2.5, 30.0, 2.0, DT_S);

    level_loop.mode = PAX_LOOP_MODE_MANUAL;
    level_loop.output = 25.0;
    level_loop.setpoint = 5.0;

    /* --- Setup Level Alarm --- */
    pax_alarm_t high_level_alarm;
    pax_alarm_init(&high_level_alarm, 1, "LAH101", "High Level Alarm",
                    PAX_ALARM_TYPE_PROCESS, PAX_ALARM_PRIORITY_HIGH,
                    8.0, 0.5, 2.0, 2.0);

    /* --- Simulation State --- */
    double tank_level = 3.0;

    printf("%-8s %-10s %-10s %-10s %-8s %-10s\n",
           "Time(s)", "SP(m)", "PV(m)", "CO(%)", "Mode", "Alarm");
    printf("--------------------------------------------------------------\n");

    double t;
    for (t = 0.0; t <= SIMULATION_TIME_S; t += DT_S) {
        /* Setpoint changes */
        if (t >= 10.0 && t < 50.0) {
            level_loop.setpoint = 5.0;
        } else if (t >= 50.0 && t < 90.0) {
            level_loop.setpoint = 7.0;
        } else if (t >= 90.0) {
            level_loop.setpoint = 4.0;
        }

        /* Switch to auto at t=5s */
        if (t >= 5.0 && level_loop.mode == PAX_LOOP_MODE_MANUAL) {
            level_loop.mode = PAX_LOOP_MODE_AUTO;
            pax_pid_bumpless_transfer(&level_loop);
            printf("%-8.1f >>> Switching to AUTO mode\n", t);
        }

        /* Disturbance: outlet valve opens more at t=30s */
        double outlet_factor = 1.0;
        if (t >= 30.0 && t < 40.0) {
            outlet_factor = 1.5;  /* 50% more outflow */
        }
        if (t >= 70.0 && t < 80.0) {
            outlet_factor = 2.0;  /* 100% more outflow */
        }

        /* Execute PID */
        double co = pax_pid_execute(&level_loop, tank_level,
                                     level_loop.setpoint, DT_S);

        /* Tank dynamics */
        double inlet_flow = co * INLET_GAIN;
        double outlet_flow = OUTLET_GAIN * sqrt(tank_level) * outlet_factor;
        double dv_dt = (inlet_flow - outlet_flow) / TANK_AREA_M2;
        tank_level += dv_dt * DT_S;
        if (tank_level < 0.0) tank_level = 0.0;

        /* Evaluate alarm */
        pax_alarm_state_t alarm_state = pax_alarm_evaluate(
            &high_level_alarm, tank_level, DT_S * 1000.0, 0.0);

        const char *mode_str = (level_loop.mode == PAX_LOOP_MODE_AUTO)
                                ? "AUTO" : "MANUAL";
        const char *alarm_str = pax_alarm_state_name(alarm_state);

        printf("%-8.1f %-10.2f %-10.2f %-10.1f %-8s %-10s\n",
               t, level_loop.setpoint, tank_level, co, mode_str, alarm_str);
    }

    /* --- Performance Summary --- */
    double iae, ise, itae;
    pax_pid_performance_metrics(&level_loop, &iae, &ise, &itae);
    printf("\n--- Performance Metrics ---\n");
    printf("  IAE:  %.2f\n", iae);
    printf("  ISE:  %.2f\n", ise);
    printf("  ITAE: %.2f\n", itae);
    printf("  Cycles: %llu\n", (unsigned long long)level_loop.cycle_count);

    return 0;
}
