/**
 * guardlogix_safety_task.c -- Safety Task Execution Implementation
 *
 * Knowledge points:
 *   L2: Safety task scheduling with watchdog monitoring
 *   L3: Cyclic execution model with exact timing
 *   L3: Priority inversion prevention (safety task = highest priority)
 *   L4: IEC 61508-2 timing performance requirements
 *   L5: P99 execution time computation from history buffer
 *   L6: Overrun detection and fault handling
 */

#include "guardlogix_safety_task.h"
#include <string.h>
#include <stdlib.h>

int glx_safety_task_init(glx_safety_task_cb_t *tcb,
                          glx_safety_task_type_t type,
                          uint32_t period_us,
                          uint16_t history_entries)
{
    if (!tcb) return -1;
    if (period_us < 1000) return -1; /* Minimum 1 ms */
    if (period_us > 500000) return -1; /* Maximum 500 ms */

    memset(tcb, 0, sizeof(*tcb));

    tcb->type = type;
    tcb->period_us = period_us;
    tcb->watchdog_timeout_us = period_us * GLX_DEFAULT_WATCHDOG_MULTIPLIER;
    tcb->task_enabled = 1;
    tcb->partner_sync_enabled = 1;
    tcb->input_image_valid = 0;
    tcb->output_image_pending = 0;

    /* Allocate history ring buffer */
    if (history_entries == 0) history_entries = 16;
    tcb->history_size = history_entries;
    tcb->history = (glx_safety_task_record_t*)calloc(
        history_entries, sizeof(glx_safety_task_record_t));
    if (!tcb->history) return -1;

    tcb->history_index = 0;

    /* Initialize statistics */
    memset(&tcb->stats, 0, sizeof(tcb->stats));
    tcb->stats.min_execution_time_us = UINT32_MAX;

    return 0;
}

int glx_safety_task_start_scan(glx_safety_task_cb_t *tcb,
                                uint32_t timestamp_us)
{
    if (!tcb) return -1;
    if (!tcb->task_enabled) return -1;

    /* Check for overrun: if we missed the scheduled start */
    if (tcb->next_start_us > 0 && timestamp_us > tcb->next_start_us) {
        uint32_t overrun_by = timestamp_us - tcb->next_start_us;

        if (overrun_by > tcb->watchdog_timeout_us) {
            /* Watchdog timeout -- critical safety fault */
            tcb->stats.overrun_count++;
            tcb->stats.last_overrun_cycle = tcb->cycle_count;
            tcb->last_record.overrun_flag = 1;
            return 1; /* Overrun */
        }
    }

    /* Record start time */
    tcb->last_record.cycle_number = tcb->cycle_count;
    tcb->last_record.start_timestamp_us = timestamp_us;
    tcb->last_record.overrun_flag = 0;

    /* Schedule next start */
    tcb->next_start_us = timestamp_us + tcb->period_us;

    return 0;
}

int glx_safety_task_end_scan(glx_safety_task_cb_t *tcb,
                              uint32_t timestamp_us)
{
    if (!tcb) return -1;

    tcb->last_record.end_timestamp_us = timestamp_us;

    /* Compute execution time with overflow protection */
    if (timestamp_us >= tcb->last_record.start_timestamp_us) {
        tcb->last_record.execution_time_us =
            timestamp_us - tcb->last_record.start_timestamp_us;
    } else {
        /* Timer overflow -- use period as fallback */
        tcb->last_record.execution_time_us = tcb->period_us;
    }

    uint32_t et = tcb->last_record.execution_time_us;

    /* Update statistics */
    tcb->stats.total_cycles++;
    tcb->cycle_count++;

    if (et > tcb->stats.max_execution_time_us)
        tcb->stats.max_execution_time_us = et;

    if (et < tcb->stats.min_execution_time_us)
        tcb->stats.min_execution_time_us = et;

    /* Running average: avg = avg * (n-1)/n + new_val / n */
    uint32_t n = tcb->stats.total_cycles;
    tcb->stats.avg_execution_time_us =
        (uint32_t)(((uint64_t)tcb->stats.avg_execution_time_us * (n - 1) + et) / n);

    /* Track max phase times */
    if (tcb->last_record.input_copy_time_us > tcb->stats.max_input_copy_us)
        tcb->stats.max_input_copy_us = tcb->last_record.input_copy_time_us;
    if (tcb->last_record.logic_time_us > tcb->stats.max_logic_us)
        tcb->stats.max_logic_us = tcb->last_record.logic_time_us;
    if (tcb->last_record.output_copy_time_us > tcb->stats.max_output_copy_us)
        tcb->stats.max_output_copy_us = tcb->last_record.output_copy_time_us;
    if (tcb->last_record.cross_check_time_us > tcb->stats.max_cross_check_us)
        tcb->stats.max_cross_check_us = tcb->last_record.cross_check_time_us;

    /* Track consecutive healthy cycles */
    if (tcb->last_record.overrun_flag == 0) {
        tcb->stats.consecutive_healthy++;
    } else {
        tcb->stats.consecutive_healthy = 0;
    }

    /* Store in history ring buffer */
    if (tcb->history && tcb->history_size > 0) {
        tcb->history[tcb->history_index] = tcb->last_record;
        tcb->history_index = (tcb->history_index + 1) % tcb->history_size;
    }

    return 0;
}

uint32_t glx_safety_task_max_response_time(const glx_safety_task_cb_t *tcb,
                                            uint32_t input_delay_us,
                                            uint32_t output_delay_us,
                                            uint32_t network_rpi_us)
{
    if (!tcb) return 0;

    /* Max Response Time = 2 * Period + Input_Delay + Output_Delay
     *                     + Network_RPI + Cross_Check_Time
     */
    uint32_t mrt = 2 * tcb->period_us
                 + input_delay_us
                 + output_delay_us
                 + network_rpi_us
                 + tcb->stats.max_cross_check_us;

    return mrt;
}

int glx_verify_task_timing_sil(const glx_safety_task_cb_t *tcb,
                                glx_sil_level_t sil,
                                uint32_t process_safety_time_us)
{
    if (!tcb) return 0;

    uint32_t response_time = tcb->period_us * 2
                           + tcb->stats.max_input_copy_us
                           + tcb->stats.max_output_copy_us
                           + tcb->stats.max_cross_check_us;

    /* SIL-dependent timing margins per IEC 61508-2 */
    switch (sil) {
    case GLX_SIL_1:
        return (response_time <= process_safety_time_us * 10) ? 1 : 0;
    case GLX_SIL_2:
        return (response_time <= process_safety_time_us * 5) ? 1 : 0;
    case GLX_SIL_3:
        return (response_time <= process_safety_time_us * 2) ? 1 : 0;
    case GLX_SIL_4:
        return (response_time <= process_safety_time_us) ? 1 : 0;
    default:
        return 0;
    }
}

void glx_safety_task_record_phases(glx_safety_task_cb_t *tcb,
                                    uint32_t input_copy_us,
                                    uint32_t logic_us,
                                    uint32_t output_copy_us,
                                    uint32_t cross_check_us)
{
    if (!tcb) return;
    tcb->last_record.input_copy_time_us = input_copy_us;
    tcb->last_record.logic_time_us = logic_us;
    tcb->last_record.output_copy_time_us = output_copy_us;
    tcb->last_record.cross_check_time_us = cross_check_us;
}

uint32_t glx_get_max_task_period_ms(glx_controller_family_t family)
{
    switch (family) {
    case GLX_CTRL_5560S:  return GLX_5560S_MAX_TASK_PERIOD_MS;
    case GLX_CTRL_5570S:  return GLX_5570S_MAX_TASK_PERIOD_MS;
    case GLX_CTRL_5580S:  return GLX_5580S_MAX_TASK_PERIOD_MS;
    case GLX_CTRL_COMPACT: return GLX_COMPACT_MAX_TASK_PERIOD_MS;
    default:              return 100;
    }
}

int glx_validate_task_period(glx_controller_family_t family,
                              uint32_t period_ms)
{
    if (period_ms < GLX_MIN_SAFETY_TASK_PERIOD_MS) return 0;

    uint32_t max_period = glx_get_max_task_period_ms(family);
    return (period_ms <= max_period) ? 1 : 0;
}

void glx_safety_task_reset_stats(glx_safety_task_cb_t *tcb)
{
    if (!tcb) return;
    tcb->stats.max_execution_time_us = 0;
    tcb->stats.min_execution_time_us = UINT32_MAX;
    tcb->stats.avg_execution_time_us = 0;
    tcb->stats.consecutive_healthy = 0;
    tcb->stats.max_input_copy_us = 0;
    tcb->stats.max_logic_us = 0;
    tcb->stats.max_output_copy_us = 0;
    tcb->stats.max_cross_check_us = 0;
}

/**
 * Quicksort comparison function for ascending uint32_t.
 */
static int uint32_cmp_asc(const void *a, const void *b)
{
    uint32_t va = *(const uint32_t *)a;
    uint32_t vb = *(const uint32_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

uint32_t glx_safety_task_p99_execution_time(const glx_safety_task_cb_t *tcb)
{
    if (!tcb || !tcb->history || tcb->history_size == 0) return 0;

    /* Collect all execution times from the ring buffer */
    uint32_t *times = (uint32_t*)malloc(
        tcb->history_size * sizeof(uint32_t));
    if (!times) return 0;

    uint16_t count = 0;
    for (uint16_t i = 0; i < tcb->history_size; i++) {
        if (tcb->history[i].execution_time_us > 0) {
            times[count++] = tcb->history[i].execution_time_us;
        }
    }

    if (count == 0) {
        free(times);
        return 0;
    }

    /* Sort to find percentile */
    qsort(times, count, sizeof(uint32_t), uint32_cmp_asc);

    /* P99 index = ceil(0.99 * count) - 1 */
    uint16_t p99_index = (uint16_t)((0.99 * count) + 0.5);
    if (p99_index >= count) p99_index = count - 1;

    uint32_t result = times[p99_index];
    free(times);
    return result;
}

double glx_safety_task_utilization(const glx_safety_task_cb_t *tcb)
{
    if (!tcb || tcb->period_us == 0) return 0.0;
    return (double)tcb->stats.avg_execution_time_us / (double)tcb->period_us;
}
