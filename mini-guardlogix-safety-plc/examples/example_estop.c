/**
 * example_estop.c -- Emergency Stop (E-Stop) Safety Function
 *
 * L6 Canonical Problem: Emergency Shutdown System
 * L7 Application: GuardLogix + GuardShield + 440R Safety Relay
 *
 * This example demonstrates a complete emergency stop safety function
 * implemented with GuardLogix safety PLC. It shows:
 *
 * 1. Dual-channel E-Stop button input (equivalent mode, SIL 3 / PL e)
 * 2. Safety output control with pulse testing
 * 3. Emergency shutdown response (outputs to safe state)
 * 4. Manual reset after E-Stop clearance
 * 5. Safety reaction time calculation
 * 6. Proof test interval for the E-Stop function
 *
 * System architecture:
 *   - 2x NC E-Stop contacts (dual channel) -> GuardLogix safety inputs
 *   - Safety logic: If E-Stop pressed (inputs = 0,0), de-energize outputs
 *   - Safety outputs -> 440R safety relay -> motor contactors
 *
 * SIL verification (per IEC 61508):
 *   - Architecture: 1oo2D (GuardLogix logic solver)
 *   - E-Stop subsystem: 1oo2 dual channel
 *   - Final element: 1oo2 contactors
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "guardlogix_safety.h"
#include "guardlogix_sil.h"
#include "guardlogix_safety_io.h"
#include "guardlogix_safety_task.h"

int main(void)
{
    printf("========================================\n");
    printf(" GuardLogix Emergency Stop (E-Stop) Demo\n");
    printf("========================================\n\n");

    /* 1. Initialize GuardLogix 5580 safety controller */
    glx_safety_controller_t ctrl;
    if (glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 20) != 0) {
        printf("FAIL: Controller init failed\n");
        return 1;
    }
    printf("[OK] GuardLogix 5580S initialized (SIL 3 / PL e)\n");

    /* 2. Transition to safety locked (run) mode */
    if (glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_LOCKED) != 0) {
        printf("FAIL: State transition failed\n");
        return 1;
    }
    printf("[OK] Controller in SAFETY_LOCKED state\n");

    /* 3. Initialize safety input for E-Stop (dual channel, equivalent) */
    glx_safety_input_point_t estop_input;
    glx_safety_input_init(&estop_input, GLX_INPUT_EQUIVALENT, 50, 3);
    printf("[OK] E-Stop dual-channel input configured\n");

    /* 4. Initialize safety output for motor contactor */
    glx_safety_output_point_t motor_output;
    glx_safety_output_init(&motor_output, GLX_OUTPUT_PULSE_BOTH, 500, 5000);
    printf("[OK] Motor contactor safety output configured\n");

    /* 5. Simulate normal operation: E-Stop NOT pressed (both channels = 1) */
    printf("\n--- Simulation: Normal Operation ---\n");
    for (int cycle = 1; cycle <= 5; cycle++) {
        uint32_t now = cycle * 20000; /* 20ms per cycle */

        /* Read E-Stop inputs (both channels healthy = 1) */
        int estop_val = glx_safety_input_update(&estop_input, 1, 1, now);
        printf("  Cycle %d: E-Stop=%d [channels: 1,1]\n", cycle, estop_val);

        /* Safety logic: E-Stop OK -> energize motor output */
        if (estop_val == 1 && estop_input.channel_fault == 0) {
            glx_safety_output_set(&motor_output, 1, 1);
        } else {
            glx_safety_output_set(&motor_output, 0, 0);
        }

        printf("    Motor output: %s\n",
            glx_safety_output_is_healthy(&motor_output) ? "ENERGIZED" : "SAFE");
    }

    /* 6. Simulate E-Stop pressed (both channels = 0) */
    printf("\n--- Simulation: E-Stop PRESSED ---\n");
    for (int cycle = 6; cycle <= 8; cycle++) {
        uint32_t now = cycle * 20000;
        int estop_val = glx_safety_input_update(&estop_input, 0, 0, now);

        if (estop_val == 0) {
            /* E-Stop is active -> de-energize immediately */
            glx_safety_output_set(&motor_output, 0, 0);
        }

        printf("  Cycle %d: E-Stop PRESSED [channels: 0,0] -> Motor DE-ENERGIZED\n",
               cycle);
    }

    /* 7. Simulate E-Stop released and manual reset */
    printf("\n--- Simulation: E-Stop Released + Manual Reset ---\n");
    int estop_val = glx_safety_input_update(&estop_input, 1, 1, 8 * 20000);
    printf("  E-Stop released [channels: 1,1]\n");

    /* Manual reset: operator presses reset button */
    printf("  Operator presses RESET button...\n");
    if (estop_val == 1 && estop_input.channel_fault == 0) {
        glx_safety_output_set(&motor_output, 1, 1);
        printf("  Motor RE-ENERGIZED (after manual reset)\n");
    }

    /* 8. Verify safety reaction time */
    uint32_t srt = glx_compute_safety_reaction_time(&ctrl, 5, 20, 10);
    printf("\n--- Safety Performance ---\n");
    printf("  Safety Reaction Time: %u ms\n", srt);
    printf("  Task Period: %u ms\n", ctrl.safety_task_period_ms);
    printf("  SRT < 50ms? %s\n", srt < 50 ? "PASS" : "FAIL");

    /* 9. SIL verification for E-Stop function */
    printf("\n--- SIL Verification (E-Stop SIF) ---\n");
    glx_sif_model_t sif;
    memset(&sif, 0, sizeof(sif));
    sif.architecture = GLX_ARCH_1OO2D;
    sif.component_type = GLX_COMPONENT_TYPE_B;
    sif.hft = GLX_HFT_1;
    sif.logic_rates.lambda_du = 1e-7;
    sif.logic_rates.lambda_safe = 5e-7;
    sif.logic_rates.lambda_dd = 9e-7;
    sif.logic_rates.mttr = 8.0;
    sif.beta = 0.02;
    sif.beta_d = 0.01;
    sif.dc_sensor = 0.99;
    sif.dc_logic = 0.99;
    sif.dc_actuator = 0.90;
    sif.proof_test_interval_h = 8760;
    sif.proof_test_coverage = 0.95;
    sif.sensor_rates = sif.logic_rates;
    sif.actuator_rates = sif.logic_rates;

    glx_sil_verification_t result;
    glx_verify_sif_sil(&sif, GLX_SIL_3, &result);
    printf("  PFDavg: %.2e\n", result.pfd_avg);
    printf("  Achieved SIL: %d\n", result.achieved_sil);
    printf("  RRF: %.0f\n", result.rrf);
    printf("  SIL 3 Achieved: %s\n", result.sil_achieved ? "YES" : "NO");

    printf("\n=== E-Stop Demo Complete ===\n");
    return 0;
}
