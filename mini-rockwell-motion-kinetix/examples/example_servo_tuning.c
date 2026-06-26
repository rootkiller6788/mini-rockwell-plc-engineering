/**
 * @file example_servo_tuning.c
 * @brief Example: Complete servo drive tuning and commissioning sequence.
 *
 * Demonstrates the Kinetix servo tuning workflow:
 *   1. Motor database selection
 *   2. Drive sizing and matching
 *   3. Auto-tuning (inertia identification + gain computation)
 *   4. Notch filter design for resonance suppression
 *   5. Friction compensation setup
 *   6. Performance evaluation (Monte Carlo robustness)
 *
 * This example covers:
 *   L6: Servo tuning (classic commissioning problem)
 *   L7: Kinetix drive selection, motor-drive matching
 *   L8: Notch filter auto-design, Monte Carlo robustness
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "axis_types.h"
#include "servo_tuning.h"

/* External declarations */
void motor_database_init_vpl_b1003f(motor_database_t *motor);
void motor_database_init_vpl_b1653f(motor_database_t *motor);
int kinetix_drive_lookup(uint16_t catalog_id, drive_power_rating_t *rating);
int kinetix_recommend_drive(const motor_database_t *motor,
                              double load_inertia_ratio, uint16_t *drive_catalog);
const char *kinetix_get_catalog_number(uint16_t catalog_id);
void monte_carlo_robustness(int n_trials, double nominal_inertia,
                              double nominal_resistance,
                              double kp_vel, double ki_vel,
                              double inertia_std_pct, uint64_t seed,
                              mc_summary_t *summary);

int main(void)
{
    printf("=== Kinetix Servo Tuning & Commissioning Example ===\n\n");

    /* Step 1: Select motor */
    printf("Step 1: Motor Selection\n");
    printf("  Selected: VPL-B1003F (Low Inertia, 240V)\n");
    motor_database_t motor;
    motor_database_init_vpl_b1003f(&motor);
    printf("  Catalog:   %s\n", motor.catalog_number);
    printf("  Torque:    %.2f N·m cont / %.2f N·m peak\n",
           motor.rated_torque_nm, motor.peak_torque_nm);
    printf("  Inertia:   %.2f kg·cm²\n", motor.rotor_inertia_kgcm2);
    printf("  Poles:     %d\n", motor.pole_pairs * 2);
    printf("  R/L:       %.2f Ω / %.2f mH\n", motor.resistance_ohms, motor.inductance_mh);

    /* Step 2: Match drive */
    printf("\nStep 2: Drive Sizing\n");
    double load_inertia_ratio = 3.0; /* Conveyor with 3:1 inertia ratio */
    printf("  Load inertia ratio: %.1f:1\n", load_inertia_ratio);

    uint16_t drive_id;
    int result = kinetix_recommend_drive(&motor, load_inertia_ratio, &drive_id);
    if (result != 0) {
        printf("  ERROR: No suitable drive found!\n");
        return 1;
    }
    printf("  Recommended drive: %s (ID=0x%04X)\n",
           kinetix_get_catalog_number(drive_id), drive_id);

    drive_power_rating_t rating;
    kinetix_drive_lookup(drive_id, &rating);
    printf("  Drive cont current: %.1f A (requires %.1f A) → OK\n",
           rating.output_current_cont, motor.rated_current_amps);
    printf("  Drive peak current: %.1f A (requires %.1f A) → OK\n",
           rating.output_current_peak, motor.peak_current_amps);

    /* Step 3: Auto-tune servo loops */
    printf("\nStep 3: Auto-Tuning\n");

    /* Initialize tuning */
    servo_tuning_t tuning;
    servo_tuning_init(&tuning, &motor);

    /* Simulate inertia identification */
    autotune_state_t autotune;
    memset(&autotune, 0, sizeof(autotune));

    /* Build synthetic ramp data for inertia ID */
    double vel_data[100], torq_data[100];
    double J_total = motor.rotor_inertia_kgcm2 * (1.0 + load_inertia_ratio);
    printf("  Actual total inertia: %.2f kg·cm²\n", J_total);

    for (int i = 0; i < 100; i++) {
        double t = 0.01 * (double)i;
        vel_data[i] = 200.0 * t; /* 200 rad/s² ramp */
        torq_data[i] = J_total * 1e-4 * 200.0; /* T = J * α */
    }

    double J_identified = autotune_identify_inertia(&autotune, torq_data,
                                                      vel_data, 100, 0.01);
    printf("  Identified inertia: %.2f kg·cm² (error: %.1f%%)\n",
           J_identified,
           fabs((J_identified - J_total) / J_total) * 100.0);

    /* Compute recommended gains */
    autotune.identified_J_kgcm2 = J_identified;
    autotune_compute_gains(&autotune, &motor);

    printf("\n  --- Recommended Servo Gains ---\n");
    printf("  Current Loop:  Kp=%.3f V/A, Ki=%.1f V/A·s\n",
           autotune.recommended_gains.current.kp_v_per_a,
           autotune.recommended_gains.current.ki_v_per_as);
    printf("  Velocity Loop: Kp=%.3f, Ki=%.3f\n",
           autotune.recommended_gains.velocity.kp_vel,
           autotune.recommended_gains.velocity.ki_vel);
    printf("  Position Loop: Kp=%.1f (%.1f Hz BW)\n",
           autotune.recommended_gains.position.kp_pos,
           autotune.estimated_pos_bw_hz);
    printf("  Bandwidth hierarchy: %d Hz (I) > %.0f Hz (V) > %.0f Hz (P)\n",
           2000,
           autotune.estimated_vel_bw_hz,
           autotune.estimated_pos_bw_hz);
    printf("  Phase margin: %.1f°, Gain margin: %.1f dB\n",
           autotune.estimated_phase_margin,
           autotune.estimated_gain_margin);

    /* Step 4: Notch filter for mechanical resonance */
    printf("\nStep 4: Notch Filter Design\n");

    /* Assume a resonance at 350 Hz */
    double resonance_hz = 350.0;
    printf("  Identified resonance: %.0f Hz\n", resonance_hz);

    notch_filter_t nf;
    int nf_result = notch_filter_design(&nf, resonance_hz, 0.01, 0.15, 0.00025);
    if (nf_result == 0) {
        printf("  Notch filter designed: f0=%.1f Hz, depth=%.1f dB, width=%.1f Hz\n",
               nf.center_freq_hz, nf.depth_db, nf.width_hz);

        /* Test: apply 350 Hz signal, verify attenuation */
        double unfiltered_sum = 0.0, filtered_sum = 0.0;
        for (int i = 0; i < 200; i++) {
            double t = 0.00025 * i;
            double signal = sin(2.0 * M_PI * resonance_hz * t);
            unfiltered_sum += fabs(signal);
            filtered_sum += fabs(notch_filter_apply(&nf, signal));
        }
        printf("  Attenuation: %.1f dB (%.1f%% of input passed)\n",
               20.0 * log10(filtered_sum / unfiltered_sum),
               100.0 * filtered_sum / unfiltered_sum);
    }

    /* Step 5: Friction compensation */
    printf("\nStep 5: Friction Compensation Setup\n");

    friction_comp_t fc;
    fc.enabled = true;
    fc.coulomb_nm = 0.1;
    fc.viscous_nm_per_krpm = 5.0;
    fc.stiction_nm = 0.15;
    fc.smooth_width = 0.5;

    printf("  Coulomb: %.2f N·m\n", fc.coulomb_nm);
    printf("  Viscous: %.1f N·m/kRPM\n", fc.viscous_nm_per_krpm);
    printf("  Stiction: %.2f N·m\n", fc.stiction_nm);

    /* Test compensation at various velocities */
    printf("  Friction feedforward:\n");
    double velocities[] = {0.0, 100.0, 500.0, 1000.0, -500.0};
    for (int i = 0; i < 5; i++) {
        double Tf = friction_compensation_compute(&fc, velocities[i]);
        printf("    ω=%.0f → Tf=%.4f N·m\n", velocities[i], Tf);
    }

    /* Step 6: Monte Carlo robustness evaluation */
    printf("\nStep 6: Monte Carlo Robustness Analysis\n");

    mc_summary_t mc;
    monte_carlo_robustness(1000,
                            J_total,
                            motor.resistance_ohms,
                            autotune.recommended_gains.velocity.kp_vel,
                            autotune.recommended_gains.velocity.ki_vel,
                            30.0,  /* ±30% inertia variation */
                            42,
                            &mc);

    printf("  Trials: %d\n", mc.total_trials);
    printf("  Settling time:  %.1f ± %.1f ms\n",
           mc.st_mean * 1000.0, mc.st_std * 1000.0);
    printf("  Overshoot:      %.1f ± %.1f %%\n", mc.os_mean, mc.os_std);
    printf("  IAE:            %.3f ± %.3f\n", mc.iae_mean, mc.iae_std);
    printf("  Instability:    %.1f%%  (%d trials)\n",
           mc.instability_pct, (int)(mc.instability_pct * mc.total_trials / 100.0));
    printf("  Worst settling: %.1f ms\n", mc.worst_st * 1000.0);

    if (mc.instability_pct < 1.0) {
        printf("  Result: ROBUST ✅ (instability < 1%%)\n");
    } else {
        printf("  Result: MARGINAL ⚠️ (consider reducing bandwidth)\n");
    }

    printf("\n=== Servo Tuning Complete ===\n");
    return 0;
}
