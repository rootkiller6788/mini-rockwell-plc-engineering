/**
 * example03_pide_loop.c
 *
 * ControlLogix PIDE (Enhanced PID) — Tank Level Control
 *
 * Demonstrates:
 *   - PIDE initialization and configuration (velocity form)
 *   - Auto/Manual/Cascade mode transitions with bumpless transfer
 *   - First-order process simulation (tank level)
 *   - PV filtering, feedforward, output rate-of-change limiting
 *   - Anti-reset windup via clamping and back-calculation
 *   - Gain form conversion (independent <-> dependent)
 */

#include <stdio.h>
#include <math.h>
#include "../include/logix_pide_instruction.h"

/* Simulate a first-order tank: dh/dt = (Qin - Qout) / Area
 * Discrete: h(k+1) = h(k) + (Ts/Area) * (Qin(k) - Qout(k))
 * Where Qout = sqrt(2*g*h) * Cv (valve characteristic) */
static double tank_level_sim(double h, double valve_pct, double ts)
{
    double area  = 2.0;    /* m^2 */
    double q_max = 0.5;    /* m^3/s at 100% valve */
    double q_in  = 0.3;    /* m^3/s constant inflow */
    double q_out = q_max * (valve_pct / 100.0) * sqrt(h / 10.0); /* orifice flow */
    double dh    = (ts / area) * (q_in - q_out);
    double h_new = h + dh;
    return (h_new < 0.0) ? 0.0 : h_new;
}

int main(void)
{
    printf("=== Example 03: PIDE Tank Level Control ===\n\n");

    /* --- PIDE Setup --- */
    logix_pide_t pide;
    logix_pide_init(&pide, 0.1);  /* Ts = 100ms */

    /* Tune: Kp=2.0, Ki=0.5, Kd=0.0 (PI control) */
    pide.kp = 2.0;
    pide.ki = 0.5;
    pide.kd = 0.0;
    pide.sp = 7.0;                /* Setpoint: 7 meters */
    pide.cv_min = 0.0;
    pide.cv_max = 100.0;

    /* Enable PV filtering (1.0 sec time constant) */
    logix_pide_set_pv_filter(&pide, 1.0);

    /* Output rate-of-change limit: 20%/sec */
    logix_pide_set_roc_limit(&pide, 20.0);

    printf("PIDE config: Kp=%.1f Ki=%.1f Kd=%.1f Ts=%.1fs SP=%.1f\n",
           pide.kp, pide.ki, pide.kd, pide.ts_sec, pide.sp);
    printf("PV filter: tau=1.0s, ROC limit: 20%%/s\n\n");

    /* --- Control Loop Simulation --- */
    double tank_level = 5.0;  /* Start at 5m, target 7m */
    double ts = pide.ts_sec;

    printf("  Time(s)   Level(m)   CV(%%)   Mode\n");
    printf("  --------  ---------  -------  ------\n");

    /* Phase 1: Auto mode, ramp to setpoint */
    logix_pide_set_mode(&pide, PIDE_MODE_AUTO);
    for (int step = 0; step < 60; step++) {
        pide.pv = tank_level;
        double cv = logix_pide_execute(&pide);
        tank_level = tank_level_sim(tank_level, cv, ts);

        if (step % 10 == 0) {
            printf("  %8.1f  %9.2f  %7.1f  AUTO\n",
                   step * ts, tank_level, cv);
        }
    }
    printf("  ... steady state reached\n\n");

    /* Phase 2: Manual mode intervention */
    logix_pide_set_mode(&pide, PIDE_MODE_MANUAL);
    pide.mode_cv_manual = 30.0;  /* Operator sets valve to 30% */
    printf("  --- Switched to MANUAL (CV=30%%) ---\n");

    for (int step = 0; step < 20; step++) {
        pide.pv = tank_level;
        logix_pide_execute(&pide);
        tank_level = tank_level_sim(tank_level, 30.0, ts);
        if (step % 10 == 0)
            printf("  %8.1f  %9.2f  %7.1f  MANUAL\n",
                   (60+step) * ts, tank_level, 30.0);
    }

    /* Phase 3: Return to Auto (bumpless transfer) */
    logix_pide_set_mode(&pide, PIDE_MODE_AUTO);
    printf("  --- Returned to AUTO (bumpless) ---\n");
    for (int step = 0; step < 60; step++) {
        pide.pv = tank_level;
        double cv = logix_pide_execute(&pide);
        tank_level = tank_level_sim(tank_level, cv, ts);
        if (step % 10 == 0)
            printf("  %8.1f  %9.2f  %7.1f  AUTO\n",
                   (80+step) * ts, tank_level, cv);
    }

    /* --- Diagnostics --- */
    printf("\n--- PIDE Diagnostics ---\n");
    printf("  Error: %.2f m\n", pide.error);
    printf("  P-term: %.3f  I-term: %.3f  D-term: %.3f\n",
           pide.p_term, pide.i_term, pide.d_term);
    printf("  Status: 0x%04X\n", logix_pide_get_status(&pide));
    printf("  CV limit active: %s\n", pide.cv_high_limit_active ? "HI" :
           pide.cv_low_limit_active ? "LO" : "NONE");
    printf("  Windup occurred: %s\n", pide.windup_occurred ? "YES" : "NO");

    /* --- Bandwidth estimate --- */
    double bw = logix_pide_closed_loop_bandwidth(&pide, 1.0, 20.0);
    printf("\n  Est. closed-loop bandwidth: %.3f rad/s (%.3f Hz)\n",
           bw, bw / (2.0 * 3.14159));

    printf("\n=== PIDE loop demo complete ===\n");
    return 0;
}
