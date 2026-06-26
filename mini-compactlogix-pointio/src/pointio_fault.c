/**
 * @file pointio_fault.c
 * @brief Fault detection, handling, and recovery for Point I/O modules.
 *
 * Level: L6 Canonical Problems - fault management is a core industrial problem
 *
 * Reference:
 *   - Rockwell "Point I/O Diagnostic Guide" (1734-TD001)
 *   - IEC 61508 "Functional Safety of E/E/PE Systems"
 *   - Rockwell "CompactLogix 5370 Controllers User Manual" (1769-UM021) Ch.9
 *
 * Implements the complete fault lifecycle:
 *   1. Fault Detection - recognize when a module has faulted
 *   2. Fault Classification - determine severity and recoverability
 *   3. Fault Response - apply safe state transitions
 *   4. Fault Recovery - attempt to clear faults and restore operation
 *   5. Fault Logging - maintain fault history for diagnostics
 */

#include "pointio_diagnostics.h"
#include "pointio_module.h"
#include "pointio_types.h"
#include "pointio_digital.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * L2: Fault Code Descriptions
 *
 * Maps each fault code to the standard Rockwell-defined description
 * and recommended recovery action per 1734-TD001.
 *===========================================================================*/

/**
 * Human-readable fault code descriptions.
 */
const char *pointio_fault_code_string(pointio_fault_code_t code)
{
    switch (code) {
    case POINTIO_FAULT_NONE:
        return "No fault";
    case POINTIO_FAULT_CONN_TIMEOUT:
        return "Connection timeout - I/O data not received within timeout period";
    case POINTIO_FAULT_MODULE_MISSING:
        return "Module missing - expected module not detected on backplane";
    case POINTIO_FAULT_CONFIG_MISMATCH:
        return "Configuration mismatch - electronic keying failed";
    case POINTIO_FAULT_COMM_ERROR:
        return "Communication error - CRC or framing error on PointBus";
    case POINTIO_FAULT_FIELD_POWER_LOST:
        return "Field power lost - 24V field power absent or below threshold";
    case POINTIO_FAULT_BACKPLANE_ERROR:
        return "Backplane error - PointBus communication failure";
    case POINTIO_FAULT_OVERTEMP:
        return "Over temperature - module internal temperature exceeded limit";
    case POINTIO_FAULT_OVERVOLTAGE:
        return "Over voltage - field power exceeds maximum rating";
    case POINTIO_FAULT_UNDERVOLTAGE:
        return "Under voltage - system voltage below minimum operating level";
    case POINTIO_FAULT_SHORT_CIRCUIT:
        return "Short circuit - output channel overload or wiring fault";
    case POINTIO_FAULT_OPEN_WIRE:
        return "Open wire - sensor or actuator disconnected";
    case POINTIO_FAULT_OUT_OF_RANGE:
        return "Out of range - signal exceeds configured engineering range";
    case POINTIO_FAULT_CALIBRATION_LOST:
        return "Calibration lost - module requires recalibration";
    case POINTIO_FAULT_FIRMWARE_UPDATE_REQUIRED:
        return "Firmware update required - incompatible firmware revision";
    case POINTIO_FAULT_INTERNAL_ERROR:
        return "Internal error - module self-test failure";
    case POINTIO_FAULT_WATCHDOG_TIMEOUT:
        return "Watchdog timeout - module processor watchdog expired";
    default:
        return "Unknown fault code";
    }
}

/**
 * Recommended recovery actions for each fault.
 */
const char *pointio_fault_recovery_action(pointio_fault_code_t code)
{
    switch (code) {
    case POINTIO_FAULT_NONE:
        return "No action required";
    case POINTIO_FAULT_CONN_TIMEOUT:
        return "1. Check Ethernet cable connection\n"
               "2. Verify RPI setting is achievable\n"
               "3. Check network switch for errors\n"
               "4. Increase RPI or timeout multiplier";
    case POINTIO_FAULT_MODULE_MISSING:
        return "1. Verify module is physically installed\n"
               "2. Check terminal base seating\n"
               "3. Verify electronic keying matches\n"
               "4. Replace module if persistent";
    case POINTIO_FAULT_CONFIG_MISMATCH:
        return "1. Check Studio 5000 I/O configuration\n"
               "2. Verify catalog number matches installed module\n"
               "3. Check electronic keying settings\n"
               "4. Inhibit and re-enable module";
    case POINTIO_FAULT_COMM_ERROR:
        return "1. Check PointBus backplane connections\n"
               "2. Remove and reseat adjacent modules\n"
               "3. Power cycle the Point I/O system\n"
               "4. Replace adapter if persistent";
    case POINTIO_FAULT_FIELD_POWER_LOST:
        return "1. Check field power supply (24V DC)\n"
               "2. Verify field power wiring and fusing\n"
               "3. Check for short circuits on field terminals\n"
               "4. Replace field power distribution module";
    case POINTIO_FAULT_BACKPLANE_ERROR:
        return "1. Remove all modules and reinstall\n"
               "2. Check for bent backplane pins\n"
               "3. Replace adapter (1734-AENT)\n"
               "4. Contact Rockwell technical support";
    case POINTIO_FAULT_OVERTEMP:
        return "1. Check ambient temperature (< 60C)\n"
               "2. Verify adequate ventilation\n"
               "3. Check for blocked air vents\n"
               "4. Reduce module density";
    case POINTIO_FAULT_OVERVOLTAGE:
        return "1. Verify field power voltage (24V ±20%)\n"
               "2. Check for voltage spikes on supply\n"
               "3. Install transient voltage suppression\n"
               "4. Replace field power supply";
    case POINTIO_FAULT_UNDERVOLTAGE:
        return "1. Verify system power supply output\n"
               "2. Check for excessive voltage drop on cables\n"
               "3. Verify power supply is correctly sized\n"
               "4. Check for partial short circuit";
    case POINTIO_FAULT_SHORT_CIRCUIT:
        return "1. Disconnect field wiring from module\n"
               "2. Check resistance between output and common\n"
               "3. Test field device for internal short\n"
               "4. Replace output channel fuse if applicable";
    case POINTIO_FAULT_OPEN_WIRE:
        return "1. Check sensor wiring continuity\n"
               "2. Verify terminal block connections are tight\n"
               "3. Check sensor for internal open circuit\n"
               "4. For 4-20mA: check loop power is present";
    case POINTIO_FAULT_OUT_OF_RANGE:
        return "1. Verify sensor is appropriate for range\n"
               "2. Check scaling configuration in Studio 5000\n"
               "3. Verify sensor calibration\n"
               "4. Check for process upset condition";
    case POINTIO_FAULT_CALIBRATION_LOST:
        return "1. Perform module calibration\n"
               "2. Verify calibration reference is accurate\n"
               "3. Replace module if calibration fails repeatedly";
    case POINTIO_FAULT_FIRMWARE_UPDATE_REQUIRED:
        return "1. Use ControlFLASH to update module firmware\n"
               "2. Verify firmware revision compatibility\n"
               "3. Contact Rockwell for latest firmware";
    case POINTIO_FAULT_INTERNAL_ERROR:
        return "1. Power cycle the module\n"
               "2. Remove and reinstall module\n"
               "3. Replace module if error persists\n"
               "4. Contact Rockwell for RMA";
    case POINTIO_FAULT_WATCHDOG_TIMEOUT:
        return "1. Module processor reset required\n"
               "2. Power cycle the Point I/O system\n"
               "3. Update module firmware\n"
               "4. Replace module if watchdog persists";
    default:
        return "Contact Rockwell Automation technical support";
    }
}

/**
 * Classify fault severity.
 *
 * Severity levels per ISA-18.2 / IEC 62682:
 *   0 = Informational (no action required)
 *   1 = Minor (degraded but operational, field-recoverable)
 *   2 = Major (module non-functional, may require replacement)
 *   3 = Critical (system safety impact, immediate action required)
 */
int pointio_fault_severity(pointio_fault_code_t code)
{
    switch (code) {
    case POINTIO_FAULT_NONE:
        return 0;
    case POINTIO_FAULT_CONN_TIMEOUT:
        return 1;  /* Minor - typically recovers on next packet */
    case POINTIO_FAULT_OUT_OF_RANGE:
        return 1;  /* Minor - process upset, not module fault */
    case POINTIO_FAULT_CONFIG_MISMATCH:
        return 2;  /* Major - module cannot operate */
    case POINTIO_FAULT_MODULE_MISSING:
        return 2;  /* Major - entire slot unavailable */
    case POINTIO_FAULT_COMM_ERROR:
        return 2;  /* Major - data integrity compromised */
    case POINTIO_FAULT_FIELD_POWER_LOST:
        return 2;  /* Major - all field devices offline */
    case POINTIO_FAULT_BACKPLANE_ERROR:
        return 3;  /* Critical - entire backplane may be down */
    case POINTIO_FAULT_OVERTEMP:
        return 2;  /* Major - module may shut down */
    case POINTIO_FAULT_OVERVOLTAGE:
        return 3;  /* Critical - potential hardware damage */
    case POINTIO_FAULT_UNDERVOLTAGE:
        return 2;  /* Major */
    case POINTIO_FAULT_SHORT_CIRCUIT:
        return 2;  /* Major - output channel damaged */
    case POINTIO_FAULT_OPEN_WIRE:
        return 1;  /* Minor - single channel affected */
    case POINTIO_FAULT_CALIBRATION_LOST:
        return 2;  /* Major - measurement accuracy lost */
    case POINTIO_FAULT_FIRMWARE_UPDATE_REQUIRED:
        return 1;  /* Minor - operates with old firmware */
    case POINTIO_FAULT_INTERNAL_ERROR:
        return 3;  /* Critical - module may be unreliable */
    case POINTIO_FAULT_WATCHDOG_TIMEOUT:
        return 3;  /* Critical - module processor may be hung */
    default:
        return 1;
    }
}

/*===========================================================================
 * L3: Module Diagnostics Collection
 *===========================================================================*/

/**
 * Collect all diagnostic data for a single module.
 *
 * Populates a comprehensive diagnostic record including:
 *   - Operating status and active fault
 *   - Fault history (rotating buffer of last 16 faults)
 *   - Scan timing statistics
 *   - LED indicator states
 *   - Electrical measurements
 *
 * Time complexity: O(1) per module
 */
int pointio_collect_module_diagnostics(const pointio_module_t *mod,
    pointio_diagnostic_record_t *record)
{
    if (!mod || !record) return -1;

    memset(record, 0, sizeof(pointio_diagnostic_record_t));

    record->slot = mod->slot.slot;
    record->status = mod->status;
    record->active_fault = mod->active_fault;
    record->scan_count = mod->scan_count;

    /* LED state determination from module status */
    pointio_determine_led_states(mod, &record->leds);

    /* Simulated electrical measurements */
    record->module_temp_c = 35.0;  /* Typical operating temperature */
    record->backplane_voltage_v = POINTIO_BACKPLANE_VOLTAGE;  /* 5.0V nominal */
    record->field_voltage_v = 24.0;  /* 24VDC nominal */
    record->power_on_time_s = mod->scan_count * (mod->rpi_us / 1000000);

    /* Connection health */
    record->connection_healthy = (mod->conn_state == POINTIO_CONN_STATE_RUNNING) ? 1 : 0;
    record->connection_quality_pct = 100.0;

    /* Fault history is maintained separately - copy last entry */
    /* (Full history management requires persistent storage) */

    return 0;
}

/**
 * Collect system-wide diagnostic summary.
 *
 * Counts modules by status category and computes overall health percentage.
 * A system is considered healthy when all configured modules are OK.
 */
int pointio_collect_system_diagnostics(const pointio_chassis_t *chassis,
    pointio_system_diag_t *diag)
{
    if (!chassis || !diag) return -1;

    memset(diag, 0, sizeof(pointio_system_diag_t));

    diag->total_modules = chassis->num_modules;

    for (int i = 0; i < POINTIO_MAX_MODULES; i++) {
        const pointio_module_t *mod = &chassis->modules[i];

        switch (mod->status) {
        case POINTIO_STATUS_OK:
        case POINTIO_STATUS_STANDBY:
            diag->ok_modules++;
            break;
        case POINTIO_STATUS_FAULTED:
        case POINTIO_STATUS_MAJOR_FAULT:
            diag->faulted_modules++;
            break;
        case POINTIO_STATUS_NOT_PRESENT:
            /* Not counted as missing unless it was configured */
            break;
        case POINTIO_STATUS_WRONG_MODULE:
            diag->wrong_modules++;
            break;
        default:
            break;
        }
    }

    /* System health: percentage of modules that are OK */
    if (diag->total_modules > 0) {
        diag->system_health_pct = 100.0 * (double)diag->ok_modules /
                                  (double)diag->total_modules;
    } else {
        diag->system_health_pct = 0.0;
    }

    /* Build summary string */
    snprintf(diag->summary_string, sizeof(diag->summary_string),
        "System: %u total, %u OK, %u faulted, %u wrong. Health: %.1f%%",
        diag->total_modules, diag->ok_modules,
        diag->faulted_modules, diag->wrong_modules,
        diag->system_health_pct);

    return 0;
}

/*===========================================================================
 * L4: LED State Determination
 *
 * Standard Point I/O LED conventions per 1734-IN001 Table 3:
 *
 * Module Status (MOD):
 *   OFF        = No power
 *   Solid Green   = Normal operation
 *   Flashing Green = Standby (not configured)
 *   Flashing Red   = Recoverable fault
 *   Solid Red      = Unrecoverable fault
 *   Flashing Yellow/Green = Self-test
 *
 * Network Status (NET):
 *   OFF        = No IP address / not powered
 *   Flashing Green = No CIP connections established
 *   Solid Green   = At least one CIP connection established
 *   Flashing Red   = One or more connections timed out
 *   Solid Red      = Duplicate IP detected
 *   Flashing Yellow/Green = Self-test
 *
 * I/O Status (per-point):
 *   Yellow ON  = Input/output active (ON/energized)
 *   Yellow OFF = Input/output inactive
 *===========================================================================*/

int pointio_determine_led_states(const pointio_module_t *mod,
    pointio_led_state_t *leds)
{
    if (!mod || !leds) return -1;

    memset(leds, 0, sizeof(pointio_led_state_t));

    /* Module Status LED */
    switch (mod->status) {
    case POINTIO_STATUS_OK:
        leds->mod_status_green_on = 1;
        break;
    case POINTIO_STATUS_CONNECTING:
        leds->mod_status_green_flash = 1;
        break;
    case POINTIO_STATUS_FAULTED:
        leds->mod_status_red_flash = 1;
        break;
    case POINTIO_STATUS_MAJOR_FAULT:
        leds->mod_status_red_on = 1;
        break;
    case POINTIO_STATUS_INHIBITED:
        leds->mod_status_green_flash = 1;
        break;
    case POINTIO_STATUS_STANDBY:
        leds->mod_status_green_flash = 1;
        break;
    case POINTIO_STATUS_NOT_PRESENT:
    case POINTIO_STATUS_WRONG_MODULE:
        leds->mod_status_red_flash = 1;
        break;
    default:
        break;
    }

    /* Network Status LED */
    if (mod->conn_state == POINTIO_CONN_STATE_RUNNING) {
        leds->net_status_green_on = 1;
    } else if (mod->conn_state == POINTIO_CONN_STATE_CONNECTING) {
        leds->net_status_green_flash = 1;
    } else if (mod->conn_state == POINTIO_CONN_STATE_TIMED_OUT) {
        leds->net_status_red_flash = 1;
    } else if (mod->conn_state == POINTIO_CONN_STATE_FAULTED) {
        leds->net_status_red_on = 1;
    }

    /* I/O Status LEDs - one per channel */
    leds->io_status_bitmask = 0;
    for (uint8_t ch = 0; ch < mod->num_channels && ch < 32; ch++) {
        uint8_t byte_idx = ch / 8;
        uint8_t bit_idx = ch % 8;

        if (mod->type == POINTIO_TYPE_DIGITAL_INPUT ||
            mod->type == POINTIO_TYPE_SAFETY_INPUT) {
            if (byte_idx < mod->input_size_bytes &&
                (mod->input_data[byte_idx] & (1 << bit_idx))) {
                leds->io_status_bitmask |= ((uint32_t)1 << ch);
            }
        } else if (mod->type == POINTIO_TYPE_DIGITAL_OUTPUT ||
                   mod->type == POINTIO_TYPE_SAFETY_OUTPUT) {
            if (byte_idx < mod->output_size_bytes &&
                (mod->output_data[byte_idx] & (1 << bit_idx))) {
                leds->io_status_bitmask |= ((uint32_t)1 << ch);
            }
        }
    }

    return 0;
}

/*===========================================================================
 * L6: Troubleshooting a Faulted Module
 *
 * Classic field service scenario: technician encounters a module
 * with flashing red MOD LED. This function provides structured
 * diagnostic guidance.
 *===========================================================================*/

int pointio_troubleshoot_faulted_module(const pointio_module_t *mod,
    const char **fault_desc, const char **action_plan, int *is_recoverable)
{
    if (!mod || !fault_desc || !action_plan || !is_recoverable) return -1;

    *fault_desc = pointio_fault_code_string(mod->active_fault);
    *action_plan = pointio_fault_recovery_action(mod->active_fault);

    int severity = pointio_fault_severity(mod->active_fault);
    *is_recoverable = (severity <= 1) ? 1 : 0;

    return 0;
}

/*===========================================================================
 * L6: Module Self-Test
 *
 * Simulates the power-on self-test (POST) and on-demand self-test
 * available in Studio 5000 Module Properties → Diagnostics tab.
 *
 * Test categories:
 *   Bit 0: RAM test (read-write pattern)
 *   Bit 1: ROM/Flash checksum verification
 *   Bit 2: A/D converter self-calibration
 *   Bit 3: Output driver test
 *   Bit 4: Communication interface loopback
 *   Bit 5: Terminal base presence detection
 *===========================================================================*/

int pointio_module_self_test(pointio_module_t *mod, uint32_t *results)
{
    if (!mod || !results) return -1;

    *results = 0;

    /* Bit 0: RAM test - simulated as always passing */
    *results |= (1 << 0);

    /* Bit 1: ROM checksum - simulated as always passing */
    *results |= (1 << 1);

    /* Bit 2: A/D self-cal - only for analog input modules */
    if (mod->type == POINTIO_TYPE_ANALOG_INPUT) {
        *results |= (1 << 2);
    }

    /* Bit 3: Output driver test - only for output modules */
    if (mod->type == POINTIO_TYPE_DIGITAL_OUTPUT ||
        mod->type == POINTIO_TYPE_SAFETY_OUTPUT ||
        mod->type == POINTIO_TYPE_ANALOG_OUTPUT) {
        *results |= (1 << 3);
    }

    /* Bit 4: Communication interface loopback */
    if (mod->status == POINTIO_STATUS_OK) {
        *results |= (1 << 4);
    }

    /* Bit 5: Terminal base presence */
    if (mod->status != POINTIO_STATUS_NOT_PRESENT) {
        *results |= (1 << 5);
    }

    /* All critical tests passed? */
    uint32_t critical_mask = (1 << 0) | (1 << 1) | (1 << 4);
    if ((*results & critical_mask) != critical_mask) {
        return -1;  /* Critical test failure */
    }

    return 0;
}

/*===========================================================================
 * L6: Module Missing Detection
 *
 * When a module expected in the I/O configuration is not physically
 * present on the PointBus backplane. This is a common startup issue.
 *
 * Detection methods:
 *   1. Adapter does not detect module in slot during enumeration
 *   2. Module fails to respond to identity request
 *   3. Electronic keying mismatch (slot populated but wrong module)
 *===========================================================================*/

int pointio_detect_module_missing(const pointio_chassis_t *chassis,
    uint8_t expected_slot, int *reason)
{
    if (!chassis || !reason) return -1;
    if (expected_slot == 0 || expected_slot >= POINTIO_MAX_MODULES) return -1;

    const pointio_module_t *mod = &chassis->modules[expected_slot];

    *reason = 0;  /* Default: not missing */

    if (mod->status == POINTIO_STATUS_NOT_PRESENT) {
        /* Module not detected */
        *reason = 1;  /* No module in slot - check power, seating */
        return 1;      /* Module IS missing */
    }

    if (mod->status == POINTIO_STATUS_WRONG_MODULE) {
        *reason = 4;  /* Wrong module in slot */
        return 1;
    }

    if (mod->status == POINTIO_STATUS_MAJOR_FAULT) {
        /* Module present but major fault - could be power or backplane */
        *reason = 2;  /* Check terminal base seating */
        return 1;
    }

    return 0;  /* Module present and OK */
}

/*===========================================================================
 * L5: Scan Jitter Analysis (per-module)
 *===========================================================================*/

int pointio_analyze_scan_jitter(const pointio_module_t *mod,
    double *max_jitter_us, double *rms_jitter_us, int *is_excessive)
{
    if (!mod || !max_jitter_us || !rms_jitter_us || !is_excessive) return -1;

    /* For a single module, jitter is estimated from scan timing */
    double rpi_us = (double)mod->rpi_us;

    /* Simulated: typical Point I/O jitter is < 1% of RPI for well-designed systems */
    *max_jitter_us = rpi_us * 0.01;   /* 1% of RPI typical */
    *rms_jitter_us = *max_jitter_us / 6.0;  /* RMS approximation */

    *is_excessive = 0;
    if (*max_jitter_us > rpi_us * 0.25) {
        *is_excessive = 1;  /* Jitter exceeds 25% of RPI */
    }

    return 0;
}

/*===========================================================================
 * L2: Fault Clear and Module Inhibit
 *===========================================================================*/

/**
 * Attempt to clear a recoverable module fault.
 *
 * Only clears faults of severity 1 (minor). Major and critical faults
 * require physical intervention.
 */
int pointio_clear_module_fault(pointio_module_t *mod)
{
    if (!mod) return -1;

    if (mod->active_fault == POINTIO_FAULT_NONE) return 0;

    int severity = pointio_fault_severity(mod->active_fault);

    if (severity <= 1) {
        /* Minor fault: clear it and return to OK */
        mod->active_fault = POINTIO_FAULT_NONE;
        mod->status = POINTIO_STATUS_OK;
        return 0;  /* Fault cleared */
    }

    return 1;  /* Cannot clear - major/critical fault */
}

/**
 * Inhibit or uninhibit a module.
 *
 * Inhibited modules:
 *   - Do not participate in I/O scanning
 *   - Outputs go to configured fault state
 *   - Module remains in I/O configuration tree
 *   - Status LED flashes yellow
 *
 * This is used during maintenance (e.g., replacing a field device)
 * without having to remove the module from the Studio 5000 project.
 */
int pointio_set_module_inhibit(pointio_module_t *mod, int inhibit)
{
    if (!mod) return -1;

    if (inhibit) {
        /* Save current status */
        mod->status = POINTIO_STATUS_INHIBITED;

        /* Apply safe output state */
        if (mod->type == POINTIO_TYPE_DIGITAL_OUTPUT ||
            mod->type == POINTIO_TYPE_SAFETY_OUTPUT) {
            pointio_do_apply_fault_state(mod);
            /* Restore inhibited status (apply_fault_state sets FAULTED) */
            mod->status = POINTIO_STATUS_INHIBITED;
        } else if (mod->type == POINTIO_TYPE_ANALOG_OUTPUT) {
            /* Zero all analog outputs */
            memset(mod->output_data, 0, POINTIO_MAX_DATA_SIZE);
        }
    } else {
        /* Uninhibit: restore to OK status */
        if (mod->active_fault == POINTIO_FAULT_NONE) {
            mod->status = POINTIO_STATUS_OK;
        } else {
            mod->status = POINTIO_STATUS_FAULTED;
        }
    }

    return 0;
}
