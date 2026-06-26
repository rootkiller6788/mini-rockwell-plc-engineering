/**
 * @file pointio_diagnostics.h
 * @brief Module-level diagnostics, fault reporting, and status LED mapping.
 *
 * Level: L3 Engineering Structures + L6 Canonical Problems
 * Reference:
 *   - Rockwell, "CompactLogix 5370 Controllers User Manual" (1769-UM021), Ch.9
 *   - Rockwell, "Point I/O Diagnostic Guide" (1734-TD001)
 *   - ISA TR18.2.4 "Alarm System Monitoring, Management, and Auditing"
 */

#ifndef POINTIO_DIAGNOSTICS_H
#define POINTIO_DIAGNOSTICS_H

#include "pointio_types.h"
#include "pointio_module.h"

/*===========================================================================
 * L1: Diagnostic Data Structures
 *===========================================================================*/

/**
 * @brief LED status mapping for one module.
 *
 * Maps the physical LED indicators on Point I/O modules:
 *   - MOD STATUS (Module Status): Green/Red, solid/flashing
 *   - NET STATUS (Network Status): Green/Red, solid/flashing
 *   - I/O STATUS (per-point): Yellow for each I/O point
 */
typedef struct {
    int mod_status_green_on;
    int mod_status_green_flash;
    int mod_status_red_on;
    int mod_status_red_flash;
    int net_status_green_on;
    int net_status_green_flash;
    int net_status_red_on;
    int net_status_red_flash;
    uint32_t io_status_bitmask;     /* One bit per channel, 1=ON (yellow) */
} pointio_led_state_t;

/**
 * @brief Comprehensive diagnostic record for a module.
 */
typedef struct {
    uint64_t                timestamp_us;
    uint8_t                 slot;
    pointio_module_status_t status;
    pointio_fault_code_t    active_fault;
    pointio_fault_code_t    fault_history[16];
    uint8_t                 fault_history_index;
    uint32_t                fault_count;
    uint32_t                scan_count;
    uint32_t                last_scan_time_us;
    uint32_t                min_scan_time_us;
    uint32_t                max_scan_time_us;
    double                  avg_scan_time_us;
    pointio_led_state_t     leds;
    double                  module_temp_c;      /* Internal temperature */
    double                  backplane_voltage_v;
    double                  field_voltage_v;
    uint32_t                power_on_time_s;    /* Total uptime */
    int                     connection_healthy;
    double                  connection_quality_pct;
} pointio_diagnostic_record_t;

/**
 * @brief System-wide diagnostic summary.
 */
typedef struct {
    uint8_t     total_modules;
    uint8_t     ok_modules;
    uint8_t     faulted_modules;
    uint8_t     missing_modules;
    uint8_t     wrong_modules;
    double      system_health_pct;
    uint64_t    last_full_scan_us;
    char        summary_string[256];
} pointio_system_diag_t;

/*===========================================================================
 * L2: Fault Code to Human-Readable Mapping
 *===========================================================================*/

/**
 * @brief Convert a fault code to an English description.
 *
 * Maps internal fault code enumeration to the corresponding
 * Rockwell-defined fault description text.
 *
 * @param code  Fault code enum value
 * @return Static string (do not free)
 */
const char *pointio_fault_code_string(pointio_fault_code_t code);

/**
 * @brief Get recommended action for a fault code.
 *
 * @param code  Fault code enum
 * @return Static string with recovery procedure
 */
const char *pointio_fault_recovery_action(pointio_fault_code_t code);

/**
 * @brief Classify fault severity.
 *
 * @param code  Fault code
 * @return 0=informational, 1=minor (recoverable), 2=major (module down),
 *         3=critical (system safety)
 */
int pointio_fault_severity(pointio_fault_code_t code);

/*===========================================================================
 * L3: Module Diagnostics Collection
 *===========================================================================*/

/**
 * @brief Collect all diagnostic data for a single module.
 *
 * Reads status, fault codes, scan timing, LED states, and electrical
 * measurements into a structured record.
 *
 * @param mod    The module to diagnose
 * @param record Output: populated diagnostic record
 * @return 0 on success, -1 if mod is NULL
 */
int pointio_collect_module_diagnostics(const pointio_module_t *mod,
    pointio_diagnostic_record_t *record);

/**
 * @brief Collect system-wide diagnostic summary.
 *
 * Iterates all modules in the chassis and computes aggregate statistics.
 *
 * @param chassis The chassis to diagnose
 * @param diag    Output: system diagnostic summary
 * @return 0 on success
 */
int pointio_collect_system_diagnostics(const pointio_chassis_t *chassis,
    pointio_system_diag_t *diag);

/*===========================================================================
 * L4: LED State Determination
 *===========================================================================*/

/**
 * @brief Determine module LED states from operating status.
 *
 * Maps the module status enum to the corresponding LED indicator pattern
 * per Rockwell's standard Point I/O LED conventions.
 *
 * Reference: 1734-IN001, Table 3 "Status Indicator Interpretation"
 *
 * @param mod  The module
 * @param leds Output: LED state pattern
 * @return 0 on success
 */
int pointio_determine_led_states(const pointio_module_t *mod,
    pointio_led_state_t *leds);

/*===========================================================================
 * L6: Classic Diagnostic Scenarios
 *===========================================================================*/

/**
 * @brief Troubleshoot a module that is in FAULTED state.
 *
 * Analyzes the active fault and provides structured troubleshooting steps.
 * This is the classic "Module Faulted" diagnostic scenario from field service.
 *
 * @param mod          The faulted module
 * @param fault_desc   Output: fault description string
 * @param action_plan  Output: step-by-step recovery plan
 * @param is_recoverable Output: 1 if recoverable online, 0 if replacement needed
 * @return 0 on success
 */
int pointio_troubleshoot_faulted_module(const pointio_module_t *mod,
    const char **fault_desc, const char **action_plan, int *is_recoverable);

/**
 * @brief Run a complete module self-test.
 *
 * Simulates the module-level self-test performed at power-up and on-demand
 * via Studio 5000. Tests: RAM, ROM checksum, A/D converter, output driver,
 * communication interface, and terminal base presence.
 *
 * @param mod        The module to test
 * @param results    Output: bitmask of passed tests
 *                   Bit 0: RAM test
 *                   Bit 1: ROM checksum
 *                   Bit 2: A/D self-cal
 *                   Bit 3: Output driver test
 *                   Bit 4: Comm interface loopback
 *                   Bit 5: Terminal base detect
 * @return 0 if all tests pass, -1 if any critical test fails
 */
int pointio_module_self_test(pointio_module_t *mod, uint32_t *results);

/**
 * @brief Detect and diagnose a "module missing" condition.
 *
 * When a module that should be present is not detected on the backplane.
 * Checks: power, terminal base seating, backplane connector contamination.
 *
 * @param chassis      The chassis
 * @param expected_slot Slot where module should be
 * @param reason       Output: diagnostic reason code
 *                     0=not missing, 1=power loss, 2=unseated,
 *                     3=backplane fault, 4=wrong keying
 * @return 0 if module present, 1 if missing (reason populated), -1 on error
 */
int pointio_detect_module_missing(const pointio_chassis_t *chassis,
    uint8_t expected_slot, int *reason);

/*===========================================================================
 * L5: Scan Time Analysis
 *===========================================================================*/

/**
 * @brief Analyze I/O scan timing and detect jitter issues.
 *
 * For high-speed applications (motion, fast packaging), scan jitter
 * can cause control instability. This function computes statistics
 * and flags excessive jitter.
 *
 * @param mod           Module to analyze
 * @param max_jitter_us Output: maximum observed jitter
 * @param rms_jitter_us Output: RMS jitter
 * @param is_excessive  Output: 1 if jitter > 25% of RPI
 * @return 0 on success
 */
int pointio_analyze_scan_jitter(const pointio_module_t *mod,
    double *max_jitter_us, double *rms_jitter_us, int *is_excessive);

/*===========================================================================
 * L2: Reset & Recovery
 *===========================================================================*/

/**
 * @brief Attempt to reset/clear a module fault.
 *
 * For recoverable faults (communication timeout, minor errors),
 * this clears the fault and transitions the module back to OK status.
 * Non-recoverable faults (hardware damage) cannot be cleared.
 *
 * @param mod  The faulted module
 * @return 0 if fault cleared, 1 if fault persists, -1 on error
 */
int pointio_clear_module_fault(pointio_module_t *mod);

/**
 * @brief Inhibit/uninhibit a module (Studio 5000 equivalent).
 *
 * Inhibiting a module stops I/O scanning and sets outputs to safe state
 * without removing it from the I/O configuration tree.
 *
 * @param mod      The module
 * @param inhibit  1 = inhibit, 0 = uninhibit
 * @return 0 on success
 */
int pointio_set_module_inhibit(pointio_module_t *mod, int inhibit);

#endif /* POINTIO_DIAGNOSTICS_H */
