/**
 * example01_chassis_setup.c
 *
 * Studio 5000 / ControlLogix — Chassis and Controller Platform Setup
 *
 * Demonstrates:
 *   - 10-slot chassis initialization and configuration
 *   - Module installation (CPU, Ethernet, AI, AO, DI, DO)
 *   - Power budget verification
 *   - Controller type enumeration (1756-L7x/L8x series)
 *   - Redundancy pair availability calculation
 */

#include <stdio.h>
#include <stdlib.h>
#include "../include/control_logix_platform.h"
#include "../include/logix_redundancy.h"

int main(void)
{
    printf("=== Example 01: Chassis & Controller Setup ===\n\n");

    /* Step 1: Create a 10-slot standard chassis */
    logix_chassis_t chassis;
    if (!logix_chassis_init(&chassis, CHASSIS_10_SLOT, CHASSIS_TYPE_STANDARD)) {
        fprintf(stderr, "Failed to initialize chassis\n");
        return 1;
    }
    printf("Chassis: 10-slot Standard (1756-A10)\n");

    /* Step 2: Install modules */
    struct { int slot; uint32_t id; double bp, p5, p24; const char *desc; }
    mods[] = {
        {0, 0x1001, 5.0, 1.0, 0.0, "1756-L85E CPU"},
        {1, 0x2001, 3.5, 0.0, 0.0, "1756-EN2TR Ethernet"},
        {2, 0x3001, 0.5, 0.0, 0.0, "1756-IB32 DC Input"},
        {3, 0x4001, 0.8, 0.0, 0.0, "1756-OB32 DC Output"},
        {4, 0x4002, 3.5, 0.0, 0.0, "1756-IF16 Analog In"},
        {5, 0x4003, 3.0, 0.0, 0.0, "1756-OF8 Analog Out"},
        {6, 0x5001, 3.5, 0.0, 0.0, "1756-RM2 Redundancy"},
    };
    int nmods = sizeof(mods) / sizeof(mods[0]);

    for (int i = 0; i < nmods; i++) {
        if (!logix_chassis_install_module(&chassis, mods[i].slot,
                                          mods[i].id, mods[i].bp,
                                          mods[i].p5, mods[i].p24)) {
            fprintf(stderr, "Install fail: %s\n", mods[i].desc);
            return 1;
        }
        printf("  Slot %d: %s (%.1fW)\n", mods[i].slot, mods[i].desc, mods[i].bp);
    }

    /* Step 3: Power budget */
    if (!logix_chassis_verify_power_budget(&chassis)) {
        fprintf(stderr, "Power budget exceeded!\n");
        return 1;
    }
    printf("\nPower budget: OK (%.1fW total)\n", chassis.total_bp_power_w);

    /* Step 4: Controller catalog */
    printf("\n--- Controller Types ---\n");
    logix_controller_type_t types[] = {
        CTLR_1756_L71, CTLR_1756_L73, CTLR_1756_L75,
        CTLR_1756_L81E, CTLR_1756_L82E, CTLR_1756_L85E,
        CTLR_5069_L320ER, CTLR_5069_L340ERM
    };
    for (size_t i = 0; i < sizeof(types)/sizeof(types[0]); i++)
        printf("  %s\n", logix_controller_type_name(types[i]));

    /* Step 5: Redundancy availability */
    double avail = logix_redundancy_availability(100000.0, 8.0);
    printf("\n--- Redundancy ---\n");
    printf("  MTBF=100000h MTTR=8h\n");
    printf("  Availability: %.6f%%\n", avail * 100.0);
    printf("  Downtime/yr: %.2f min\n", (1.0-avail)*365.25*24*60);

    printf("\n=== Setup complete, %d slots occupied ===\n",
           logix_chassis_occupied_slot_count(&chassis));
    return 0;
}
