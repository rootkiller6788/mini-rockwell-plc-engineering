/**
 * @file example_digital_io_demo.c
 * @brief End-to-end example: Configuring and operating digital I/O.
 *
 * Demonstrates:
 *   1. Chassis assembly with mixed digital I/O modules
 *   2. Digital input reading with edge detection
 *   3. Digital output control with fault handling
 *   4. Power budget validation
 *
 * This example simulates a simple conveyor control system with:
 *   - 1x start pushbutton (DI, channel 0)
 *   - 1x stop pushbutton (DI, channel 1)
 *   - 1x photo-eye (DI, channel 2)
 *   - 1x conveyor motor contactor (DO, channel 0)
 *   - 1x indicator lamp (DO, channel 1)
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "pointio_types.h"
#include "pointio_module.h"
#include "pointio_digital.h"
#include "pointio_scanner.h"

int main(void)
{
    printf("==============================================\n");
    printf("  Point I/O Digital I/O Demo\n");
    printf("  Conveyor Control System\n");
    printf("==============================================\n\n");

    /* --- Step 1: Initialize chassis --- */
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);
    printf("[1] Chassis initialized: %s\n", chassis.chassis_name);

    /* --- Step 2: Add digital input module (1734-IB8) at slot 1 --- */
    pointio_module_t di_mod;
    pointio_module_config_digital_input(&di_mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "Conveyor_DI");
    di_mod.slot.slot = 1;
    pointio_chassis_add_module(&chassis, &di_mod);
    printf("[2] Added %s (8ch DI) at slot 1\n", di_mod.identity.catalog_number);

    /* --- Step 3: Add digital output module (1734-OB8) at slot 2 --- */
    pointio_module_t do_mod;
    pointio_module_config_digital_output(&do_mod, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "Conveyor_DO");
    do_mod.slot.slot = 2;
    pointio_chassis_add_module(&chassis, &do_mod);
    printf("[3] Added %s (8ch DO) at slot 2\n", do_mod.identity.catalog_number);

    /* --- Step 4: Calculate power budget --- */
    pointio_power_budget_t budget;
    pointio_calculate_power_budget(&chassis, &budget);
    printf("[4] Power budget:\n");
    printf("     Backplane 5V: %.0f/%.0f mA\n",
        budget.backplane_5v_used_ma, budget.backplane_5v_available_ma);
    printf("     Total power:  %.2f/%.2f W\n",
        budget.total_power_used_w, budget.total_power_available_w);
    printf("     Status: %s\n\n", budget.overloaded ? "OVERLOADED!" : "OK");

    /* --- Step 5: Simulate field inputs --- */
    pointio_module_t *di = &chassis.modules[1];
    pointio_module_t *do_ptr = &chassis.modules[2];

    printf("[5] Simulating field inputs...\n");

    /* Scenario A: Normal startup - start button pressed */
    di->input_data[0] = 0x01;  /* Channel 0 (Start) = ON */
    printf("  [A] Start button pressed: DI.0 = ON\n");

    pointio_di_state_t state;
    pointio_di_read_channel(di, 0, &state);
    printf("       DI.0 state: %s\n", state == POINTIO_DI_ON ? "ON" : "OFF");

    /* Conveyor ON */
    pointio_do_write_channel(do_ptr, 0, POINTIO_DO_ON);
    printf("       -> Conveyor motor (DO.0) = ON\n");

    /* Scenario B: Part detected at photo-eye */
    di->input_data[0] = 0x05;  /* Start held + Photo-eye active */
    printf("  [B] Part at photo-eye: DI.2 = ON\n");

    pointio_di_read_channel(di, 2, &state);
    printf("       DI.2 state: %s\n", state == POINTIO_DI_ON ? "BLOCKED" : "CLEAR");

    /* Indicator ON */
    pointio_do_write_channel(do_ptr, 1, POINTIO_DO_ON);
    printf("       -> Part present lamp (DO.1) = ON\n");

    /* Scenario C: Emergency stop */
    printf("  [C] Emergency stop activated!\n");

    /* Apply fault state to all outputs */
    int fault_channels = pointio_do_apply_fault_state(do_ptr);
    printf("       %d outputs set to fault state (safe)\n", fault_channels);
    printf("       All outputs de-energized per safety config\n\n");

    /* --- Step 6: Edge detection demo --- */
    printf("[6] Edge Detection Demo:\n");

    di->input_data[0] = 0x01;  /* Channel 0 transitions OFF->ON */
    pointio_di_process_input_image(di, di->input_data, 1);

    uint32_t prev_mask = 0x00;
    int edge;

    pointio_di_rising_edge(di, 0, &prev_mask, &edge);
    printf("  Rising edge on DI.0: %s\n", edge ? "DETECTED" : "no");
    pointio_di_rising_edge(di, 0, &prev_mask, &edge);
    printf("  Rising edge again:    %s (should be 'no')\n\n", edge ? "DETECTED" : "no");

    /* --- Step 7: Summary --- */
    printf("[7] System Summary:\n");
    printf("  Chassis: %s\n", chassis.chassis_name);
    printf("  Modules: %u configured\n", chassis.num_modules);
    printf("  DI Module: %s (slot %d, %d channels)\n",
        di->name, di->slot.slot, di->num_channels);
    printf("  DO Module: %s (slot %d, %d channels)\n",
        do_ptr->name, do_ptr->slot.slot, do_ptr->num_channels);
    printf("  Power: %.2f W used, %s\n",
        budget.total_power_used_w,
        budget.overloaded ? "OVERLOAD" : "within limits");

    printf("\n=== Demo Complete ===\n");
    return 0;
}
