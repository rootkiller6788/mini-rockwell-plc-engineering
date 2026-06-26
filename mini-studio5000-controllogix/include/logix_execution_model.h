/**
 * @file    logix_execution_model.h
 * @brief   L2-L3: ControlLogix Execution Model and Scan Cycle
 *
 * Knowledge Mapping:
 *   L2 Concepts:     Prescan, scan cycle phases, I/O synchronous vs
 *                    asynchronous, task priority preemption, system overhead
 *                    time slice, communication servicing
 *   L3 Structures:   Scan cycle timing, interrupt handling, motion planner
 *                    integration, overlap detection and handling
 *   L6 Problems:     Scan time optimization, watchdog configuration
 *   L7 Applications: ControlLogix real-time execution
 *
 * Reference: Rockwell Automation Publication 1756-PM005
 *   "Logix5000 Controllers Design Considerations"
 *
 * Course Alignment:
 *   MIT 6.302 - Feedback Systems: real-time software architecture
 *   Purdue ECE 602 - Lumped Systems: periodic task scheduling
 *   RWTH Aachen - Industrial Control Systems: PLC scan cycle
 */

#ifndef LOGIX_EXECUTION_MODEL_H
#define LOGIX_EXECUTION_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "control_logix_platform.h"
#include "studio5000_project.h"
#include "logix_tag_model.h"

typedef enum {
    SCAN_PHASE_IDLE,
    SCAN_PHASE_INPUT_SCAN,
    SCAN_PHASE_PROGRAM_SCAN,
    SCAN_PHASE_OUTPUT_SCAN,
    SCAN_PHASE_SYSTEM_OVERHEAD,
    SCAN_PHASE_PRESCAN,
    SCAN_PHASE_POSTSCAN
} logix_scan_phase_t;

typedef enum {
    CONTROLLER_MODE_PROGRAM = 0,
    CONTROLLER_MODE_RUN     = 1,
    CONTROLLER_MODE_TEST    = 2,
    CONTROLLER_MODE_FAULT   = 3
} logix_controller_mode_t;

typedef enum {
    IO_CONNECTION_NONE           = 0,
    IO_CONNECTION_DIRECT         = 1,
    IO_CONNECTION_RACK_OPTIMIZED = 2,
    IO_CONNECTION_LISTEN_ONLY    = 3
} logix_io_connection_type_t;

typedef struct {
    uint8_t                 slot;
    logix_io_connection_type_t conn_type;
    double                  rpi_ms;
    bool                    connection_active;
    bool                    faulted;
    uint32_t                input_size_bytes;
    uint32_t                output_size_bytes;
    uint32_t                packet_count;
    uint32_t                timeout_count;
    uint64_t                last_update_timestamp_ns;
    bool                    cos_enabled;
    uint32_t                cos_trigger_mask;
} logix_io_connection_t;

typedef struct {
    int64_t     min_scan_time_ns;
    int64_t     max_scan_time_ns;
    int64_t     avg_scan_time_ns;
    int64_t     last_scan_time_ns;
    uint64_t    scan_count;
    double      cpu_utilization_pct;
    uint32_t    task_overlap_count;
    uint32_t    task_timeout_count;
    uint32_t    msg_serviced_count;
    uint32_t    msg_backlog_count;
} logix_execution_stats_t;

typedef struct {
    uint32_t    coarse_update_rate_us;
    uint32_t    servo_update_rate_us;
    uint32_t    axis_count;
    uint32_t    coordinate_system_count;
    double      motion_cpu_budget_pct;
    bool        motion_enabled;
} logix_motion_planner_t;

typedef enum {
    FAULT_NONE                    = 0,
    FAULT_TYPE_MISMATCH           = 1,
    FAULT_ARRAY_INDEX_OUT_OF_BOUNDS = 2,
    FAULT_TIMER_INVALID_PRESET    = 3,
    FAULT_DIVIDE_BY_ZERO          = 4,
    FAULT_STACK_OVERFLOW          = 5,
    FAULT_OVERFLOW                = 6,
    FAULT_TASK_WATCHDOG           = 7,
    FAULT_I_O_FAILURE             = 8,
    FAULT_COMMUNICATION_FAILURE   = 9,
    FAULT_MOTION_FAULT            = 10,
    FAULT_SAFETY_FAULT            = 11,
    FAULT_REDUNDANCY_FAILURE      = 12,
    FAULT_POWER_UP                = 13
} logix_fault_type_t;

typedef struct {
    logix_fault_type_t  type;
    uint32_t            fault_code;
    uint64_t            timestamp_ns;
    uint32_t            task_index;
    uint32_t            program_index;
    uint32_t            routine_index;
    uint32_t            rung_number;
    bool                is_major_fault;
    bool                power_up_clearable;
    char                fault_message[256];
} logix_fault_record_t;

void logix_execution_init(logix_scan_cycle_t *scan,
                           logix_execution_stats_t *stats);
logix_fault_type_t logix_execute_scan(logix_scan_cycle_t *scan,
                                       logix_execution_stats_t *stats,
                                       const studio5000_project_t *project,
                                       logix_io_connection_t *io_conns,
                                       uint32_t io_count);
void logix_execute_input_scan(logix_scan_cycle_t *scan,
                               logix_io_connection_t *io_conns,
                               uint32_t io_count,
                               uint64_t current_time_ns);
void logix_execute_output_scan(logix_scan_cycle_t *scan,
                                logix_io_connection_t *io_conns,
                                uint32_t io_count,
                                uint64_t current_time_ns);
bool logix_check_task_overlap(const logix_task_t *task, int64_t current_time_ns);
int64_t logix_compute_overhead_slice(const studio5000_project_t *project);
uint32_t logix_validate_io_connections(const logix_io_connection_t *conns,
                                         uint32_t count);
bool logix_controller_mode_transition(logix_controller_mode_t from,
                                       logix_controller_mode_t to,
                                       bool major_fault_present);
void logix_record_fault(logix_fault_record_t *record,
                         logix_fault_type_t type,
                         uint32_t code,
                         uint64_t timestamp_ns,
                         bool is_major);
double logix_estimate_rpi_jitter_us(double rpi_ms);

#endif /* LOGIX_EXECUTION_MODEL_H */