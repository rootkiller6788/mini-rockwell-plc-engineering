/**
 * @file example_fault_diagnostics.c
 * @brief End-to-end example: Fault detection, diagnostics, and system monitoring.
 *
 * Demonstrates:
 *   1. Complete system assembly (CompactLogix + Point I/O chassis)
 *   2. Connection establishment with RPI management
 *   3. Fault injection and recovery
 *   4. System diagnostics and health monitoring
 *   5. Connection health analysis
 *
 * This example simulates a real-world system with:
 *   - 1x CompactLogix L30ER controller
 *   - 1x 1734-AENT adapter
 *   - Various I/O modules
 *   - CIP I/O connections
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pointio_types.h"
#include "pointio_module.h"
#include "pointio_digital.h"
#include "pointio_analog.h"
#include "pointio_connection.h"
#include "pointio_diagnostics.h"

int main(void)
{
    printf("==============================================\n");
    printf("  Point I/O Fault Diagnostics Demo\n");
    printf("  System Health Monitoring\n");
    printf("==============================================\n\n");

    /* --- Step 1: CPU selection --- */
    compactlogix_cpu_t cpu;
    compactlogix_get_cpu_properties(CPU_1769_L30ER, &cpu);
    printf("[1] Controller: 1769-L30ER\n");
    printf("     User memory: %u KB\n", cpu.user_memory_kb);
    printf("     Max Ethernet nodes: %u\n", cpu.max_ethernet_nodes);
    printf("     Max local modules: %u\n\n", cpu.max_local_modules);

    /* --- Step 2: Build I/O chassis --- */
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);
    printf("[2] Building Point I/O chassis...\n");

    /* Add digital input modules */
    pointio_module_t di_mods[4];
    const char *di_names[] = {"DI_ValveFB", "DI_LimitSw", "DI_PB_Start", "DI_PB_Stop"};
    for (int i = 0; i < 4; i++) {
        pointio_module_config_digital_input(&di_mods[i], "1734-IB8", 8,
            POINTIO_FILTER_1MS, di_names[i]);
        di_mods[i].slot.slot = (uint8_t)(i + 1);
        pointio_chassis_add_module(&chassis, &di_mods[i]);
        printf("     Slot %d: %s (%s)\n", i + 1, di_names[i],
            di_mods[i].identity.catalog_number);
    }

    /* Add digital output modules */
    pointio_module_t do_mods[2];
    const char *do_names[] = {"DO_Valves", "DO_Motors"};
    for (int i = 0; i < 2; i++) {
        pointio_module_config_digital_output(&do_mods[i], "1734-OB8", 8,
            POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, do_names[i]);
        do_mods[i].slot.slot = (uint8_t)(i + 5);
        pointio_chassis_add_module(&chassis, &do_mods[i]);
        printf("     Slot %d: %s (%s)\n", i + 5, do_names[i],
            do_mods[i].identity.catalog_number);
    }

    /* Add analog input module */
    pointio_module_t ai_mod;
    pointio_analog_scaling_t scaling;
    memset(&scaling, 0, sizeof(scaling));
    scaling.eu_low = -40.0;
    scaling.eu_high = 150.0;
    scaling.raw_low = 3277.0;
    scaling.raw_high = 16383.0;
    scaling.range = POINTIO_AI_RANGE_4_20MA;
    pointio_module_config_analog_input(&ai_mod, "1734-IE4C", 4,
        POINTIO_AI_RANGE_4_20MA, &scaling, "AI_Temps");
    ai_mod.slot.slot = 7;
    pointio_chassis_add_module(&chassis, &ai_mod);
    printf("     Slot %d: AI_Temps (%s)\n", 7, ai_mod.identity.catalog_number);

    printf("     Total modules: %u\n\n", chassis.num_modules);

    /* --- Step 3: Capacity check --- */
    printf("[3] Capacity check:\n");
    int fits = compactlogix_check_capacity(&cpu,
        chassis.num_modules, chassis.num_modules + 1);
    printf("     %u modules (max %u): %s\n",
        chassis.num_modules, cpu.max_local_modules,
        fits ? "FITS" : "EXCEEDS");
    printf("     %u Ethernet nodes (max %u): %s\n\n",
        chassis.num_modules + 1, cpu.max_ethernet_nodes,
        fits ? "OK" : "EXCEEDS");

    /* --- Step 4: Power budget --- */
    printf("[4] Power budget analysis:\n");
    pointio_power_budget_t budget;
    pointio_calculate_power_budget(&chassis, &budget);
    printf("     Backplane 5V: %.0f / %.0f mA (%.0f%%)\n",
        budget.backplane_5v_used_ma, budget.backplane_5v_available_ma,
        100.0 * budget.backplane_5v_used_ma / budget.backplane_5v_available_ma);
    printf("     Field 24V:    %.0f / %.0f mA (%.0f%%)\n",
        budget.field_power_used_ma, budget.field_power_available_ma,
        budget.field_power_available_ma > 0 ?
            100.0 * budget.field_power_used_ma / budget.field_power_available_ma : 0);
    printf("     Total power:  %.2f / %.2f W\n",
        budget.total_power_used_w, budget.total_power_available_w);
    printf("     Status: %s\n\n", budget.overloaded ? "OVERLOADED" : "WITHIN LIMITS");

    /* --- Step 5: Establish CIP connections --- */
    printf("[5] Establishing CIP I/O connections...\n");
    pointio_connection_pool_t pool;
    pointio_conn_pool_init(&pool);

    /* Open connections for output modules */
    int conn_ids[2];
    for (int i = 0; i < 2; i++) {
        int rc = pointio_conn_open(&pool, POINTIO_CONN_EXCLUSIVE_OWNER,
            &do_mods[i], 20000, POINTIO_TRIGGER_CYCLIC, &conn_ids[i]);
        printf("     %s: conn_id=%d (%s)\n", do_names[i], conn_ids[i],
            rc == 0 ? "OK" : "FAILED");
    }

    /* Open listen-only connection for input module */
    int listen_conn;
    pointio_conn_open(&pool, POINTIO_CONN_LISTEN_ONLY,
        &di_mods[0], 20000, POINTIO_TRIGGER_CYCLIC, &listen_conn);
    pointio_conn_set_multicast(&pool, listen_conn, 1);
    printf("     %s: conn_id=%d (LISTEN ONLY, multicast)\n\n",
        di_names[0], listen_conn);

    /* --- Step 6: System diagnostics --- */
    printf("[6] System diagnostic scan:\n");
    pointio_system_diag_t diag;
    pointio_collect_system_diagnostics(&chassis, &diag);
    printf("     %s\n", diag.summary_string);

    /* Per-module diagnostics */
    for (int i = 1; i < chassis.num_modules && i < 10; i++) {
        pointio_module_t *mod = &chassis.modules[i];
        if (mod->status != POINTIO_STATUS_OK) continue;

        pointio_diagnostic_record_t record;
        pointio_collect_module_diagnostics(mod, &record);
        printf("     Slot %d [%s]: LED MOD=%s, NET=%s, Temp=%.1f degC\n",
            mod->slot.slot, mod->name,
            record.leds.mod_status_green_on ? "GREEN" :
            record.leds.mod_status_red_flash ? "RED_FLASH" : "OFF",
            record.leds.net_status_green_on ? "GREEN" : "OFF",
            record.module_temp_c);
    }
    printf("\n");

    /* --- Step 7: Simulate a fault --- */
    printf("[7] Injecting fault: module missing at slot 3\n");
    chassis.modules[3].status = POINTIO_STATUS_NOT_PRESENT;
    chassis.modules[3].type = POINTIO_TYPE_UNKNOWN;

    int reason;
    int is_missing = pointio_detect_module_missing(&chassis, 3, &reason);
    printf("     Module missing detected: %s (reason=%d)\n",
        is_missing ? "YES" : "NO", reason);

    /* Re-evaluate system health */
    pointio_collect_system_diagnostics(&chassis, &diag);
    printf("     Updated: %s\n\n", diag.summary_string);

    /* --- Step 8: Connection health check --- */
    printf("[8] Connection health analysis:\n");
    for (int i = 0; i < 2; i++) {
        int is_flapping;
        double health_pct;
        pointio_conn_health_check(&pool, conn_ids[i], &is_flapping, &health_pct);
        printf("     %s: health=%.1f%%, flapping=%s\n",
            do_names[i], health_pct, is_flapping ? "YES" : "NO");
    }
    printf("\n");

    /* --- Step 9: Self-test --- */
    printf("[9] Running module self-tests:\n");
    for (int i = 1; i < chassis.num_modules && i < 10; i++) {
        if (chassis.modules[i].status != POINTIO_STATUS_OK) continue;

        uint32_t results;
        int rc = pointio_module_self_test(&chassis.modules[i], &results);
        printf("     Slot %d [%s]: self-test %s (results=0x%02X)\n",
            chassis.modules[i].slot.slot, chassis.modules[i].name,
            rc == 0 ? "PASSED" : "FAILED", results);
    }
    printf("\n");

    /* --- Step 10: Cleanup --- */
    printf("[10] Cleanup - closing connections:\n");
    for (int i = 0; i < 2; i++) {
        pointio_conn_close(&pool, conn_ids[i]);
        printf("     Connection %d closed\n", conn_ids[i]);
    }
    pointio_conn_close(&pool, listen_conn);
    printf("     Listen-only connection %d closed\n", listen_conn);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
