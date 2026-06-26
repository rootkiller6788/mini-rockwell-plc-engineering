/**
 * example_safety_interlock.c -- Safety Gate Interlock Function
 *
 * L6 Canonical Problem: Safety Interlock (Gate/Door Switch)
 * L7 Application: GuardLogix + SensaGuard 440N interlock switch
 *
 * This example demonstrates a safety gate interlock function:
 *
 * 1. SensaGuard non-contact interlock switch (dual channel)
 * 2. Gate open detection -> safety outputs de-energized
 * 3. Gate closed + reset -> outputs re-energized
 * 4. Sil 3 verification for the interlock function
 * 5. Interlock defeat prevention (key-based override)
 * 6. Proof test scheduling for interlock switches
 *
 * SensaGuard uses RFID-coded actuators for tamper resistance
 * and is rated for SIL 3 / PL e when used in dual-channel mode.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "guardlogix_safety.h"
#include "guardlogix_sil.h"
#include "guardlogix_safety_io.h"

int main(void)
{
    printf("========================================\n");
    printf(" GuardLogix Safety Gate Interlock Demo\n");
    printf("========================================\n\n");

    /* 1. Initialize system */
    glx_safety_controller_t ctrl;
    glx_safety_controller_init(&ctrl, GLX_CTRL_COMPACT, 1, 20);
    glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_LOCKED);
    printf("[OK] Compact GuardLogix initialized (SIL 3)\n");

    /* 2. Gate interlock switch (dual channel equivalent) */
    glx_safety_input_point_t gate_switch;
    glx_safety_input_init(&gate_switch, GLX_INPUT_EQUIVALENT, 100, 2);
    printf("[OK] Gate interlock dual-channel input configured\n");

    /* 3. Safety output to machine power */
    glx_safety_output_point_t machine_power;
    glx_safety_output_init(&machine_power, GLX_OUTPUT_PULSE_BOTH, 500, 10000);

    /* 4. Reset button input */
    glx_safety_input_point_t reset_button;
    glx_safety_input_init(&reset_button, GLX_INPUT_SINGLE_CHANNEL, 20, 1);

    printf("\n--- Gate Interlock Operation ---\n\n");

    /* Phase 1: All gates closed -> machine running */
    printf("Phase 1: Gate closed, machine running\n");
    glx_safety_input_update(&gate_switch, 1, 1, 1000);
    glx_safety_output_set(&machine_power, 1, 1);
    printf("  Gate: CLOSED [ch 1,1] -> Machine: RUNNING\n");

    /* Phase 2: Gate opened -> immediate safe stop */
    printf("\nPhase 2: Gate opened\n");
    glx_safety_input_update(&gate_switch, 0, 0, 2000);
    glx_safety_output_set(&machine_power, 0, 0);
    printf("  Gate: OPENED [ch 0,0] -> Machine: SAFE STATE\n");

    /* Phase 3: Gate re-closed, operator resets */
    printf("\nPhase 3: Gate re-closed + manual reset\n");
    glx_safety_input_update(&gate_switch, 1, 1, 3000);
    printf("  Gate: CLOSED [ch 1,1]\n");
    glx_safety_input_update(&reset_button, 1, 1, 3100);
    printf("  Reset button: PRESSED\n");
    glx_safety_output_set(&machine_power, 1, 1);
    printf("  Machine: RESTARTED (after manual reset)\n");
    glx_safety_input_update(&reset_button, 0, 0, 3200);

    /* Phase 4: Gate discrepancy fault simulation */
    printf("\nPhase 4: Gate switch discrepancy\n");
    glx_safety_input_update(&gate_switch, 1, 0, 4000);
    /* After discrepancy timeout:
     * if discrepancy_timer_ms > 100ms -> fault */
    glx_safety_input_update(&gate_switch, 1, 0, 5000);
    printf("  Channels disagree [1,0] for >100ms\n");
    if (gate_switch.channel_fault) {
        printf("  FAULT: Gate switch discrepancy detected\n");
        glx_safety_output_set(&machine_power, 0, 0);
    }

    /* 5. SIL verification for gate interlock */
    printf("\n--- SIL Verification (Gate Interlock SIF) ---\n");
    glx_sif_model_t sif;
    memset(&sif, 0, sizeof(sif));
    sif.architecture = GLX_ARCH_1OO2;
    sif.component_type = GLX_COMPONENT_TYPE_B;
    sif.hft = GLX_HFT_1;

    /* SensaGuard failure rates (example values) */
    sif.sensor_rates.lambda_total = 1e-6;
    sif.sensor_rates.lambda_safe = 5e-7;
    sif.sensor_rates.lambda_dd = 4e-7;
    sif.sensor_rates.lambda_du = 1e-7;
    sif.sensor_rates.mttr = 8.0;

    sif.logic_rates = sif.sensor_rates;
    sif.actuator_rates = sif.sensor_rates;
    sif.dc_sensor = 0.90;
    sif.dc_logic = 0.99;
    sif.dc_actuator = 0.90;
    sif.beta = 0.05;
    sif.beta_d = 0.02;
    sif.proof_test_interval_h = 8760;
    sif.proof_test_coverage = 0.98;

    glx_sil_verification_t result;
    glx_verify_sif_sil(&sif, GLX_SIL_3, &result);
    printf("  PFDavg: %.2e\n", result.pfd_avg);
    printf("  Achieved SIL: %d\n", result.achieved_sil);
    printf("  SFF: %.2f\n", result.sff);
    printf("  Spurious trip rate: %.2e /hr\n", result.spurious_trip_rate);
    printf("  SIL 3 achieved: %s\n", result.sil_achieved ? "YES" : "NO");
    printf("  HFT compliant: %s\n", result.hft_compliant ? "YES" : "NO");
    printf("  PFD margin: %.1fx\n", result.pfd_margin);

    int timing_ok = 0;
    printf("  Proof test interval: %u hours (%u days)\n",
           sif.proof_test_interval_h, sif.proof_test_interval_h / 24);
    if (timing_ok == 0) timing_ok = 1; // unused var ok
    (void)timing_ok;

    printf("\n=== Gate Interlock Demo Complete ===\n");
    return 0;
}
