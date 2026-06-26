/**
 * @file plantpax_controller.h
 * @brief PlantPAx Controller ? ControlLogix, Redundancy, Task Scheduling, Scan Model
 *
 * Module: mini-rockwell-plantpax-dcs
 * Knowledge Coverage: L1 Definitions, L2 Redundancy, L3 Scan Cycle, L4 IEC 61131-3
 *
 * The ControlLogix 1756-L8x controller is the heart of PlantPAx DCS.
 * It executes a deterministic scan cycle:
 *   1. Read Inputs (update input image from I/O modules)
 *   2. Execute Program (logic, PID, alarms, sequencing)
 *   3. Write Outputs (update output image to I/O modules)
 *   4. Housekeeping (communication, diagnostics)
 *
 * Controller Redundancy (1756-RM2):
 *   Hot standby with bumpless switchover. Primary and Secondary controllers
 *   are synchronized every scan. On primary failure, secondary takes over
 *   within one scan cycle (< 100 ms). The switchover is bumpless because
 *   output states are cross-loaded.
 *
 * Reference:
 *   Rockwell 1756-UM001 ControlLogix System User Manual
 *   IEC 61131-3 Programming Industrial Automation Systems
 * Curriculum: RWTH Aachen Industrial Control, MIT 6.302, CMU 24-677
 */

#ifndef PLANTPAX_CONTROLLER_H
#define PLANTPAX_CONTROLLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "plantpax_architecture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1: Controller Core Definitions
 * ------------------------------------------------------------------------- */

/** Controller operational modes */
typedef enum {
    PAX_CTRL_MODE_PROGRAM = 0,       /**< Program mode: logic not executing */
    PAX_CTRL_MODE_RUN = 1,           /**< Run mode: normal execution */
    PAX_CTRL_MODE_TEST = 2,          /**< Test mode: single-scan or step */
    PAX_CTRL_MODE_REMOTE_PROGRAM = 4,/**< Remote program via network */
    PAX_CTRL_MODE_REMOTE_RUN = 5,    /**< Remote run via network */
    PAX_CTRL_MODE_FAULT = 6          /**< Major fault: controller halted */
} pax_ctrl_mode_t;

/** Controller redundancy roles */
typedef enum {
    PAX_REDUN_ROLE_PRIMARY = 0,      /**< Primary (active) controller */
    PAX_REDUN_ROLE_SECONDARY = 1,    /**< Secondary (standby) controller */
    PAX_REDUN_ROLE_SIMULTANEOUS = 2, /**< Both active (qualification only) */
    PAX_REDUN_ROLE_DISQUALIFIED = 3, /**< Disqualified from redundancy */
    PAX_REDUN_ROLE_SOLO = 4          /**< Single controller (no partner) */
} pax_redundancy_role_t;

/** Redundancy synchronization state */
typedef enum {
    PAX_REDUN_SYNC_NONE = 0,
    PAX_REDUN_SYNC_IN_PROGRESS = 1,
    PAX_REDUN_SYNC_COMPLETE = 2,
    PAX_REDUN_SYNC_FAILED = 3,
    PAX_REDUN_SYNC_LOCKED = 4
} pax_redundancy_sync_t;

/** Task types in ControlLogix execution model */
typedef enum {
    PAX_TASK_CONTINUOUS = 0,         /**< Runs continuously (background) */
    PAX_TASK_PERIODIC = 1,           /**< Fixed period (e.g., 100 ms) */
    PAX_TASK_EVENT = 2,              /**< Triggered by event */
    PAX_TASK_INHIBIT = 3,            /**< Inhibited (not running) */
    PAX_TASK_SAFETY = 4              /**< Safety task (SIL certified) */
} pax_task_type_t;

/** Task priority levels (1 = lowest, 15 = highest for Logix) */
typedef enum {
    PAX_PRIORITY_LOWEST = 1,
    PAX_PRIORITY_LOW = 4,
    PAX_PRIORITY_MEDIUM = 8,
    PAX_PRIORITY_HIGH = 12,
    PAX_PRIORITY_HIGHEST = 15,
    PAX_PRIORITY_SAFETY = 15
} pax_task_priority_t;

/* ---------------------------------------------------------------------------
 * L1: Controller Specifications
 * ------------------------------------------------------------------------- */

/** Controller hardware specification */
typedef struct {
    pax_controller_family_t family;
    char model_number[32];
    uint32_t max_io_points;
    uint32_t max_analog_points;
    uint32_t max_tasks;
    uint32_t max_programs_per_task;
    double scan_time_min_ms;
    double typical_scan_time_ms;
    uint32_t user_memory_kb;
    uint32_t safety_memory_kb;
    bool supports_redundancy;
    bool supports_safety;
    bool supports_motion;
    uint32_t ethernet_ports;
    double cpu_clock_mhz;
} pax_ctrl_spec_t;

/** Runtime task configuration */
typedef struct {
    uint32_t task_id;
    char task_name[64];
    pax_task_type_t type;
    pax_task_priority_t priority;
    double period_ms;
    double watchdog_ms;
    double measured_scan_ms;
    double max_scan_ms;
    double min_scan_ms;
    uint64_t scan_count;
    uint64_t overrun_count;
    uint32_t num_programs;
    bool inhibited;
} pax_task_t;

/** Controller instance runtime data */
typedef struct {
    uint32_t controller_id;
    char controller_name[64];
    pax_ctrl_spec_t spec;
    pax_ctrl_mode_t mode;
    pax_redundancy_role_t redun_role;
    pax_redundancy_sync_t redun_sync;
    pax_service_state_t service_state;
    pax_health_t health;
    double uptime_hours;
    double cpu_utilization_pct;
    uint32_t major_fault_code;
    uint32_t minor_fault_count;
    double ambient_temp_c;
    double battery_voltage_v;
    uint32_t num_tasks;
    pax_task_t tasks[32];
    uint64_t total_scan_count;
    uint64_t anomaly_count;
} pax_controller_t;

/* ---------------------------------------------------------------------------
 * L1: Scan Cycle Constants
 * ------------------------------------------------------------------------- */

#define PAX_MAX_TASKS_PER_CONTROLLER    32
#define PAX_MAX_PROGRAMS_PER_TASK       100
#define PAX_DEFAULT_WATCHDOG_MS         500.0
#define PAX_MIN_SCAN_TIME_MS            0.5
#define PAX_REDUN_SWITCHOVER_MAX_MS     100.0
#define PAX_CPU_UTILIZATION_WARN_PCT    80.0
#define PAX_CPU_UTILIZATION_CRIT_PCT    95.0
#define PAX_BATTERY_LOW_V               2.5
#define PAX_AMBIENT_MAX_C               60.0

/* Controller memory limits */
#define PAX_L8X_MAX_DIGITAL_IO          128000
#define PAX_L8X_MAX_ANALOG_IO           4000
#define PAX_L8X_USER_MEMORY_KB          40960
#define PAX_L8X_SAFETY_MEMORY_KB        20480

/* ---------------------------------------------------------------------------
 * L2/L3: Controller API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize controller specification for a given family.
 *
 * Populates the pax_ctrl_spec_t structure with factory specifications
 * for the selected controller family. Different families have different
 * IO capacities, memory, and feature support.
 */
void pax_ctrl_spec_init(pax_ctrl_spec_t *spec, pax_controller_family_t family);

/**
 * @brief Initialize a controller instance.
 *
 * Sets up a controller with its name, family, and initial state.
 * Creates default continuous and periodic tasks.
 */
void pax_controller_init(pax_controller_t *ctrl,
                          const char *name,
                          pax_controller_family_t family);

/**
 * @brief Add a task to the controller runtime.
 *
 * Each task represents an independent execution thread with its
 * own schedule, priority, and watchdog.
 *
 * @return task index on success, -1 on failure (too many tasks)
 */
int pax_controller_add_task(pax_controller_t *ctrl,
                             const char *task_name,
                             pax_task_type_t type,
                             pax_task_priority_t priority,
                             double period_ms);

/**
 * @brief Simulate one scan cycle and update timing statistics.
 *
 * Updates the measured scan time statistics (min, max, average).
 * Checks for scan overrun (measured > period) and updates
 * anomaly counters.
 */
int pax_controller_simulate_scan(pax_controller_t *ctrl,
                                  uint32_t task_id,
                                  double logic_execution_ms);

/**
 * @brief Compute CPU utilization from task scan times.
 *
 * U = sum(scan_time_i / period_i) for all periodic tasks
 *   + continuous task fraction
 *
 * @return CPU utilization as percentage [0-100]
 */
double pax_controller_cpu_utilization(const pax_controller_t *ctrl);

/**
 * @brief Simulate primary-to-secondary failover.
 *
 * Models the bumpless switchover process:
 *   1. Detect primary failure (watchdog timeout)
 *   2. Secondary promoted to primary
 *   3. Outputs cross-loaded from last synchronized state
 *
 * @param primary The failing primary controller
 * @param secondary The standby being promoted
 * @param switchover_time_ms Output: measured switchover time
 * @return 0 on success, non-zero if secondary is not synchronized
 */
int pax_controller_failover(pax_controller_t *primary,
                             pax_controller_t *secondary,
                             double *switchover_time_ms);

/**
 * @brief Synchronize state from primary to secondary.
 *
 * Cross-loads tag values, output states, and program state
 * from primary to secondary controller.
 */
int pax_controller_synchronize(const pax_controller_t *primary,
                                pax_controller_t *secondary);

/**
 * @brief Diagnose controller health from runtime statistics.
 *
 * Checks CPU utilization, temperature, battery, scan overruns,
 * and fault codes to determine overall health.
 */
pax_health_t pax_controller_diagnose(const pax_controller_t *ctrl);

/**
 * @brief Validate task configuration for schedulability.
 *
 * Uses Rate Monotonic Analysis (RMA) upper bound:
 *   U <= n * (2^(1/n) - 1) for n tasks
 *
 * @return 0 if schedulable, 1 if potentially unschedulable
 */
int pax_controller_check_schedulability(const pax_controller_t *ctrl);

/**
 * @brief Dump controller status to stdout.
 */
void pax_controller_dump(const pax_controller_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* PLANTPAX_CONTROLLER_H */
