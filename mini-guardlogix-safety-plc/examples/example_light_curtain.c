/**
 * example_light_curtain.c -- Safety Light Curtain Integration
 *
 * L6 Canonical Problem: Presence-Sensing Safeguarding Device
 * L7 Application: GuardLogix + GuardShield 450L light curtain
 *
 * This example demonstrates integrating a safety light curtain
 * with GuardLogix. Light curtains use OSSD (Output Signal Switching
 * Device) outputs which are dual-channel complementary.
 *
 * OSSD1 = PNP output (active high)
 * OSSD2 = PNP output (active high)
 *
 * In normal operation (beam unblocked): OSSD1=1, OSSD2=1
 * (or complementary mode: OSSD1=1, OSSD2=0)
 *
 * When beam is interrupted: OSSD outputs go to OFF (0,0)
 * GuardLogix safety input detects this as a safety demand.
 *
 * Muting: Temporary suppression of the light curtain safety
 * function to allow material passage without stopping the machine.
 * Muting requires additional sensors (muting sensors) and is
 * time-limited per IEC 61496.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "guardlogix_safety.h"
#include "guardlogix_safety_io.h"
#include "guardlogix_safety_task.h"

/* Muting state machine states */
typedef enum {
    MUTING_IDLE      = 0,
    MUTING_ARMED     = 1,
    MUTING_ACTIVE    = 2,
    MUTING_OVERRIDE  = 3
} muting_state_t;

/* Muting configuration */
typedef struct {
    muting_state_t state;
    uint32_t max_muting_time_ms;
    uint32_t muting_start_time;
    uint8_t  muting_sensor_a;
    uint8_t  muting_sensor_b;
    uint8_t  override_active;
} muting_controller_t;

int main(void)
{
    printf("========================================\n");
    printf(" GuardLogix Light Curtain Safety Demo\n");
    printf("========================================\n\n");

    /* 1. Initialize controller */
    glx_safety_controller_t ctrl;
    glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 10);
    glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_LOCKED);
    printf("[OK] GuardLogix 5580S at 10ms safety task\n");

    /* 2. Light curtain input (complementary mode: OSSD1=1, OSSD2=0 normal) */
    glx_safety_input_point_t light_curtain;
    glx_safety_input_init(&light_curtain, GLX_INPUT_COMPLEMENTARY, 30, 1);

    /* 3. Machine output */
    glx_safety_output_point_t machine_output;
    glx_safety_output_init(&machine_output, GLX_OUTPUT_PULSE_BOTH, 500, 5000);

    /* 4. Muting sensors (separate inputs) */
    glx_safety_input_point_t muting_a, muting_b;
    glx_safety_input_init(&muting_a, GLX_INPUT_SINGLE_CHANNEL, 50, 1);
    glx_safety_input_init(&muting_b, GLX_INPUT_SINGLE_CHANNEL, 50, 1);

    /* 5. Muting controller */
    muting_controller_t muting;
    memset(&muting, 0, sizeof(muting));
    muting.state = MUTING_IDLE;
    muting.max_muting_time_ms = 60000; /* 60 seconds max muting */

    printf("[OK] Light curtain + muting sensors configured\n");

    /* 6. Simulation sequence */
    printf("\n--- Light Curtain Operation Sequence ---\n\n");

    /* Phase 1: Normal operation (beam clear) */
    printf("Phase 1: Normal operation (beam clear)\n");
    for (int i = 0; i < 3; i++) {
        glx_safety_input_update(&light_curtain, 1, 0, i * 10000);
        glx_safety_output_set(&machine_output, 1, 1);
        printf("  t=%dms: OSSD1=1 OSSD2=0 -> Machine RUNNING\n", i * 10);
    }

    /* Phase 2: Beam interrupted -> immediate stop */
    printf("\nPhase 2: Beam interrupted\n");
    glx_safety_input_update(&light_curtain, 0, 0, 30000);
    glx_safety_output_set(&machine_output, 0, 0);
    printf("  t=30ms: OSSD1=0 OSSD2=0 -> BEAM BROKEN -> Machine STOPPED\n");

    /* Phase 3: Muting sequence for material passage */
    printf("\nPhase 3: Muting sequence (material passage)\n");
    /* Muting sensor A activates first */
    glx_safety_input_update(&muting_a, 1, 1, 40000);
    printf("  t=40ms: Muting sensor A activated\n");
    muting.state = MUTING_ARMED;

    /* Muting sensor B activates */
    glx_safety_input_update(&muting_b, 1, 1, 42000);
    printf("  t=42ms: Muting sensor B activated\n");

    /* Both muting sensors active -> enter muting */
    if (muting.state == MUTING_ARMED) {
        muting.state = MUTING_ACTIVE;
        muting.muting_start_time = 42000;
        printf("  MUTING ACTIVE: Light curtain temporarily bypassed\n");
        /* During muting, machine can continue running */
        glx_safety_output_set(&machine_output, 1, 1);
    }

    /* Material passes through light curtain */
    glx_safety_input_update(&light_curtain, 0, 0, 45000);
    printf("  t=45ms: Material breaks beam (muting active) -> Machine continues\n");

    /* Material clears light curtain */
    glx_safety_input_update(&light_curtain, 1, 0, 47000);
    printf("  t=47ms: Material cleared beam\n");

    /* Muting sensors clear -> exit muting */
    glx_safety_input_update(&muting_a, 0, 0, 50000);
    glx_safety_input_update(&muting_b, 0, 0, 51000);
    muting.state = MUTING_IDLE;
    printf("  MUTING ENDED: Light curtain safety function restored\n");

    /* 7. Safety reaction time calculation */
    printf("\n--- Safety Performance ---\n");
    uint32_t srt = glx_compute_safety_reaction_time(&ctrl, 3, 10, 5);
    printf("  Safety Reaction Time (light curtain): %u ms\n", srt);
    printf("  Required for finger protection (<14mm): < 30ms\n");

    /* 8. Process Safety Time verification */
    glx_safety_task_cb_t tcb;
    glx_safety_task_init(&tcb, GLX_SAFETY_TASK_PERIODIC, 10000, 16);
    glx_safety_task_start_scan(&tcb, 0);
    tcb.last_record.input_copy_time_us = 100;
    tcb.last_record.logic_time_us = 50;
    tcb.last_record.output_copy_time_us = 100;
    tcb.last_record.cross_check_time_us = 50;
    glx_safety_task_end_scan(&tcb, 300);
    int timing_ok = glx_verify_task_timing_sil(&tcb, GLX_SIL_3, 30000);
    printf("  Task timing meets SIL 3 (< 2*PST)? %s\n",
           timing_ok ? "YES" : "NO");

    printf("\n=== Light Curtain Demo Complete ===\n");
    return 0;
}
