/**
 * @file pointio_analog.h
 * @brief Analog I/O operations and signal scaling for Point I/O modules.
 *
 * Level: L1 Definitions + L5 Algorithms
 * Reference:
 *   - Rockwell "1734-IE4C Analog Input Module User Manual" (1734-UM003)
 *   - Rockwell "1734-OE2C Analog Output Module User Manual" (1734-UM004)
 *   - IEC 61131-2, Section 5.3 (Analog I/O requirements)
 */

#ifndef POINTIO_ANALOG_H
#define POINTIO_ANALOG_H

#include "pointio_types.h"
#include "pointio_module.h"

/*===========================================================================
 * L1: Analog Channel Configuration
 *===========================================================================*/

/**
 * @brief Analog input channel configuration.
 */
typedef struct {
    uint8_t                 channel;
    pointio_analog_range_t  range;
    pointio_data_format_t   data_format;
    pointio_analog_scaling_t scaling;
    int                     enable_filtering;   /* 1 = enable digital filter */
    double                  filter_time_constant_ms; /* First-order lag time */
    int                     enable_alarm;       /* 1 = enable process alarm */
    double                  alarm_high;         /* High alarm threshold (EU) */
    double                  alarm_high_high;    /* High-high alarm (EU) */
    double                  alarm_low;          /* Low alarm threshold (EU) */
    double                  alarm_low_low;      /* Low-low alarm (EU) */
    double                  alarm_deadband;     /* Hysteresis for alarms (EU) */
    int                     enable_open_wire_detect; /* Broken wire detection */
    char                    alias[32];
} pointio_ai_config_t;

/**
 * @brief Analog output channel configuration.
 */
typedef struct {
    uint8_t                     channel;
    pointio_analog_output_range_t range;
    pointio_data_format_t       data_format;
    pointio_analog_scaling_t    scaling;
    double                      clamp_high_eu;  /* Max allowed output (EU) */
    double                      clamp_low_eu;   /* Min allowed output (EU) */
    pointio_fault_mode_t        fault_mode;
    double                      fault_value_eu; /* Output value on fault (EU) */
    double                      ramp_rate_eu_s; /* Output rate-of-change limit */
    int                         enable_readback; /* Read output current for diag */
    char                        alias[32];
} pointio_ao_config_t;

/*===========================================================================
 * L5: Analog Input Scaling & Conversion
 *===========================================================================*/

/**
 * @brief Convert raw A/D counts to engineering units.
 *
 * Formula: EU = ((Raw - RawLo) / (RawHi - RawLo)) * (EUHi - EULo) + EULo
 *
 * Handles special case of 4-20mA where RawLo != 0 (provides open-wire detection:
 * raw==0 means broken wire).
 *
 * Reference: ISA TR20.00.01, "Specification Forms for Process Measurement"
 *
 * @param raw_count   Raw A/D count value
 * @param scaling      Scaling parameters (range definition)
 * @param eu_value     Output: engineering units value
 * @return 0 on success, -1 if scaling.raw_high == scaling.raw_low (division by zero)
 */
int pointio_ai_raw_to_eu(uint16_t raw_count,
    const pointio_analog_scaling_t *scaling, double *eu_value);

/**
 * @brief Convert engineering units to raw counts (for comparison or output).
 *
 * Inverse of raw_to_eu. Clamps to [raw_low, raw_high].
 *
 * @param eu_value    Engineering units value
 * @param scaling     Scaling parameters
 * @param raw_count   Output: raw A/D count equivalent
 * @return 0 on success, -1 if invalid scaling
 */
int pointio_ai_eu_to_raw(double eu_value,
    const pointio_analog_scaling_t *scaling, uint16_t *raw_count);

/*===========================================================================
 * L5: Analog Input Channel Operations
 *===========================================================================*/

/**
 * @brief Read a single analog input channel and return the value in EU.
 *
 * Reads the raw value from the module data image, applies scaling,
 * and optionally runs through a first-order digital filter.
 *
 * @param mod       The analog input module
 * @param channel   Channel number (0-based)
 * @param config    Channel configuration (for scaling & filtering)
 * @param value     Output: value in engineering units
 * @return 0 on success, -1 on error (out of range, open wire)
 */
int pointio_ai_read_eu(pointio_module_t *mod, uint8_t channel,
    const pointio_ai_config_t *config, double *value);

/**
 * @brief Read raw A/D count from an analog input channel.
 *
 * Direct access to the 16-bit integer data from the module image.
 * Point I/O analog modules use INT data type (signed 16-bit).
 *
 * @param mod       The analog input module
 * @param channel   Channel number
 * @param raw_count Output: raw signed 16-bit count
 * @param quality   Output: data quality (0=good, >0=degraded)
 * @return 0 on success, -1 on error
 */
int pointio_ai_read_raw(pointio_module_t *mod, uint8_t channel,
    int16_t *raw_count, uint8_t *quality);

/**
 * @brief Apply a first-order IIR filter to an analog input value.
 *
 * Filter equation: y[k] = alpha * x[k] + (1-alpha) * y[k-1]
 * where alpha = Ts / (tau + Ts), Ts = sample period, tau = time constant.
 *
 * The filter state is stored per-channel in prev_filtered.
 *
 * @param raw_value     New raw measurement
 * @param prev_filtered Previous filtered output (in/out, updated in place)
 * @param tau_ms         Filter time constant in ms
 * @param ts_ms          Sample period in ms
 * @param filtered       Output: filtered value
 * @return 0 on success
 */
int pointio_ai_filter(double raw_value, double *prev_filtered,
    double tau_ms, double ts_ms, double *filtered);

/*===========================================================================
 * L5: Analog Output Channel Operations
 *===========================================================================*/

/**
 * @brief Write an engineering-units value to an analog output channel.
 *
 * Converts EU to raw counts, applies clamping, and writes to module
 * output data image. Supports rate-of-change limiting.
 *
 * @param mod       The analog output module
 * @param channel   Channel number
 * @param config    Channel configuration
 * @param value_eu  Desired output in engineering units
 * @return 0 on success, -1 on error (out of range, clamping applied)
 */
int pointio_ao_write_eu(pointio_module_t *mod, uint8_t channel,
    const pointio_ao_config_t *config, double value_eu);

/**
 * @brief Write raw count directly to an analog output channel.
 *
 * Bypasses scaling. Useful for testing and calibration.
 *
 * @param mod       The analog output module
 * @param channel   Channel number
 * @param raw_count Raw signed 16-bit value
 * @return 0 on success, -1 on error
 */
int pointio_ao_write_raw(pointio_module_t *mod, uint8_t channel,
    int16_t raw_count);

/**
 * @brief Ramp an analog output to a target value at a controlled rate.
 *
 * Smoothly transitions from current output to target_eu at
 * the specified ramp_rate (EU/sec). This prevents sudden changes
 * that could shock the process or actuator.
 *
 * @param mod        The analog output module
 * @param channel    Channel number
 * @param config     Channel configuration
 * @param target_eu  Target value in EU
 * @param ramp_rate  Rate of change in EU/second
 * @param dt_s       Time step since last call in seconds
 * @param current_eu Output: the value actually written this step
 * @return 0 on success (still ramping), 1 if reached target, -1 on error
 */
int pointio_ao_ramp_to(pointio_module_t *mod, uint8_t channel,
    const pointio_ao_config_t *config, double target_eu,
    double ramp_rate, double dt_s, double *current_eu);

/*===========================================================================
 * L6: Canonical Problem - 4-20mA Loop Integrity
 *===========================================================================*/

/**
 * @brief Check 4-20mA loop integrity.
 *
 * Returns diagnostic information about the analog loop:
 *   - Open wire: raw count near zero (0-1mA)
 *   - Short circuit: raw count at maximum (>20.5mA)
 *   - Normal range: 4-20mA (±0.5mA margin)
 *   - Under-range: 2-4mA
 *   - Over-range: 20-21mA
 *
 * @param raw_count  Raw A/D reading
 * @param scaling    Channel scaling (must be 4-20mA range type)
 * @param status     Output: 0=OK, 1=open wire, 2=short, 3=under, 4=over
 * @param loop_ma    Output: approximate loop current in mA
 * @return 0 on success, -1 on wrong range type
 */
int pointio_ai_check_loop_integrity(uint16_t raw_count,
    const pointio_analog_scaling_t *scaling,
    int *status, double *loop_ma);

/**
 * @brief Perform open-wire detection on a 4-20mA input.
 *
 * In 4-20mA mode, a raw count of 0 or near 0 indicates an open circuit
 * because a properly functioning loop always carries at least 4mA.
 *
 * @param raw_count    Raw A/D reading
 * @param scaling      Must be 4-20mA range
 * @param is_open      Output: 1 if open wire detected
 * @return 0 on success
 */
int pointio_ai_detect_open_wire(uint16_t raw_count,
    const pointio_analog_scaling_t *scaling, int *is_open);

/*===========================================================================
 * L5: Thermocouple Cold Junction Compensation
 *===========================================================================*/

/**
 * @brief Apply cold junction compensation to a thermocouple reading.
 *
 * For Point I/O thermocouple modules (1734-IR2, 1734-IT2I), the CJC
 * temperature is sensed at the terminal block and must be algebraically
 * added to the measured thermocouple voltage.
 *
 * Reference: ASTM E230, "Standard Specification for Thermocouples"
 *
 * @param tc_voltage_mv    Measured TC voltage (mV)
 * @param cjc_temp_c       Cold junction temperature (degC)
 * @param tc_type          0=J, 1=K, 2=T, 3=E
 * @param temp_c           Output: compensated temperature
 * @return 0 on success, -1 if unsupported TC type
 */
int pointio_ai_cold_junction_compensation(double tc_voltage_mv,
    double cjc_temp_c, int tc_type, double *temp_c);

/*===========================================================================
 * L5: Analog Alarm Processing
 *===========================================================================*/

/**
 * @brief Evaluate process alarms on an analog input channel.
 *
 * Checks current value against configured alarm thresholds with hysteresis.
 * Implements standard ISA-18.2 alarm management: HH, H, L, LL with deadband.
 *
 * @param value_eu   Current process value in EU
 * @param config     Channel configuration with alarm thresholds
 * @param prev_alarm Previous alarm state (for hysteresis)
 * @param alarm      Output: 0=normal, 1=H, 2=HH, 3=L, 4=LL
 * @return 0 on success
 */
int pointio_ai_evaluate_alarms(double value_eu,
    const pointio_ai_config_t *config, int *prev_alarm, int *alarm);

#endif /* POINTIO_ANALOG_H */
