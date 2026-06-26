/**
 * @file    logix_execution_model.c
 * @brief   ControlLogix Execution Model - Scan Cycle Simulation
 * L2-L3, L6
 */

#include <string.h>
#include "logix_execution_model.h"

void logix_execution_init(logix_scan_cycle_t *scan,
                           logix_execution_stats_t *stats)
{
    if (!scan || !stats) return;

    memset(scan, 0, sizeof(*scan));
    memset(stats, 0, sizeof(*stats));

    scan->system_overhead_slice_ns = 20000000;  /* Default 20ms */
}

logix_fault_type_t logix_execute_scan(logix_scan_cycle_t *scan,
                                       logix_execution_stats_t *stats,
                                       const studio5000_project_t *project,
                                       logix_io_connection_t *io_conns,
                                       uint32_t io_count)
{
    if (!scan || !stats) return FAULT_NONE;
    (void)project; /* Reserved for task execution scheduling */

    /* Phase 1: Input Scan */
    scan->last_input_scan_time_ns = 100000; /* Simulated 100us */
    logix_execute_input_scan(scan, io_conns, io_count, stats->scan_count);

    /* Phase 2: Program Scan */
    /* Task execution would occur here */
    scan->last_scan_time_ns = 1000000; /* Simulated 1ms */

    /* Phase 3: Output Scan */
    scan->last_output_scan_time_ns = 50000; /* Simulated 50us */
    logix_execute_output_scan(scan, io_conns, io_count, stats->scan_count);

    /* Phase 4: System Overhead */
    scan->last_overhead_time_ns = scan->system_overhead_slice_ns;

    /* Update statistics */
    stats->scan_count++;
    int64_t total_scan = scan->last_input_scan_time_ns +
                         scan->last_scan_time_ns +
                         scan->last_output_scan_time_ns +
                         scan->last_overhead_time_ns;
    stats->last_scan_time_ns = total_scan;

    if (total_scan < stats->min_scan_time_ns || stats->min_scan_time_ns == 0) {
        stats->min_scan_time_ns = total_scan;
    }
    if (total_scan > stats->max_scan_time_ns) {
        stats->max_scan_time_ns = total_scan;
    }
    stats->avg_scan_time_ns = (stats->avg_scan_time_ns * (stats->scan_count - 1) +
                               total_scan) / stats->scan_count;

    return FAULT_NONE;
}

void logix_execute_input_scan(logix_scan_cycle_t *scan,
                               logix_io_connection_t *io_conns,
                               uint32_t io_count,
                               uint64_t current_time_ns)
{
    if (!scan || !io_conns) return;

    for (uint32_t i = 0; i < io_count; i++) {
        if (!io_conns[i].connection_active || io_conns[i].faulted) continue;

        /* Check if RPI interval has elapsed */
        int64_t rpi_ns = (int64_t)(io_conns[i].rpi_ms * 1000000.0);
        uint64_t elapsed = current_time_ns - io_conns[i].last_update_timestamp_ns;

        if (elapsed >= (uint64_t)rpi_ns) {
            /* Update input data */
            io_conns[i].packet_count++;
            io_conns[i].last_update_timestamp_ns = current_time_ns;
        }

        /* COS (Change of State) handling */
        if (io_conns[i].cos_enabled) {
            /* COS triggers would be detected and cause immediate update */
            /* Implementation: check input data change mask */
        }
    }
}

void logix_execute_output_scan(logix_scan_cycle_t *scan,
                                logix_io_connection_t *io_conns,
                                uint32_t io_count,
                                uint64_t current_time_ns)
{
    if (!scan || !io_conns) return;

    for (uint32_t i = 0; i < io_count; i++) {
        if (!io_conns[i].connection_active || io_conns[i].faulted) continue;

        /* Write output data to module */
        io_conns[i].packet_count++;
        io_conns[i].last_update_timestamp_ns = current_time_ns;
    }
}

bool logix_check_task_overlap(const logix_task_t *task, int64_t current_time_ns)
{
    if (!task) return false;
    if (task->type != TASK_TYPE_PERIODIC) return false;
    (void)current_time_ns; /* Used in timestamp comparison logic */

    int64_t period_ns = (int64_t)task->period_us * 1000;

    /* Overlap occurs if last scan time exceeds period */
    bool overlap = (task->last_scan_time_ns > period_ns);

    if (overlap) {
        switch (task->overlap_policy) {
            case OVERLAP_IGNORE:
                /* Skip this trigger */
                return false;
            case OVERLAP_CRUSH:
                /* Queue one pending execution */
                return true;
            case OVERLAP_MINOR_FAULT:
                /* Generate minor fault */
                return true;
        }
    }

    return false;
}

int64_t logix_compute_overhead_slice(const studio5000_project_t *project)
{
    if (!project) return 20000000; /* Default 20ms */

    /* Default overhead: 20% of the fastest periodic task period.
     * If no periodic tasks, use default 20ms. */
    int64_t fastest_period_ns = INT64_MAX;

    for (uint32_t i = 0; i < project->task_count; i++) {
        /* In a real implementation, we'd check task type and period */
        fastest_period_ns = 10000000; /* Placeholder: 10ms */
        break;
    }

    if (fastest_period_ns == INT64_MAX) {
        return 20000000; /* 20ms default */
    }

    /* 20% of fastest period, minimum 1ms */
    int64_t overhead = fastest_period_ns / 5;
    if (overhead < 1000000) overhead = 1000000;
    return overhead;
}

uint32_t logix_validate_io_connections(const logix_io_connection_t *conns,
                                         uint32_t count)
{
    if (!conns) return count;

    uint32_t faulted = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (conns[i].faulted) faulted++;
    }
    return faulted;
}

bool logix_controller_mode_transition(logix_controller_mode_t from,
                                       logix_controller_mode_t to,
                                       bool major_fault_present)
{
    /* Program -> Run: only if no non-clearable major faults */
    if (from == CONTROLLER_MODE_PROGRAM && to == CONTROLLER_MODE_RUN) {
        return !major_fault_present;
    }
    /* Program -> Test */
    if (from == CONTROLLER_MODE_PROGRAM && to == CONTROLLER_MODE_TEST) {
        return true;
    }
    /* Test -> Program */
    if (from == CONTROLLER_MODE_TEST && to == CONTROLLER_MODE_PROGRAM) {
        return true;
    }
    /* Test -> Run */
    if (from == CONTROLLER_MODE_TEST && to == CONTROLLER_MODE_RUN) {
        return !major_fault_present;
    }
    /* Run -> Program */
    if (from == CONTROLLER_MODE_RUN && to == CONTROLLER_MODE_PROGRAM) {
        return true;
    }
    /* Run -> Fault */
    if (from == CONTROLLER_MODE_RUN && to == CONTROLLER_MODE_FAULT) {
        return true;
    }
    /* Fault -> Program (after fault cleared) */
    if (from == CONTROLLER_MODE_FAULT && to == CONTROLLER_MODE_PROGRAM) {
        return !major_fault_present;
    }
    /* Any -> Remote Program/Run/Test */
    if (to == CONTROLLER_MODE_PROGRAM || to == CONTROLLER_MODE_RUN ||
        to == CONTROLLER_MODE_TEST) {
        return true;
    }

    return false;
}

void logix_record_fault(logix_fault_record_t *record,
                         logix_fault_type_t type,
                         uint32_t code,
                         uint64_t timestamp_ns,
                         bool is_major)
{
    if (!record) return;

    memset(record, 0, sizeof(*record));
    record->type = type;
    record->fault_code = code;
    record->timestamp_ns = timestamp_ns;
    record->is_major_fault = is_major;

    /* Determine if power-up clearable */
    switch (type) {
        case FAULT_TYPE_MISMATCH:
        case FAULT_ARRAY_INDEX_OUT_OF_BOUNDS:
        case FAULT_DIVIDE_BY_ZERO:
        case FAULT_TIMER_INVALID_PRESET:
            record->power_up_clearable = false;
            break;
        case FAULT_TASK_WATCHDOG:
        case FAULT_OVERFLOW:
        case FAULT_STACK_OVERFLOW:
            record->power_up_clearable = true;
            break;
        default:
            record->power_up_clearable = false;
            break;
    }
}

double logix_estimate_rpi_jitter_us(double rpi_ms)
{
    /* RPI-induced jitter: typically 25% of RPI for EtherNet/IP */
    return rpi_ms * 1000.0 * 0.25;
}