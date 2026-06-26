/**
 * guardlogix_safety_io.h -- GuardLogix Safety I/O Module Definitions
 *
 * Covers L1: Safety I/O channel types, dual-channel architectures,
 * pulse testing, discrepancy time monitoring.
 *
 * Covers L2: Safety input evaluation (equivalent vs complementary),
 * test output signal generation, channel-to-channel correlation.
 *
 * GuardLogix safety I/O uses dual-channel configurations to achieve
 * SIL 3 / PL e. Each safety input reads two physical channels with
 * continuous discrepancy monitoring. Safety outputs use periodic
 * pulse testing to detect stuck-at faults.
 *
 * Reference: 1734-IB8S / 1791ES User Manuals
 *            IEC 61508-2:2010 section 7.4.5
 */

#ifndef GUARDLOGIX_SAFETY_IO_H
#define GUARDLOGIX_SAFETY_IO_H

#include <stdint.h>
#include "guardlogix_safety.h"

typedef enum {
    GLX_INPUT_EQUIVALENT     = 0,
    GLX_INPUT_COMPLEMENTARY  = 1,
    GLX_INPUT_SINGLE_CHANNEL = 2,
    GLX_INPUT_DUAL_EQUIV     = 3,
    GLX_INPUT_DUAL_COMP      = 4
} glx_safety_input_mode_t;

typedef enum {
    GLX_OUTPUT_PULSE_NONE     = 0,
    GLX_OUTPUT_PULSE_LIGHT    = 1,
    GLX_OUTPUT_PULSE_DARK     = 2,
    GLX_OUTPUT_PULSE_BOTH     = 3
} glx_output_pulse_mode_t;

typedef struct {
    uint8_t     channel_a_raw;
    uint8_t     channel_b_raw;
    uint8_t     evaluated_value;
    uint8_t     channel_fault;
    uint8_t     pulse_test_active;
    uint32_t    discrepancy_timer_ms;
    uint32_t    discrepancy_limit_ms;
    uint32_t    last_change_timestamp;
    uint8_t     filter_samples;
    uint8_t     filter_state;
    glx_safety_input_mode_t mode;
} glx_safety_input_point_t;

typedef struct {
    uint16_t    module_id;
    uint16_t    snn;
    glx_safety_io_type_t io_type;
    uint8_t     num_channels;
    uint8_t     module_fault;
    uint8_t     config_locked;
    uint32_t    config_signature;
    uint32_t    last_scan_timestamp;
    glx_safety_input_point_t *channels;
    uint32_t    global_discrepancy_limit_ms;
} glx_safety_input_module_t;

typedef struct {
    uint8_t     commanded_state;
    uint8_t     readback_state;
    uint8_t     output_fault;
    uint8_t     pulse_test_in_progress;
    glx_output_pulse_mode_t pulse_mode;
    uint32_t    pulse_width_us;
    uint32_t    pulse_period_ms;
    uint32_t    last_pulse_timestamp;
    uint8_t     consecutive_faults;
} glx_safety_output_point_t;

typedef struct {
    uint16_t    module_id;
    uint16_t    snn;
    glx_safety_io_type_t io_type;
    uint8_t     num_channels;
    uint8_t     module_fault;
    uint8_t     config_locked;
    uint32_t    config_signature;
    uint32_t    last_scan_timestamp;
    glx_safety_output_point_t *channels;
    uint32_t    global_pulse_width_us;
    uint32_t    global_pulse_period_ms;
} glx_safety_output_module_t;

typedef struct {
    int32_t     channel_a_value;
    int32_t     channel_b_value;
    int32_t     evaluated_value;
    uint8_t     overrange_a;
    uint8_t     overrange_b;
    uint8_t     discrepancy_fault;
    uint8_t     wire_off_fault;
    uint32_t    discrepancy_tolerance;
    uint32_t    engineering_units_lo;
    uint32_t    engineering_units_hi;
    uint32_t    raw_adc_lo;
    uint32_t    raw_adc_hi;
    double      scaled_value;
} glx_safety_analog_input_t;

int glx_safety_input_init(glx_safety_input_point_t *point,
                           glx_safety_input_mode_t mode,
                           uint32_t discrepancy_ms,
                           uint8_t filter_samples);
int glx_safety_output_init(glx_safety_output_point_t *point,
                            glx_output_pulse_mode_t pulse_mode,
                            uint32_t pulse_width_us,
                            uint32_t pulse_period_ms);
int glx_safety_input_update(glx_safety_input_point_t *point,
                             int channel_a, int channel_b,
                             uint32_t timestamp_ms);
int glx_safety_output_pulse_test(glx_safety_output_point_t *point,
                                  uint32_t timestamp_ms);
int glx_safety_analog_evaluate(glx_safety_analog_input_t *analog,
                                int32_t chan_a_raw, int32_t chan_b_raw);
double glx_analog_scale_eu(const glx_safety_analog_input_t *analog,
                            int32_t raw);
int glx_safety_input_module_init(glx_safety_input_module_t *module,
                                  uint8_t num_channels, uint16_t snn,
                                  uint32_t disc_limit_ms);
int glx_safety_output_module_init(glx_safety_output_module_t *module,
                                   uint8_t num_channels, uint16_t snn);
int glx_safety_input_module_scan(glx_safety_input_module_t *module,
                                  const uint8_t *raw_values,
                                  uint32_t timestamp_ms,
                                  uint16_t *fault_mask_out);
int glx_safety_output_set(glx_safety_output_point_t *point,
                           int state, int readback);
int glx_safety_output_is_healthy(const glx_safety_output_point_t *point);
int glx_safety_output_emergency_shutdown(glx_safety_output_module_t *module);
int glx_verify_io_config_signature(const void *module,
                                    uint32_t expected_sig, int sig_type);

#endif /* GUARDLOGIX_SAFETY_IO_H */
