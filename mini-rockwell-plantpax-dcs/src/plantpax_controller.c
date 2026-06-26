/**
 * @file plantpax_controller.c
 * @brief PlantPAx Controller Implementation ? Scan Model, Redundancy, Schedulability
 */
#include "plantpax_controller.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------------------
 * L1: Controller Specification Database
 * ------------------------------------------------------------------------- */

void pax_ctrl_spec_init(pax_ctrl_spec_t *spec, pax_controller_family_t family)
{
    if (spec == NULL) return;
    memset(spec, 0, sizeof(pax_ctrl_spec_t));

    spec->family = family;

    switch (family) {
        case PAX_CTRL_CONTROLLOGIX_L8:
            strncpy(spec->model_number, "1756-L85E", sizeof(spec->model_number) - 1);
            spec->max_io_points = PAX_L8X_MAX_DIGITAL_IO;
            spec->max_analog_points = PAX_L8X_MAX_ANALOG_IO;
            spec->max_tasks = 32;
            spec->max_programs_per_task = 100;
            spec->scan_time_min_ms = 0.5;
            spec->typical_scan_time_ms = 10.0;
            spec->user_memory_kb = PAX_L8X_USER_MEMORY_KB;
            spec->safety_memory_kb = PAX_L8X_SAFETY_MEMORY_KB;
            spec->supports_redundancy = true;
            spec->supports_safety = true;
            spec->supports_motion = true;
            spec->ethernet_ports = 2;
            spec->cpu_clock_mhz = 1200.0;
            break;

        case PAX_CTRL_CONTROLLOGIX_L7:
            strncpy(spec->model_number, "1756-L75", sizeof(spec->model_number) - 1);
            spec->max_io_points = 128000;
            spec->max_analog_points = 2000;
            spec->max_tasks = 32;
            spec->max_programs_per_task = 100;
            spec->scan_time_min_ms = 0.5;
            spec->typical_scan_time_ms = 15.0;
            spec->user_memory_kb = 32768;
            spec->safety_memory_kb = 0;
            spec->supports_redundancy = true;
            spec->supports_safety = false;
            spec->supports_motion = true;
            spec->ethernet_ports = 1;
            spec->cpu_clock_mhz = 800.0;
            break;

        case PAX_CTRL_COMPACTLOGIX_5069:
            strncpy(spec->model_number, "5069-L330ERM", sizeof(spec->model_number) - 1);
            spec->max_io_points = 31000;
            spec->max_analog_points = 800;
            spec->max_tasks = 16;
            spec->max_programs_per_task = 100;
            spec->scan_time_min_ms = 1.0;
            spec->typical_scan_time_ms = 20.0;
            spec->user_memory_kb = 5120;
            spec->safety_memory_kb = 0;
            spec->supports_redundancy = false;
            spec->supports_safety = false;
            spec->supports_motion = true;
            spec->ethernet_ports = 2;
            spec->cpu_clock_mhz = 600.0;
            break;

        case PAX_CTRL_COMPACTLOGIX_1769:
            strncpy(spec->model_number, "1769-L36ERM", sizeof(spec->model_number) - 1);
            spec->max_io_points = 30000;
            spec->max_analog_points = 500;
            spec->max_tasks = 16;
            spec->max_programs_per_task = 100;
            spec->scan_time_min_ms = 1.0;
            spec->typical_scan_time_ms = 25.0;
            spec->user_memory_kb = 3072;
            spec->safety_memory_kb = 0;
            spec->supports_redundancy = false;
            spec->supports_safety = false;
            spec->supports_motion = true;
            spec->ethernet_ports = 2;
            spec->cpu_clock_mhz = 400.0;
            break;

        case PAX_CTRL_FLEXLOGIX:
            strncpy(spec->model_number, "1794-L34", sizeof(spec->model_number) - 1);
            spec->max_io_points = 10240;
            spec->max_analog_points = 256;
            spec->max_tasks = 8;
            spec->max_programs_per_task = 32;
            spec->scan_time_min_ms = 2.0;
            spec->typical_scan_time_ms = 30.0;
            spec->user_memory_kb = 1024;
            spec->safety_memory_kb = 0;
            spec->supports_redundancy = false;
            spec->supports_safety = false;
            spec->supports_motion = false;
            spec->ethernet_ports = 1;
            spec->cpu_clock_mhz = 200.0;
            break;

        case PAX_CTRL_COMPACT_GUARDLOGIX:
            strncpy(spec->model_number, "5069-L330ERS3", sizeof(spec->model_number) - 1);
            spec->max_io_points = 31000;
            spec->max_analog_points = 800;
            spec->max_tasks = 16;
            spec->max_programs_per_task = 100;
            spec->scan_time_min_ms = 1.0;
            spec->typical_scan_time_ms = 20.0;
            spec->user_memory_kb = 5120;
            spec->safety_memory_kb = 2560;
            spec->supports_redundancy = false;
            spec->supports_safety = true;
            spec->supports_motion = true;
            spec->ethernet_ports = 2;
            spec->cpu_clock_mhz = 600.0;
            break;

        case PAX_CTRL_GUARDLOGIX:
            strncpy(spec->model_number, "1756-L82S", sizeof(spec->model_number) - 1);
            spec->max_io_points = PAX_L8X_MAX_DIGITAL_IO;
            spec->max_analog_points = PAX_L8X_MAX_ANALOG_IO;
            spec->max_tasks = 32;
            spec->max_programs_per_task = 100;
            spec->scan_time_min_ms = 0.5;
            spec->typical_scan_time_ms = 10.0;
            spec->user_memory_kb = PAX_L8X_USER_MEMORY_KB;
            spec->safety_memory_kb = PAX_L8X_SAFETY_MEMORY_KB;
            spec->supports_redundancy = true;
            spec->supports_safety = true;
            spec->supports_motion = true;
            spec->ethernet_ports = 2;
            spec->cpu_clock_mhz = 1200.0;
            break;

        default:
            break;
    }
}

/* ---------------------------------------------------------------------------
 * L2: Controller Instance Initialization
 * ------------------------------------------------------------------------- */

void pax_controller_init(pax_controller_t *ctrl,
                          const char *name,
                          pax_controller_family_t family)
{
    if (ctrl == NULL) return;

    memset(ctrl, 0, sizeof(pax_controller_t));

    pax_ctrl_spec_init(&ctrl->spec, family);
    ctrl->controller_id = 0;
    if (name != NULL) {
        strncpy(ctrl->controller_name, name,
                sizeof(ctrl->controller_name) - 1);
    }
    ctrl->mode = PAX_CTRL_MODE_PROGRAM;
    ctrl->redun_role = PAX_REDUN_ROLE_SOLO;
    ctrl->redun_sync = PAX_REDUN_SYNC_NONE;
    ctrl->service_state = PAX_SERVICE_COMMISSIONING;
    ctrl->health = PAX_HEALTH_OK;
    ctrl->uptime_hours = 0.0;
    ctrl->cpu_utilization_pct = 0.0;
    ctrl->major_fault_code = 0;
    ctrl->minor_fault_count = 0;
    ctrl->ambient_temp_c = 25.0;
    ctrl->battery_voltage_v = 3.6;
    ctrl->num_tasks = 1;  /* Default continuous task */
    ctrl->total_scan_count = 0;
    ctrl->anomaly_count = 0;

    /* Create default continuous task */
    ctrl->tasks[0].task_id = 0;
    strncpy(ctrl->tasks[0].task_name, "MainTask",
            sizeof(ctrl->tasks[0].task_name) - 1);
    ctrl->tasks[0].type = PAX_TASK_CONTINUOUS;
    ctrl->tasks[0].priority = PAX_PRIORITY_MEDIUM;
    ctrl->tasks[0].period_ms = 0.0;
    ctrl->tasks[0].watchdog_ms = PAX_DEFAULT_WATCHDOG_MS;
    ctrl->tasks[0].measured_scan_ms = 0.0;
    ctrl->tasks[0].max_scan_ms = 0.0;
    ctrl->tasks[0].min_scan_ms = 1e9;
    ctrl->tasks[0].scan_count = 0;
    ctrl->tasks[0].overrun_count = 0;
    ctrl->tasks[0].num_programs = 0;
    ctrl->tasks[0].inhibited = false;
}

/* ---------------------------------------------------------------------------
 * L3: Task Management
 * ------------------------------------------------------------------------- */

int pax_controller_add_task(pax_controller_t *ctrl,
                             const char *task_name,
                             pax_task_type_t type,
                             pax_task_priority_t priority,
                             double period_ms)
{
    if (ctrl == NULL || task_name == NULL) return -1;

    if (ctrl->num_tasks >= PAX_MAX_TASKS_PER_CONTROLLER) {
        return -1;
    }

    uint32_t idx = ctrl->num_tasks;
    pax_task_t *task = &ctrl->tasks[idx];

    memset(task, 0, sizeof(pax_task_t));
    task->task_id = idx;
    strncpy(task->task_name, task_name, sizeof(task->task_name) - 1);
    task->type = type;
    task->priority = priority;
    task->period_ms = period_ms;
    task->watchdog_ms = (period_ms > 0) ? period_ms * 5.0 : PAX_DEFAULT_WATCHDOG_MS;
    task->measured_scan_ms = 0.0;
    task->max_scan_ms = 0.0;
    task->min_scan_ms = 1e9;
    task->scan_count = 0;
    task->overrun_count = 0;
    task->num_programs = 0;
    task->inhibited = false;

    ctrl->num_tasks++;
    return (int)idx;
}

/* ---------------------------------------------------------------------------
 * L3: Scan Cycle Simulation
 *
 * Models one scan cycle execution:
 *   1. Measure scan time
 *   2. Update min/max statistics
 *   3. Check for overrun (measured > period for periodic tasks)
 *   4. Check for watchdog violation
 * ------------------------------------------------------------------------- */

int pax_controller_simulate_scan(pax_controller_t *ctrl,
                                  uint32_t task_id,
                                  double logic_execution_ms)
{
    if (ctrl == NULL) return -1;
    if (task_id >= ctrl->num_tasks) return -2;

    pax_task_t *task = &ctrl->tasks[task_id];

    if (task->inhibited) return 0;

    /* Add I/O overhead (~1-2 ms) to logic execution */
    double io_overhead_ms = 1.5;
    double total_scan_ms = logic_execution_ms + io_overhead_ms;

    /* Update scan statistics */
    task->measured_scan_ms = total_scan_ms;
    task->scan_count++;

    if (total_scan_ms > task->max_scan_ms) {
        task->max_scan_ms = total_scan_ms;
    }
    if (total_scan_ms < task->min_scan_ms) {
        task->min_scan_ms = total_scan_ms;
    }

    /* Check for overrun on periodic tasks */
    if (task->type == PAX_TASK_PERIODIC && task->period_ms > 0.0) {
        if (total_scan_ms > task->period_ms) {
            task->overrun_count++;
            ctrl->anomaly_count++;
        }
        /* Watchdog check */
        if (total_scan_ms > task->watchdog_ms) {
            ctrl->major_fault_code = 1;
            ctrl->mode = PAX_CTRL_MODE_FAULT;
            ctrl->health = PAX_HEALTH_FAILURE;
            return -3;
        }
    }

    ctrl->total_scan_count++;
    return 0;
}

/* ---------------------------------------------------------------------------
 * L5: CPU Utilization (Rate Monotonic Analysis)
 *
 * U = sum(C_i / T_i) for all periodic tasks
 *   where C_i = measured_scan_ms, T_i = period_ms
 *
 * For continuous tasks, estimate fraction of time consumed
 * relative to reference period.
 * ------------------------------------------------------------------------- */

double pax_controller_cpu_utilization(const pax_controller_t *ctrl)
{
    if (ctrl == NULL) return 0.0;

    double utilization = 0.0;
    uint32_t i;
    double ref_period_ms = 100.0;  /* Reference for continuous tasks */

    for (i = 0; i < ctrl->num_tasks; i++) {
        const pax_task_t *task = &ctrl->tasks[i];
        if (task->inhibited) continue;

        double period = (task->type == PAX_TASK_CONTINUOUS)
                        ? ref_period_ms : task->period_ms;

        if (period > 0.0 && task->measured_scan_ms > 0.0) {
            utilization += task->measured_scan_ms / period;
        }
    }

    return utilization * 100.0;
}

/* ---------------------------------------------------------------------------
 * L2: Controller Redundancy Failover
 *
 * Models the 1756-RM2 redundancy module behavior:
 *   1. Primary failure detected (watchdog or health check)
 *   2. Check secondary is synchronized
 *   3. Promote secondary to primary
 *   4. Transfer cross-load data
 *   5. Disqualify failed primary
 *
 * Switchover time = qualification_check_time + data_transfer_time
 * Target: < 100 ms for bumpless switchover
 * ------------------------------------------------------------------------- */

int pax_controller_failover(pax_controller_t *primary,
                             pax_controller_t *secondary,
                             double *switchover_time_ms)
{
    if (primary == NULL || secondary == NULL) return -1;

    /* Secondary must be synchronized */
    if (secondary->redun_sync != PAX_REDUN_SYNC_COMPLETE) {
        return -2;
    }

    /* Qualification check: ~10 ms */
    double qual_time_ms = 10.0;

    /* Data cross-load: tag data transfer (~5-20 ms for typical project) */
    double data_time_ms = 15.0;

    double total_switchover = qual_time_ms + data_time_ms;

    if (switchover_time_ms != NULL) {
        *switchover_time_ms = total_switchover;
    }

    /* Transition roles */
    secondary->redun_role = PAX_REDUN_ROLE_PRIMARY;
    secondary->mode = PAX_CTRL_MODE_RUN;
    secondary->service_state = PAX_SERVICE_RUNNING;

    primary->redun_role = PAX_REDUN_ROLE_DISQUALIFIED;
    primary->mode = PAX_CTRL_MODE_FAULT;
    primary->service_state = PAX_SERVICE_DEGRADED;
    primary->health = PAX_HEALTH_FAILURE;

    return 0;
}

/* ---------------------------------------------------------------------------
 * L2: Primary-to-Secondary Synchronization
 * ------------------------------------------------------------------------- */

int pax_controller_synchronize(const pax_controller_t *primary,
                                pax_controller_t *secondary)
{
    if (primary == NULL || secondary == NULL) return -1;

    /* Synchronize operational data from primary to secondary */
    secondary->mode = primary->mode;
    secondary->uptime_hours = primary->uptime_hours;
    secondary->cpu_utilization_pct = primary->cpu_utilization_pct;
    secondary->total_scan_count = primary->total_scan_count;

    /* Copy task data */
    secondary->num_tasks = primary->num_tasks;
    uint32_t i;
    for (i = 0; i < primary->num_tasks && i < PAX_MAX_TASKS_PER_CONTROLLER; i++) {
        secondary->tasks[i] = primary->tasks[i];
    }

    secondary->redun_sync = PAX_REDUN_SYNC_COMPLETE;
    secondary->redun_role = PAX_REDUN_ROLE_SECONDARY;

    return 0;
}

/* ---------------------------------------------------------------------------
 * L2: Controller Health Diagnosis
 *
 * Checks multiple health indicators and returns the worst:
 *   - CPU utilization > CRIT: FAILURE
 *   - CPU > WARN: CHECK_FUNCTION
 *   - Temperature > MAX: FAILURE
 *   - Battery < LOW: MAINTENANCE_REQUIRED
 *   - Scan overruns detected: OUT_OF_SPEC
 *   - Major fault: FAILURE
 * ------------------------------------------------------------------------- */

pax_health_t pax_controller_diagnose(const pax_controller_t *ctrl)
{
    if (ctrl == NULL) return PAX_HEALTH_FAILURE;

    pax_health_t result = PAX_HEALTH_OK;

    /* Critical: major fault */
    if (ctrl->major_fault_code != 0) {
        return PAX_HEALTH_FAILURE;
    }

    /* Critical: CPU utilization */
    if (ctrl->cpu_utilization_pct >= PAX_CPU_UTILIZATION_CRIT_PCT) {
        return PAX_HEALTH_FAILURE;
    }
    if (ctrl->cpu_utilization_pct >= PAX_CPU_UTILIZATION_WARN_PCT) {
        result = PAX_HEALTH_OUT_OF_SPEC;
    }

    /* Critical: ambient temperature */
    if (ctrl->ambient_temp_c > PAX_AMBIENT_MAX_C) {
        return PAX_HEALTH_FAILURE;
    }

    /* Warning: battery */
    if (ctrl->battery_voltage_v < PAX_BATTERY_LOW_V) {
        if (result < PAX_HEALTH_MAINTENANCE_REQUIRED) {
            result = PAX_HEALTH_MAINTENANCE_REQUIRED;
        }
    }

    /* Check scan overruns */
    uint32_t i;
    for (i = 0; i < ctrl->num_tasks; i++) {
        if (ctrl->tasks[i].overrun_count > 0) {
            if (result < PAX_HEALTH_OUT_OF_SPEC) {
                result = PAX_HEALTH_OUT_OF_SPEC;
            }
            break;
        }
    }

    /* Minor faults increase severity */
    if (ctrl->minor_fault_count > 100) {
        if (result < PAX_HEALTH_CHECK_FUNCTION) {
            result = PAX_HEALTH_CHECK_FUNCTION;
        }
    }

    return result;
}

/* ---------------------------------------------------------------------------
 * L5: Rate Monotonic Schedulability Check
 *
 * Liu & Layland (1973) upper bound:
 *   U <= n * (2^(1/n) - 1)
 *
 * Where U = sum(C_i / T_i) for n periodic tasks.
 *
 * For n->infinity: U_max -> ln(2) ~ 0.693 (69.3%)
 *
 * This is a sufficient but not necessary condition.
 * A system can be schedulable even if U exceeds this bound.
 * ------------------------------------------------------------------------- */

int pax_controller_check_schedulability(const pax_controller_t *ctrl)
{
    if (ctrl == NULL) return -1;

    /* Count non-continuous, non-inhibited tasks */
    uint32_t n = 0;
    double total_utilization = 0.0;
    uint32_t i;

    for (i = 0; i < ctrl->num_tasks; i++) {
        const pax_task_t *task = &ctrl->tasks[i];
        if (task->inhibited || task->type == PAX_TASK_CONTINUOUS) continue;
        if (task->period_ms <= 0.0) continue;

        n++;
        total_utilization += task->measured_scan_ms / task->period_ms;
    }

    if (n == 0) return 0;  /* Only continuous tasks: always schedulable */

    /* RMA upper bound: U <= n * (2^(1/n) - 1) */
    double bound = (double)n * (pow(2.0, 1.0 / (double)n) - 1.0);

    if (total_utilization <= bound) {
        return 0;  /* Definitely schedulable */
    } else {
        return 1;  /* May or may not be schedulable */
    }
}

/* ---------------------------------------------------------------------------
 * L3: Controller Status Dump
 * ------------------------------------------------------------------------- */

void pax_controller_dump(const pax_controller_t *ctrl)
{
    if (ctrl == NULL) {
        printf("Controller: (null)\n");
        return;
    }

    printf("===========================================\n");
    printf("  Controller: %s\n", ctrl->controller_name);
    printf("  Model:      %s\n", ctrl->spec.model_number);
    printf("===========================================\n");
    printf("  Mode:       ");
    switch (ctrl->mode) {
        case PAX_CTRL_MODE_RUN:    printf("RUN\n"); break;
        case PAX_CTRL_MODE_PROGRAM: printf("PROGRAM\n"); break;
        case PAX_CTRL_MODE_FAULT:  printf("FAULT (code %u)\n",
                                          ctrl->major_fault_code); break;
        default: printf("OTHER\n");
    }
    printf("  Redundancy: ");
    switch (ctrl->redun_role) {
        case PAX_REDUN_ROLE_PRIMARY:   printf("PRIMARY\n"); break;
        case PAX_REDUN_ROLE_SECONDARY: printf("SECONDARY\n"); break;
        case PAX_REDUN_ROLE_SOLO:      printf("SOLO\n"); break;
        default: printf("DISQUALIFIED\n");
    }
    printf("  CPU:        %.1f%%\n", ctrl->cpu_utilization_pct);
    printf("  Uptime:     %.1f h\n", ctrl->uptime_hours);
    printf("  Temp:       %.1f C\n", ctrl->ambient_temp_c);
    printf("  Battery:    %.2f V\n", ctrl->battery_voltage_v);
    printf("  Tasks:      %u\n", ctrl->num_tasks);
    printf("  Scans:      %llu\n",
           (unsigned long long)ctrl->total_scan_count);
    printf("  Anomalies:  %llu\n",
           (unsigned long long)ctrl->anomaly_count);
    printf("  Faults:     major=%u minor=%u\n",
           ctrl->major_fault_code, ctrl->minor_fault_count);
    printf("===========================================\n");
}
