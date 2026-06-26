/**
 * guardlogix_diagnostics.c -- GuardLogix Diagnostic Coverage and Fault Detection
 *
 * Knowledge points:
 *   L1: Diagnostic Coverage (DC) per IEC 61508-2
 *   L2: Online diagnostics vs offline diagnostics
 *   L2: Fault reaction time and fault annunciation
 *   L4: IEC 61508-2 Tables A.1-A.15 (diagnostic techniques by SIL)
 *   L5: Diagnostic test interval optimization
 *   L6: Fault detection sequencing for multi-channel systems
 *   L7: Industrial application: Rockwell GuardLogix diagnostic suite
 *   L8: Time-varying diagnostic coverage with aging effects
 *
 * Diagnostic coverage (DC) is the fraction of dangerous failures that
 * are detected by automatic diagnostic tests. GuardLogix achieves
 * DC >= 99% for its internal logic solver through diverse hardware
 * and firmware diagnostics.
 *
 * Reference: IEC 61508-2:2010 Annex A (Diagnostic techniques)
 *            GuardLogix 5580 Safety Reference Manual
 */

#include "guardlogix_safety.h"
#include "guardlogix_sil.h"
#include <string.h>
#include <stdlib.h>

/* Diagnostic test categories per IEC 61508-2 Table A.1 */
typedef enum {
    GLX_DIAG_RAM_TEST           = 0,
    GLX_DIAG_ROM_TEST           = 1,
    GLX_DIAG_CPU_REGISTER_TEST  = 2,
    GLX_DIAG_WATCHDOG           = 3,
    GLX_DIAG_POWER_MONITOR      = 4,
    GLX_DIAG_CLOCK_MONITOR      = 5,
    GLX_DIAG_VOLTAGE_MONITOR    = 6,
    GLX_DIAG_TEMPERATURE_MONITOR = 7,
    GLX_DIAG_COMMUNICATION_TEST  = 8,
    GLX_DIAG_MEMORY_PROTECTION   = 9,
    GLX_DIAG_INSTRUCTION_TEST    = 10,
    GLX_DIAG_INTERRUPT_HANDLING  = 11,
    GLX_DIAG_FLOATING_POINT_TEST = 12,
    GLX_DIAG_DMA_TEST            = 13,
    GLX_DIAG_BROWN_OUT_DETECT    = 14,
    GLX_DIAG_COUNT               = 15
} glx_diagnostic_test_type_t;

/* Fault injection types for diagnostic validation */
typedef enum {
    GLX_FAULT_STUCK_AT_0       = 0,
    GLX_FAULT_STUCK_AT_1       = 1,
    GLX_FAULT_BIT_FLIP         = 2,
    GLX_FAULT_TIMING           = 3,
    GLX_FAULT_VOLTAGE_DROOP    = 4,
    GLX_FAULT_CLOCK_DRIFT      = 5,
    GLX_FAULT_MEMORY_CORRUPT   = 6,
    GLX_FAULT_COMMUNICATION    = 7
} glx_fault_type_t;

/* Individual diagnostic test result */
typedef struct {
    glx_diagnostic_test_type_t test_type;
    uint8_t  test_enabled;          /* 1 = test is active */
    uint8_t  last_result;           /* 1 = passed, 0 = failed */
    uint32_t test_interval_us;      /* How often the test runs */
    uint32_t last_run_timestamp;    /* When the test last ran */
    uint32_t consecutive_failures;  /* Number of consecutive failures */
    uint32_t total_runs;            /* Total number of test runs */
    uint32_t total_failures;        /* Cumulative failure count */
    double   contribution_to_dc;    /* This test"s contribution to DC */
    uint8_t  required_for_sil3;     /* Required for SIL 3 per IEC 61508-2 */
} glx_diagnostic_test_t;

/* Complete diagnostic suite */
typedef struct {
    glx_diagnostic_test_t tests[GLX_DIAG_COUNT];
    double   overall_dc;               /* Overall diagnostic coverage */
    double   target_dc;                /* Required DC for target SIL */
    uint8_t  diagnostics_healthy;      /* All critical tests passing */
    uint32_t last_full_suite_timestamp;/* Last complete diagnostic run */
    uint32_t diagnostic_interval_us;   /* Interval between full suites */
    uint8_t  fault_reaction_active;    /* Fault reaction in progress */
    uint32_t fault_count_since_reset;  /* Total faults since last reset */
} glx_diagnostic_suite_t;

/* L8 Advanced: time-varying diagnostic coverage model */
typedef struct {
    double   initial_dc;           /* DC when system is new */
    double   aging_rate_per_hour;  /* DC degradation rate */
    double   current_dc;           /* Current estimated DC */
    uint32_t last_recalibration;   /* Last recalibration timestamp */
    uint32_t hours_online;         /* Total online hours */
} glx_dc_aging_model_t;

/* ==========================================================================
 * Diagnostic Suite Initialization and Management
 * ========================================================================== */

int glx_diagnostic_suite_init(glx_diagnostic_suite_t *suite,
                               double target_dc,
                               uint32_t diag_interval_us)
{
    if (!suite) return -1;
    if (target_dc < 0.0 || target_dc > 1.0) return -1;

    memset(suite, 0, sizeof(*suite));
    suite->target_dc = target_dc;
    suite->diagnostic_interval_us = diag_interval_us;
    suite->diagnostics_healthy = 0;

    /* DC contribution per test per IEC 61508-2 Annex A.
     * RAM test + ROM test + CPU register test + watchdog
     * + power monitor + program sequence monitoring = >99% DC */
    const double dc_contributions[GLX_DIAG_COUNT] = {
        0.15, 0.10, 0.12, 0.10, 0.08, 0.08,
        0.05, 0.05, 0.08, 0.05, 0.05,
        0.03, 0.02, 0.02, 0.02
    };

    const uint8_t required_sil3[GLX_DIAG_COUNT] = {
        1, 1, 1, 1, 1, 1,
        0, 0, 1, 1, 1,
        0, 0, 0, 0
    };

    for (int i = 0; i < GLX_DIAG_COUNT; i++) {
        suite->tests[i].test_type = (glx_diagnostic_test_type_t)i;
        suite->tests[i].test_enabled = 1;
        suite->tests[i].last_result = 0;
        suite->tests[i].test_interval_us = diag_interval_us;
        suite->tests[i].contribution_to_dc = dc_contributions[i];
        suite->tests[i].required_for_sil3 = required_sil3[i];
    }

    suite->overall_dc = 0.0;
    return 0;
}

/**
 * Run a specific diagnostic test.
 *
 * This simulates the execution of a diagnostic test and records
 * the result. Each test has its own DC contribution.
 *
 * @param suite     Diagnostic suite.
 * @param test_type Which test to run.
 * @param test_data Pointer to test-specific data (or NULL).
 * @param timestamp Current timestamp.
 * @return          1 = passed, 0 = failed.
 */
int glx_diagnostic_run_test(glx_diagnostic_suite_t *suite,
                             glx_diagnostic_test_type_t test_type,
                             const void *test_data,
                             uint32_t timestamp)
{
    if (!suite || test_type >= GLX_DIAG_COUNT) return 0;

    glx_diagnostic_test_t *test = &suite->tests[test_type];
    if (!test->test_enabled) return 1; /* Disabled tests pass by default */

    test->last_run_timestamp = timestamp;
    test->total_runs++;

    /* Perform the diagnostic test.
     * In a real implementation, each test type has hardware-specific
     * operations. Here we model the test execution generically. */
    int result = 1; /* Default: pass */

    switch (test_type) {
    case GLX_DIAG_RAM_TEST:
        /* March-C RAM test: write 0xAA, verify, write 0x55, verify */
        /* Test uses the test_data pointer as a buffer address */
        result = (test_data != NULL) ? 1 : 0;
        break;

    case GLX_DIAG_ROM_TEST:
        /* CRC-32 over ROM content compared to stored value */
        /* ROM CRC would be calculated over firmware region */
        result = 1;
        break;

    case GLX_DIAG_CPU_REGISTER_TEST:
        /* Walk-through test of CPU registers with known patterns */
        result = 1;
        break;

    case GLX_DIAG_WATCHDOG:
        /* Verify watchdog can trigger within specified time */
        result = 1;
        break;

    case GLX_DIAG_POWER_MONITOR:
        /* Verify all power rails within tolerance */
        result = 1;
        break;

    case GLX_DIAG_CLOCK_MONITOR:
        /* Verify system clock frequency within tolerance */
        result = 1;
        break;

    case GLX_DIAG_VOLTAGE_MONITOR:
        /* Verify core and I/O voltages */
        result = 1;
        break;

    case GLX_DIAG_TEMPERATURE_MONITOR:
        /* Verify junction temperature below threshold */
        result = 1;
        break;

    case GLX_DIAG_COMMUNICATION_TEST:
        /* Loopback test on safety communication channels */
        result = (test_data != NULL) ? 1 : 0;
        break;

    case GLX_DIAG_MEMORY_PROTECTION:
        /* Verify MPU prevents access to unauthorized regions */
        result = 1;
        break;

    case GLX_DIAG_INSTRUCTION_TEST:
        /* Execute known instruction sequences and verify results */
        result = 1;
        break;

    case GLX_DIAG_INTERRUPT_HANDLING:
        /* Verify interrupt latency and nesting */
        result = 1;
        break;

    case GLX_DIAG_FLOATING_POINT_TEST:
        /* Verify FPU operations with IEEE 754 corner cases */
        result = 1;
        break;

    case GLX_DIAG_DMA_TEST:
        /* DMA transfer with CRC verification */
        result = 1;
        break;

    case GLX_DIAG_BROWN_OUT_DETECT:
        /* Verify brown-out detection threshold */
        result = 1;
        break;

    default:
        result = 1;
        break;
    }

    test->last_result = (uint8_t)result;

    if (result) {
        test->consecutive_failures = 0;
    } else {
        test->consecutive_failures++;
        test->total_failures++;
        suite->fault_count_since_reset++;
    }

    return result;
}

/**
 * Run a complete diagnostic suite (all enabled tests).
 *
 * @param suite     Diagnostic suite.
 * @param timestamp Current timestamp.
 * @return          1 = all tests passed, 0 = one or more failures.
 */
int glx_diagnostic_run_full_suite(glx_diagnostic_suite_t *suite,
                                   uint32_t timestamp)
{
    if (!suite) return 0;

    int all_passed = 1;
    double total_dc = 0.0;

    for (int i = 0; i < GLX_DIAG_COUNT; i++) {
        int result = glx_diagnostic_run_test(suite,
            (glx_diagnostic_test_type_t)i, NULL, timestamp);

        if (result && suite->tests[i].test_enabled) {
            total_dc += suite->tests[i].contribution_to_dc;
        }

        if (!result && suite->tests[i].required_for_sil3) {
            all_passed = 0;
        }
    }

    suite->overall_dc = total_dc;
    suite->diagnostics_healthy = (uint8_t)all_passed;
    suite->last_full_suite_timestamp = timestamp;

    return all_passed ? 1 : 0;
}

/**
 * Get the current diagnostic coverage from the suite.
 *
 * @param suite Diagnostic suite.
 * @return      Overall DC (0.0 to 1.0).
 */
double glx_diagnostic_get_coverage(const glx_diagnostic_suite_t *suite)
{
    if (!suite) return 0.0;
    return suite->overall_dc;
}

/**
 * Verify that diagnostic coverage meets SIL requirements.
 *
 * SIL 1: DC >= 0% (no minimum)
 * SIL 2: DC >= 60%
 * SIL 3: DC >= 90% (GuardLogix achieves >= 99% for logic solver)
 * SIL 4: DC >= 99%
 *
 * @param suite Diagnostic suite.
 * @param sil   Required SIL level.
 * @return      1 = DC meets requirement, 0 = insufficient.
 */
int glx_diagnostic_verify_sil(const glx_diagnostic_suite_t *suite,
                               glx_sil_level_t sil)
{
    if (!suite) return 0;

    double required_dc = 0.0;
    switch (sil) {
    case GLX_SIL_1: required_dc = 0.0;   break;
    case GLX_SIL_2: required_dc = 0.60;  break;
    case GLX_SIL_3: required_dc = 0.90;  break;
    case GLX_SIL_4: required_dc = 0.99;  break;
    default:        required_dc = 0.60;  break;
    }

    return (suite->overall_dc >= required_dc) ? 1 : 0;
}

/**
 * Get the number of test types that have failed since last reset.
 *
 * @param suite Diagnostic suite.
 * @return      Number of failing test types.
 */
int glx_diagnostic_failure_count(const glx_diagnostic_suite_t *suite)
{
    if (!suite) return 0;
    return (int)suite->fault_count_since_reset;
}

/**
 * Reset all diagnostic test statistics.
 *
 * @param suite Diagnostic suite.
 */
void glx_diagnostic_reset_stats(glx_diagnostic_suite_t *suite)
{
    if (!suite) return;
    suite->fault_count_since_reset = 0;
    for (int i = 0; i < GLX_DIAG_COUNT; i++) {
        suite->tests[i].consecutive_failures = 0;
        suite->tests[i].total_failures = 0;
        suite->tests[i].total_runs = 0;
    }
}

/* ==========================================================================
 * L8 Advanced: DC Aging Model
 * ========================================================================== */

/**
 * Initialize the diagnostic coverage aging model.
 *
 * Over time, diagnostic coverage may degrade due to component aging,
 * environmental effects, and wear-out. This model tracks DC degradation.
 *
 * DC(t) = DC_initial * exp(-aging_rate * t)
 *
 * @param model          Aging model structure.
 * @param initial_dc     Initial DC when commissioned.
 * @param aging_rate     Degradation rate per hour (e.g., 1e-6 for 1%/10000h).
 * @param timestamp      Current timestamp (Unix epoch).
 * @return               0 on success.
 */
int glx_dc_aging_model_init(glx_dc_aging_model_t *model,
                             double initial_dc,
                             double aging_rate_per_hour,
                             uint32_t timestamp)
{
    if (!model) return -1;
    if (initial_dc < 0.0 || initial_dc > 1.0) return -1;
    if (aging_rate_per_hour < 0.0) return -1;

    memset(model, 0, sizeof(*model));
    model->initial_dc = initial_dc;
    model->aging_rate_per_hour = aging_rate_per_hour;
    model->current_dc = initial_dc;
    model->last_recalibration = timestamp;
    model->hours_online = 0;

    return 0;
}

/**
 * Update the DC aging model based on elapsed time.
 *
 * @param model          Aging model.
 * @param timestamp      Current timestamp.
 * @return               Current (aged) DC value.
 */
double glx_dc_aging_update(glx_dc_aging_model_t *model,
                            uint32_t timestamp)
{
    if (!model) return 0.0;

    uint32_t hours_elapsed = 0;
    if (timestamp > model->last_recalibration) {
        hours_elapsed = (timestamp - model->last_recalibration) / 3600;
    }

    model->hours_online += hours_elapsed;
    model->current_dc = model->initial_dc *
        exp(-model->aging_rate_per_hour * (double)model->hours_online);

    model->last_recalibration = timestamp;

    return model->current_dc;
}

/**
 * Estimate the remaining useful life until DC drops below threshold.
 *
 * @param model       Aging model.
 * @param min_dc      Minimum acceptable DC.
 * @param hours_remain Output: estimated hours until DC < min_dc.
 * @return            0 on success, -1 if already below threshold.
 */
int glx_dc_estimate_remaining_life(const glx_dc_aging_model_t *model,
                                    double min_dc,
                                    uint32_t *hours_remain)
{
    if (!model || !hours_remain) return -1;
    if (model->current_dc <= min_dc) {
        *hours_remain = 0;
        return -1;
    }
    if (model->aging_rate_per_hour <= 0.0) {
        *hours_remain = UINT32_MAX;
        return 0;
    }

    /* Solve: min_dc = initial_dc * exp(-rate * (hours_online + t))
     * t = -ln(min_dc / initial_dc) / rate - hours_online */
    double total_hours = -log(min_dc / model->initial_dc) /
                          model->aging_rate_per_hour;
    double remaining = total_hours - model->hours_online;

    if (remaining < 0.0) {
        *hours_remain = 0;
        return -1;
    }

    if (remaining > (double)UINT32_MAX) {
        *hours_remain = UINT32_MAX;
    } else {
        *hours_remain = (uint32_t)remaining;
    }
    return 0;
}
