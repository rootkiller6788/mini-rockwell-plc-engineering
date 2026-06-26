/**
 * guardlogix_safety_task.h -- Safety Task Execution Model Header
 *
 * Covers L2: Safety task scheduling, watchdog, interleaving with
 * standard tasks, safety task priority model.
 *
 * Covers L3: Engineering structure -- safety task execution timing,
 * scan time analysis, maximum response time calculation.
 *
 * In GuardLogix, the safety task runs at the highest priority
 * independent of standard Logix tasks. It has a dedicated watchdog
 * and separate memory space.
 *
 * Reference: GuardLogix 5580 User Manual 1756-RM099
 *            IEC 61508-2:2010 section 7.4.2.14
 */

#ifndef GUARDLOGIX_SAFETY_TASK_H
#define GUARDLOGIX_SAFETY_TASK_H

#include <stdint.h>
#include "guardlogix_safety.h"

#define GLX_5560S_MAX_TASK_PERIOD_MS  200
#define GLX_5570S_MAX_TASK_PERIOD_MS  100
#define GLX_5580S_MAX_TASK_PERIOD_MS  50
#define GLX_COMPACT_MAX_TASK_PERIOD_MS 20
#define GLX_MIN_SAFETY_TASK_PERIOD_MS 1
#define GLX_DEFAULT_WATCHDOG_MULTIPLIER 3

typedef struct {
    uint32_t cycle_number;
    uint32_t start_timestamp_us;
    uint32_t end_timestamp_us;
    uint32_t execution_time_us;
    uint32_t input_copy_time_us;
    uint32_t logic_time_us;
    uint32_t output_copy_time_us;
    uint32_t cross_check_time_us;
    uint8_t  overrun_flag;
    uint8_t  partner_sync_ok;
} glx_safety_task_record_t;

typedef struct {
    uint32_t total_cycles;
    uint32_t overrun_count;
    uint32_t max_execution_time_us;
    uint32_t min_execution_time_us;
    uint32_t avg_execution_time_us;
    uint32_t last_overrun_cycle;
    uint32_t consecutive_healthy;
    uint32_t max_input_copy_us;
    uint32_t max_logic_us;
    uint32_t max_output_copy_us;
    uint32_t max_cross_check_us;
} glx_safety_task_stats_t;

typedef struct {
    glx_safety_task_type_t type;
    uint32_t period_us;
    uint32_t watchdog_timeout_us;
    uint32_t cycle_count;
    uint32_t next_start_us;
    uint8_t  task_enabled;
    uint8_t  partner_sync_enabled;
    uint8_t  input_image_valid;
    uint8_t  output_image_pending;
    uint32_t input_image_size;
    uint32_t output_image_size;
    glx_safety_task_record_t last_record;
    glx_safety_task_stats_t stats;
    glx_safety_task_record_t *history;
    uint16_t history_size;
    uint16_t history_index;
} glx_safety_task_cb_t;

int glx_safety_task_init(glx_safety_task_cb_t *tcb,
                          glx_safety_task_type_t type,
                          uint32_t period_us,
                          uint16_t history_entries);
int glx_safety_task_start_scan(glx_safety_task_cb_t *tcb,
                                uint32_t timestamp_us);
int glx_safety_task_end_scan(glx_safety_task_cb_t *tcb,
                              uint32_t timestamp_us);
uint32_t glx_safety_task_max_response_time(const glx_safety_task_cb_t *tcb,
                                            uint32_t input_delay_us,
                                            uint32_t output_delay_us,
                                            uint32_t network_rpi_us);
int glx_verify_task_timing_sil(const glx_safety_task_cb_t *tcb,
                                glx_sil_level_t sil,
                                uint32_t process_safety_time_us);
void glx_safety_task_record_phases(glx_safety_task_cb_t *tcb,
                                    uint32_t input_copy_us,
                                    uint32_t logic_us,
                                    uint32_t output_copy_us,
                                    uint32_t cross_check_us);
uint32_t glx_get_max_task_period_ms(glx_controller_family_t family);
int glx_validate_task_period(glx_controller_family_t family,
                              uint32_t period_ms);
void glx_safety_task_reset_stats(glx_safety_task_cb_t *tcb);
uint32_t glx_safety_task_p99_execution_time(const glx_safety_task_cb_t *tcb);
double glx_safety_task_utilization(const glx_safety_task_cb_t *tcb);

#endif /* GUARDLOGIX_SAFETY_TASK_H */
