/**
 * @file plantpax_io_subsystem.h
 * @brief PlantPAx I/O Subsystem ? Module Types, Signal Processing, Scaling, Fault Detection
 *
 * Module: mini-rockwell-plantpax-dcs
 * Knowledge Coverage: L1 Definitions, L3 I/O Architecture, L4 NAMUR NE43, L5 Signal Processing
 *
 * The PlantPAx I/O subsystem connects field instrumentation to the controller.
 * Key I/O families:
 *   - 1756 ControlLogix I/O: Chassis-based, high density, hot-swappable
 *   - 5069 CompactLogix I/O: Compact, integrated with controller
 *   - FLEX 5000: Distributed I/O with 1 Gbps EtherNet/IP
 *   - 1734 POINT I/O: Low channel count, distributed
 *
 * Signal Types:
 *   - AI: 4-20 mA (NAMUR NE43), 0-10V, RTD, Thermocouple
 *   - AO: 4-20 mA, 0-10V, HART pass-through
 *   - DI: 24VDC, 120VAC, NAMUR proximity sensor
 *   - DO: Relay, transistor, triac
 *
 * NAMUR NE43 Fault Detection:
 *   - 0-3.6 mA: Under-range / wire break
 *   - 3.6-3.8 mA: Sensor failure low
 *   - 3.8-20.5 mA: Normal operating range
 *   - 20.5-21.0 mA: Sensor failure high
 *   - >21.0 mA: Over-range / short circuit
 *
 * Reference:
 *   Rockwell 1756-TD001 Technical Data
 *   NAMUR NE43 Standardization of Signal Levels
 *   ISA-5.1 Instrumentation Symbols and Identification
 * Curriculum: Berkeley EE C128 Mechatronics, RMIT Aachen, Purdue ME 575
 */

#ifndef PLANTPAX_IO_SUBSYSTEM_H
#define PLANTPAX_IO_SUBSYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "plantpax_architecture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1: I/O Module Core Definitions
 * ------------------------------------------------------------------------- */

/** Signal direction */
typedef enum {
    PAX_IO_DIR_INPUT = 0,
    PAX_IO_DIR_OUTPUT = 1,
    PAX_IO_DIR_BIDIRECTIONAL = 2
} pax_io_direction_t;

/** Analog signal type */
typedef enum {
    PAX_ANALOG_4_20MA = 0,           /**< 4-20 mA current loop */
    PAX_ANALOG_0_20MA = 1,           /**< 0-20 mA */
    PAX_ANALOG_0_10V = 2,            /**< 0-10 VDC */
    PAX_ANALOG_MINUS_10_10V = 3,    /**< +/- 10 VDC */
    PAX_ANALOG_0_5V = 4,             /**< 0-5 VDC */
    PAX_ANALOG_1_5V = 5,             /**< 1-5 VDC */
    PAX_ANALOG_RTD_PT100 = 6,        /**< PT100 RTD, 3/4 wire */
    PAX_ANALOG_RTD_PT1000 = 7,       /**< PT1000 RTD */
    PAX_ANALOG_TC_K = 8,             /**< Thermocouple Type K */
    PAX_ANALOG_TC_J = 9,             /**< Thermocouple Type J */
    PAX_ANALOG_TC_T = 10,            /**< Thermocouple Type T */
    PAX_ANALOG_TC_E = 11,            /**< Thermocouple Type E */
    PAX_ANALOG_TC_S = 12,            /**< Thermocouple Type S (high temp) */
    PAX_ANALOG_HART = 13,            /**< HART on 4-20 mA */
    PAX_ANALOG_POTENTIOMETER = 14    /**< Potentiometer / resistance */
} pax_analog_signal_type_t;

/** Digital signal type */
typedef enum {
    PAX_DIGITAL_24VDC_SINK = 0,
    PAX_DIGITAL_24VDC_SOURCE = 1,
    PAX_DIGITAL_120VAC = 2,
    PAX_DIGITAL_230VAC = 3,
    PAX_DIGITAL_DRY_CONTACT = 4,
    PAX_DIGITAL_NAMUR = 5,           /**< NAMUR proximity sensor (EN 60947-5-6) */
    PAX_DIGITAL_TTL = 6,
    PAX_DIGITAL_RELAY_NO = 7,        /**< Relay normally open */
    PAX_DIGITAL_RELAY_NC = 8         /**< Relay normally closed */
} pax_digital_signal_type_t;

/** NAMUR NE43 sensor fault states */
typedef enum {
    PAX_NAMUR_NORMAL = 0,            /**< Signal in normal range */
    PAX_NAMUR_UNDER_RANGE = 1,       /**< 0-3.6 mA: Under-range or wire break */
    PAX_NAMUR_SENSOR_FAULT_LOW = 2,  /**< 3.6-3.8 mA: Sensor malfunction low */
    PAX_NAMUR_SATURATION_LOW = 3,    /**< 3.8-4.0 mA: Saturation low */
    PAX_NAMUR_SATURATION_HIGH = 4,   /**< 20.0-20.5 mA: Saturation high */
    PAX_NAMUR_SENSOR_FAULT_HIGH = 5, /**< 20.5-21.0 mA: Sensor malfunction high */
    PAX_NAMUR_OVER_RANGE = 6         /**< > 21.0 mA: Over-range or short */
} pax_namur_fault_t;

/** I/O module quality */
typedef enum {
    PAX_IO_QUALITY_GOOD = 0,
    PAX_IO_QUALITY_UNCERTAIN = 1,
    PAX_IO_QUALITY_BAD = 2,
    PAX_IO_QUALITY_MANUAL = 3,       /**< Force / manual override */
    PAX_IO_QUALITY_OFFLINE = 4
} pax_io_quality_t;

/* ---------------------------------------------------------------------------
 * L1: I/O Data Structures
 * ------------------------------------------------------------------------- */

/** Analog input channel configuration */
typedef struct {
    uint32_t channel_id;
    char tag_name[64];
    pax_analog_signal_type_t signal_type;
    double range_min_eu;             /**< Minimum engineering units */
    double range_max_eu;             /**< Maximum engineering units */
    double range_min_raw;            /**< Minimum raw ADC value */
    double range_max_raw;            /**< Maximum raw ADC value */
    char engineering_unit[16];       /**< e.g., "degC", "bar", "m3/h" */
    double filter_time_constant_s;   /**< First-order filter time constant */
    double alarm_hihi;               /**< High-high alarm limit */
    double alarm_hi;                 /**< High alarm limit */
    double alarm_lo;                 /**< Low alarm limit */
    double alarm_lolo;               /**< Low-low alarm limit */
    double alarm_deadband;
    bool square_root_extraction;     /**< For DP flow transmitters */
    bool hart_enabled;               /**< HART communication active */
    double last_raw_value;
    double last_scaled_value;
    pax_io_quality_t quality;
    pax_namur_fault_t namur_state;
    uint64_t update_count;
} pax_ai_channel_t;

/** Analog output channel configuration */
typedef struct {
    uint32_t channel_id;
    char tag_name[64];
    pax_analog_signal_type_t signal_type;
    double range_min_eu;
    double range_max_eu;
    double range_min_raw;
    double range_max_raw;
    char engineering_unit[16];
    double commanded_eu;
    double actual_raw;
    double slew_rate_limit_eu_s;     /**< Maximum rate of change */
    double fault_value_eu;           /**< Fail-safe output value */
    bool clamped;                    /**< Output at saturation limit */
    pax_io_quality_t quality;
} pax_ao_channel_t;

/** Digital input channel configuration */
typedef struct {
    uint32_t channel_id;
    char tag_name[64];
    pax_digital_signal_type_t signal_type;
    bool normally_closed;            /**< TRUE if NC contact (inverted logic) */
    double on_debounce_ms;           /**< Debounce on delay */
    double off_debounce_ms;          /**< Debounce off delay */
    bool alarm_on_true;              /**< Generate alarm when TRUE */
    bool alarm_on_false;             /**< Generate alarm when FALSE */
    bool last_state;
    bool debounced_state;
    uint32_t transition_count;
    pax_io_quality_t quality;
} pax_di_channel_t;

/** Digital output channel configuration */
typedef struct {
    uint32_t channel_id;
    char tag_name[64];
    pax_digital_signal_type_t signal_type;
    bool commanded_state;
    bool actual_state;               /**< Readback verification */
    bool fault_state;                /**< Fail-safe output state */
    double min_on_time_ms;           /**< Minimum pulse width */
    double min_off_time_ms;          /**< Minimum off time */
    pax_io_quality_t quality;
} pax_do_channel_t;

/** I/O module descriptor */
typedef struct {
    uint32_t module_id;
    pax_io_family_t family;
    char module_name[64];
    pax_io_direction_t direction;
    uint16_t slot;
    uint32_t num_channels;
    bool hot_swap;
    double update_rate_ms;           /**< RPI for this module */
    pax_health_t health;
    pax_system_node_t node;
    uint64_t total_scan_count;
    uint64_t error_count;
} pax_io_module_t;

/* ---------------------------------------------------------------------------
 * L1: I/O Constants
 * ------------------------------------------------------------------------- */

#define PAX_NAMUR_UNDER_RANGE_MA       3.6
#define PAX_NAMUR_FAULT_LOW_MA         3.8
#define PAX_NAMUR_NORMAL_LOW_MA        4.0
#define PAX_NAMUR_NORMAL_HIGH_MA       20.0
#define PAX_NAMUR_FAULT_HIGH_MA        20.5
#define PAX_NAMUR_OVER_RANGE_MA        21.0
#define PAX_MAX_AI_CHANNELS_PER_MODULE 16
#define PAX_MAX_AO_CHANNELS_PER_MODULE 16
#define PAX_MAX_DI_CHANNELS_PER_MODULE 32
#define PAX_MAX_DO_CHANNELS_PER_MODULE 32
#define PAX_ADC_RESOLUTION_BITS        16
#define PAX_ADC_MAX_COUNTS             65535

/* ---------------------------------------------------------------------------
 * L3/L5: I/O Subsystem API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize an analog input channel with standard 4-20 mA configuration.
 */
void pax_ai_channel_init(pax_ai_channel_t *ch, uint32_t id,
                          const char *tag, double min_eu, double max_eu,
                          const char *unit);

/**
 * @brief Scale raw ADC value to engineering units.
 *
 * Linear scaling: EU = EU_min + (RAW - RAW_min) * (EU_max - EU_min) / (RAW_max - RAW_min)
 *
 * For square root extraction (DP flow):
 *   EU = EU_min + sqrt((RAW - RAW_min) / (RAW_max - RAW_min)) * (EU_max - EU_min)
 */
double pax_ai_scale_raw_to_eu(const pax_ai_channel_t *ch, double raw_value);

/**
 * @brief Scale engineering units to raw value (for AO).
 */
double pax_ao_scale_eu_to_raw(const pax_ao_channel_t *ch, double eu_value);

/**
 * @brief Apply first-order low-pass filter to analog input.
 *
 * Y[n] = Y[n-1] + (Ts / (tau + Ts)) * (X[n] - Y[n-1])
 *
 * @param prev_filtered Previous filtered value
 * @param raw_input New raw input value
 * @param time_constant_s Filter time constant (tau)
 * @param sample_time_s Sample time (Ts)
 * @return New filtered value
 */
double pax_ai_apply_filter(double prev_filtered, double raw_input,
                            double time_constant_s, double sample_time_s);

/**
 * @brief Classify NAMUR NE43 signal level.
 *
 * Maps a 4-20 mA current reading to its NAMUR fault state.
 */
pax_namur_fault_t pax_ai_classify_namur(double current_ma);

/**
 * @brief Check analog input against alarm limits.
 *
 * Evaluates HIHI, HI, LO, LOLO with deadband.
 *
 * @return 0 = no alarm, 1 = LO, 2 = LOLO, 3 = HI, 4 = HIHI
 */
int pax_ai_check_alarms(const pax_ai_channel_t *ch, double value);

/**
 * @brief Apply digital input debounce logic.
 *
 * State change only registered after debounce time elapses
 * with stable signal.
 */
bool pax_di_apply_debounce(const pax_di_channel_t *ch,
                            bool raw_state, double time_since_change_ms);

/**
 * @brief Validate output against readback.
 *
 * For safety-critical outputs with readback verification,
 * checks that the actual state matches the commanded state.
 */
bool pax_do_verify_readback(const pax_do_channel_t *ch, bool readback_state);

/**
 * @brief Apply slew rate limiting to analog output.
 *
 * Limits the rate of change of the output to prevent
 * process upsets from sudden valve movements.
 */
double pax_ao_apply_slew_rate(const pax_ao_channel_t *ch,
                               double target_eu,
                               double current_eu,
                               double dt_s);

/**
 * @brief Convert thermocouple voltage to temperature.
 *
 * Uses ITS-90 polynomial coefficients for Type K thermocouple.
 *
 * @param voltage_mv Thermocouple EMF in mV
 * @param cjc_temp_c Cold junction compensation temperature
 * @return Temperature in Celsius
 */
double pax_tc_voltage_to_temp(double voltage_mv, double cjc_temp_c,
                               pax_analog_signal_type_t tc_type);

/**
 * @brief Convert RTD resistance to temperature.
 *
 * Uses Callendar-Van Dusen equation (IEC 60751).
 *
 * @param resistance_ohm RTD measured resistance
 * @return Temperature in Celsius
 */
double pax_rtd_resistance_to_temp(double resistance_ohm,
                                   pax_analog_signal_type_t rtd_type);

/**
 * @brief Convert HART digital value to engineering units.
 */
double pax_hart_convert_to_eu(uint32_t hart_digital_value,
                               double range_min_eu, double range_max_eu);

/**
 * @brief Dump I/O module status to stdout.
 */
void pax_io_module_dump(const pax_io_module_t *module);

#ifdef __cplusplus
}
#endif

#endif /* PLANTPAX_IO_SUBSYSTEM_H */
