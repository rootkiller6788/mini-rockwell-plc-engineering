/**
 * @file pointio_analog_io.c
 * @brief Analog I/O operations: scaling, filtering, loop integrity, calibration.
 *
 * Level: L5 Algorithms - signal processing for industrial analog I/O
 *
 * Reference:
 *   - Rockwell "1734-IE4C/IE8C Analog Input Module User Manual" (1734-UM003)
 *   - Rockwell "1734-OE2C/OE4C Analog Output User Manual" (1734-UM004)
 *   - ISA TR20.00.01 "Specification Forms for Process Measurement"
 *   - ASTM E230 "Standard Specification for Thermocouples"
 *   - IEC 61131-2 Section 5.3 (Analog I/O)
 *
 * Implements: raw↔EU conversion, 4-20mA loop diagnostics, digital filtering,
 * ramp generation, cold junction compensation, process alarm evaluation.
 */

#include "pointio_analog.h"
#include "pointio_module.h"
#include "pointio_types.h"
#include <string.h>
#include <math.h>

/*===========================================================================
 * Thermocouple voltage-to-temperature polynomial coefficients
 * Reference: NIST ITS-90 Thermocouple Database
 * Valid for Type J: -210 to 1200 degC
 *           Type K: -270 to 1372 degC
 *           Type T: -270 to 400 degC
 *           Type E: -270 to 1000 degC
 *
 * Polynomial form: T = c0 + c1*v + c2*v^2 + ... + cn*v^n
 * where v = thermocouple voltage in mV
 *===========================================================================*/

/* Type J coefficients (Iron-Constantan), range -210 to 760 degC, 6th order */
static const double tc_j_coeffs[] = {
     0.0000000E+00,
     1.9528268E+01,
    -1.2286185E+00,
    -1.0752178E+00,
    -5.9086933E-01,
    -1.7256713E-01,
    -2.8131513E-02,
    -2.3963370E-03,
    -8.3823321E-05
};

/* Type K coefficients (Chromel-Alumel), range -270 to 0 degC, 8th order */
static const double tc_k_coeffs_neg[] = {
     0.0000000E+00,
     2.5173462E+01,
    -1.1662878E+00,
    -1.0833638E+00,
    -8.9773540E-01,
    -3.7342377E-01,
    -8.6632643E-02,
    -1.0450598E-02,
    -5.1920577E-04
};

/* Type K coefficients (Chromel-Alumel), range 0 to 1372 degC, 9th order */
static const double tc_k_coeffs_pos[] = {
     0.0000000E+00,
     2.5083550E+01,
     7.8601060E-02,
    -2.5031310E-01,
     8.3152700E-02,
    -1.2280340E-02,
     9.8040360E-04,
    -4.4130300E-05,
     1.0577340E-06,
    -1.0527550E-08
};

/* Type T coefficients (Copper-Constantan), range -270 to 0 degC, 8th order */
static const double tc_t_coeffs_neg[] = {
     0.0000000E+00,
     2.5949192E+01,
    -2.1316967E-01,
     7.9018692E-01,
     4.2527777E-01,
     1.3304473E-01,
     2.0241446E-02,
     1.2668171E-03,
     0.0000000E+00
};

/* Type T coefficients (Copper-Constantan), range 0 to 400 degC, 9th order */
static const double tc_t_coeffs_pos[] = {
     0.0000000E+00,
     2.5928000E+01,
    -7.6029610E-01,
     4.6377910E-02,
    -2.1653940E-03,
     6.0481440E-05,
    -7.2934220E-07,
     0.0000000E+00,
     0.0000000E+00,
     0.0000000E+00
};

/* Type E coefficients (Chromel-Constantan), range -270 to 0 degC, 8th order */
static const double tc_e_coeffs_neg[] = {
     0.0000000E+00,
     1.6977288E+01,
    -4.3514970E-01,
    -1.5859697E-01,
    -9.2502871E-02,
    -2.6084314E-02,
    -4.1360199E-03,
    -3.4034030E-04,
    -1.1564890E-05
};

/* Type E coefficients (Chromel-Constantan), range 0 to 1000 degC, 9th order */
static const double tc_e_coeffs_pos[] = {
     0.0000000E+00,
     1.7057035E+01,
    -2.3301759E-01,
     6.5435585E-03,
    -7.3562749E-05,
    -1.7896001E-06,
     8.4036165E-08,
    -1.3735879E-09,
     1.0629823E-11,
    -3.2447087E-14
};

/**
 * Evaluate a polynomial using Horner's method.
 *
 * p(x) = c0 + c1*x + c2*x^2 + ... + cn*x^n
 * Evaluated as: c0 + x*(c1 + x*(c2 + ... + x*(cn)))
 *
 * Time complexity: O(n), numerically stable.
 *
 * @param coeffs  Array of n+1 coefficients (coeffs[0] = c0)
 * @param order   Polynomial order n
 * @param x       Input value
 * @return p(x)
 */
static double poly_eval(const double *coeffs, int order, double x)
{
    double result = coeffs[order];
    for (int i = order - 1; i >= 0; i--) {
        result = result * x + coeffs[i];
    }
    return result;
}

/*===========================================================================
 * L5: Analog Input - Raw to Engineering Units Conversion
 *
 * Standard linear scaling formula used in all Rockwell analog modules:
 *
 *   EU = ((Raw - RawLo) / (RawHi - RawLo)) * (EUHi - EULo) + EULo
 *
 * For 4-20mA on 1734-IE4C:
 *   RawLo  = 3277  (20% of 16384, corresponds to 4mA)
 *   RawHi  = 16383 (20mA, module full-scale in 0-20mA mode)
 *   EULo   = user-defined (e.g. 0 for 0%, or -40 for temperature)
 *   EUHi   = user-defined (e.g. 100 for 100%, or 150 for temperature)
 *
 * The offset RawLo > 0 enables open-wire detection: a reading near 0
 * indicates a broken wire (current < 1mA).
 *
 * Reference: 1734-UM003, Appendix A "Scaling"
 *===========================================================================*/

/**
 * Convert raw A/D count to engineering units.
 */
int pointio_ai_raw_to_eu(uint16_t raw_count,
    const pointio_analog_scaling_t *scaling, double *eu_value)
{
    if (!scaling || !eu_value) return -1;

    double rl = scaling->raw_low;
    double rh = scaling->raw_high;
    double el = scaling->eu_low;
    double eh = scaling->eu_high;

    /* Guard against division by zero */
    if (fabs(rh - rl) < 1e-12) return -1;

    /* Handle open-wire detection: raw near 0 in 4-20mA mode */
    if (scaling->range == POINTIO_AI_RANGE_4_20MA && raw_count < (uint16_t)(rl * 0.25)) {
        *eu_value = el - 1e-10;  /* Below range indicator */
        return 0;
    }

    /* Apply scaling formula */
    double raw_d = (double)raw_count;
    *eu_value = ((raw_d - rl) / (rh - rl)) * (eh - el) + el;

    return 0;
}

/**
 * Convert engineering units to raw A/D counts.
 *
 * Inverse of the raw-to-EU formula. The result is clamped to
 * [raw_low, raw_high] to prevent invalid values from being
 * written to the module.
 */
int pointio_ai_eu_to_raw(double eu_value,
    const pointio_analog_scaling_t *scaling, uint16_t *raw_count)
{
    if (!scaling || !raw_count) return -1;

    double rl = scaling->raw_low;
    double rh = scaling->raw_high;
    double el = scaling->eu_low;
    double eh = scaling->eu_high;

    if (fabs(eh - el) < 1e-12) return -1;

    /* Inverse scaling */
    double raw_d = ((eu_value - el) / (eh - el)) * (rh - rl) + rl;

    /* Clamp */
    if (raw_d < rl) raw_d = rl;
    if (raw_d > rh) raw_d = rh;

    *raw_count = (uint16_t)raw_d;
    return 0;
}

/*===========================================================================
 * L5: Analog Input Channel Read
 *===========================================================================*/

/**
 * Read a single analog input channel in engineering units.
 *
 * The raw 16-bit signed integer is extracted from the module input image.
 * For Point I/O analog modules, the data layout is:
 *   Bytes 0-1: Channel 0 low byte, high byte (little-endian INT)
 *   Bytes 2-3: Channel 1
 *   ...
 *   Bytes 2N, 2N+1: Status word (quality, under/over-range flags)
 */
int pointio_ai_read_eu(pointio_module_t *mod, uint8_t channel,
    const pointio_ai_config_t *config, double *value)
{
    if (!mod || !config || !value) return -1;
    if (mod->type != POINTIO_TYPE_ANALOG_INPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    /* Extract 16-bit signed integer from module input image */
    uint8_t byte_offset = channel * 2;
    if (byte_offset + 1 >= mod->input_size_bytes) return -1;

    int16_t raw = (int16_t)(mod->input_data[byte_offset] |
                           (mod->input_data[byte_offset + 1] << 8));

    uint16_t raw_unsigned = (uint16_t)raw;
    uint8_t quality = 0;

    /* Read quality byte (last 2 bytes of input image) */
    if (mod->input_size_bytes >= 2) {
        quality = mod->input_data[mod->input_size_bytes - 2];
    }

    /* Convert to EU */
    if (pointio_ai_raw_to_eu(raw_unsigned, &config->scaling, value) != 0) {
        return -1;
    }

    /* Apply digital filter if enabled */
    if (config->enable_filtering && config->filter_time_constant_ms > 0.0) {
        static double prev_filtered[POINTIO_MAX_CHANNELS] = {0};
        double rpi_ms = (double)mod->rpi_us / 1000.0;
        double filtered;
        if (pointio_ai_filter(*value, &prev_filtered[channel],
                              config->filter_time_constant_ms, rpi_ms, &filtered) == 0) {
            *value = filtered;
        }
    }

    (void)quality;
    return 0;
}

/**
 * Read raw signed 16-bit count directly from the input image.
 */
int pointio_ai_read_raw(pointio_module_t *mod, uint8_t channel,
    int16_t *raw_count, uint8_t *quality)
{
    if (!mod || !raw_count || !quality) return -1;
    if (mod->type != POINTIO_TYPE_ANALOG_INPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    uint8_t byte_offset = channel * 2;
    if (byte_offset + 1 >= mod->input_size_bytes) return -1;

    *raw_count = (int16_t)(mod->input_data[byte_offset] |
                          (mod->input_data[byte_offset + 1] << 8));

    /* Quality byte: last 2 bytes of input image */
    *quality = 0;
    if (mod->input_size_bytes >= 2) {
        *quality = mod->input_data[mod->input_size_bytes - 2];
    }

    return 0;
}

/*===========================================================================
 * L5: First-Order IIR Digital Filter
 *
 * y[k] = alpha * x[k] + (1 - alpha) * y[k-1]
 * alpha = Ts / (tau + Ts)
 *
 * Equivalent to a continuous first-order lag with time constant tau.
 * The -3dB cutoff frequency is f_c = 1 / (2*pi*tau) Hz.
 *
 * Stability condition: 0 < alpha <= 1 (satisfied when tau >= 0)
 * DC gain: lim_{s->0} = 1.0 (no steady-state error)
 *
 * For tau = 0: alpha = 1, output follows input exactly (no filtering)
 * For tau >> Ts: alpha ≈ Ts/tau, heavy smoothing
 *
 * Reference: Oppenheim & Schafer, "Discrete-Time Signal Processing" (3rd ed), Ch.6
 *===========================================================================*/

int pointio_ai_filter(double raw_value, double *prev_filtered,
    double tau_ms, double ts_ms, double *filtered)
{
    if (!prev_filtered || !filtered) return -1;
    if (tau_ms < 0.0 || ts_ms <= 0.0) return -1;

    /* Compute filter coefficient alpha */
    double alpha;
    if (tau_ms < 1e-6) {
        alpha = 1.0;  /* No filtering */
    } else {
        alpha = ts_ms / (tau_ms + ts_ms);
    }

    /* Apply discrete-time first-order filter */
    *filtered = alpha * raw_value + (1.0 - alpha) * (*prev_filtered);
    *prev_filtered = *filtered;

    return 0;
}

/*===========================================================================
 * L5: Analog Output Operations
 *===========================================================================*/

/**
 * Write engineering units value to an analog output channel.
 *
 * Converts EU to raw counts, enforces output clamping limits,
 * and writes to the module's output data image.
 */
int pointio_ao_write_eu(pointio_module_t *mod, uint8_t channel,
    const pointio_ao_config_t *config, double value_eu)
{
    if (!mod || !config) return -1;
    if (mod->type != POINTIO_TYPE_ANALOG_OUTPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    /* Apply output clamping */
    if (value_eu > config->clamp_high_eu) value_eu = config->clamp_high_eu;
    if (value_eu < config->clamp_low_eu)  value_eu = config->clamp_low_eu;

    /* Convert to raw count */
    uint16_t raw_count;
    if (pointio_ai_eu_to_raw(value_eu, &config->scaling, &raw_count) != 0) {
        return -1;
    }

    /* Write to output image (little-endian INT) */
    uint8_t byte_offset = channel * 2;
    if (byte_offset + 1 >= POINTIO_MAX_DATA_SIZE) return -1;

    mod->output_data[byte_offset]     = (uint8_t)(raw_count & 0xFF);
    mod->output_data[byte_offset + 1] = (uint8_t)((raw_count >> 8) & 0xFF);

    return 0;
}

/**
 * Write raw count directly to an analog output.
 */
int pointio_ao_write_raw(pointio_module_t *mod, uint8_t channel,
    int16_t raw_count)
{
    if (!mod) return -1;
    if (mod->type != POINTIO_TYPE_ANALOG_OUTPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    uint8_t byte_offset = channel * 2;
    if (byte_offset + 1 >= POINTIO_MAX_DATA_SIZE) return -1;

    mod->output_data[byte_offset]     = (uint8_t)(raw_count & 0xFF);
    mod->output_data[byte_offset + 1] = (uint8_t)((raw_count >> 8) & 0xFF);

    return 0;
}

/**
 * Ramp an analog output to a target at a controlled rate.
 *
 * Prevents process shocks by smoothly transitioning the output.
 * The ramp follows a linear trajectory from the current output
 * toward the target at ramp_rate EU/s.
 *
 * Algorithm:
 *   1. Read current output
 *   2. Compute step = sign(target - current) * min(|target - current|, ramp_rate * dt)
 *   3. Write current + step
 *
 * Returns 1 when target is reached (within tolerance).
 */
int pointio_ao_ramp_to(pointio_module_t *mod, uint8_t channel,
    const pointio_ao_config_t *config, double target_eu,
    double ramp_rate, double dt_s, double *current_eu)
{
    if (!mod || !config || !current_eu) return -1;
    if (mod->type != POINTIO_TYPE_ANALOG_OUTPUT) return -1;
    if (channel >= mod->num_channels) return -1;
    if (ramp_rate <= 0.0 || dt_s <= 0.0) {
        /* No ramping: direct write */
        pointio_ao_write_eu(mod, channel, config, target_eu);
        *current_eu = target_eu;
        return 1;
    }

    /* Read current output from the data image */
    uint8_t byte_offset = channel * 2;
    int16_t raw = (int16_t)(mod->output_data[byte_offset] |
                           (mod->output_data[byte_offset + 1] << 8));
    uint16_t raw_unsigned = (uint16_t)raw;
    double current;
    pointio_ai_raw_to_eu(raw_unsigned, &config->scaling, &current);

    /* Check if target reached */
    const double tolerance = 0.01;
    if (fabs(target_eu - current) < tolerance) {
        *current_eu = current;
        return 1;  /* At target */
    }

    /* Compute ramp step */
    double max_step = ramp_rate * dt_s;
    double delta = target_eu - current;
    double step;
    if (fabs(delta) <= max_step) {
        step = delta;
    } else {
        step = (delta > 0) ? max_step : -max_step;
    }

    double new_value = current + step;
    pointio_ao_write_eu(mod, channel, config, new_value);
    *current_eu = new_value;

    return 0;  /* Still ramping */
}

/*===========================================================================
 * L6: Canonical Problem - 4-20mA Loop Integrity Check
 *
 * The 4-20mA current loop is the dominant analog signaling standard
 * in process industries. Loop integrity checking is essential for:
 *   - Detecting open circuits (broken wire, loose terminal)
 *   - Detecting short circuits (sensor short, wiring fault)
 *   - Identifying sensor over-range or under-range conditions
 *
 * Reference: ISA TR20.00.01 Section 6 "Loop Diagnostics"
 *===========================================================================*/

/**
 * Check 4-20mA loop integrity.
 *
 * Mapping (for 1734-IE4C with 16384 counts full scale):
 *   0-1mA:    0-820 counts   → Open wire
 *   2-3.9mA:  1638-3195      → Under-range
 *   4-20mA:   3277-16383      → Normal
 *   20-21mA:  16383-17202     → Over-range
 *   >21mA:    >17202          → Short circuit / sensor fault
 */
int pointio_ai_check_loop_integrity(uint16_t raw_count,
    const pointio_analog_scaling_t *scaling,
    int *status, double *loop_ma)
{
    if (!scaling || !status || !loop_ma) return -1;
    if (scaling->range != POINTIO_AI_RANGE_4_20MA) return -1;

    /* Convert raw count to mA (linear mapping: 0-16383 → 0-21mA) */
    double raw_full_scale = 16383.0;
    double ma_full_scale = 21.0;  /* Module can measure up to 21mA */
    *loop_ma = ((double)raw_count / raw_full_scale) * ma_full_scale;

    /* Classify loop condition */
    if (*loop_ma < 1.0) {
        *status = 1;  /* Open wire */
    } else if (*loop_ma < 3.5) {
        *status = 3;  /* Under-range */
    } else if (*loop_ma <= 20.5) {
        *status = 0;  /* OK */
    } else if (*loop_ma <= 21.5) {
        *status = 4;  /* Over-range */
    } else {
        *status = 2;  /* Short circuit / saturation */
    }

    return 0;
}

/**
 * Detect open wire on 4-20mA input.
 */
int pointio_ai_detect_open_wire(uint16_t raw_count,
    const pointio_analog_scaling_t *scaling, int *is_open)
{
    if (!scaling || !is_open) return -1;

    *is_open = 0;

    if (scaling->range != POINTIO_AI_RANGE_4_20MA) {
        return -1;  /* Open wire detection only meaningful for 4-20mA */
    }

    /* In 4-20mA mode, raw count near 0 indicates < 1mA → open wire */
    if (raw_count < (uint16_t)(scaling->raw_low * 0.25)) {
        *is_open = 1;
    }

    return 0;
}

/*===========================================================================
 * L5: Thermocouple Cold Junction Compensation
 *
 * TC voltage V_measured = V(T_hot, T_cjc) where T_cjc is cold junction temp.
 * The relationship V(T_hot, T_cjc) = V(T_hot, 0) - V(T_cjc, 0).
 *
 * Therefore: V(T_hot, 0) = V_measured + V(T_cjc, 0)
 *
 * Then T_hot is found by evaluating the inverse polynomial.
 *
 * For simplicity, we use a linear approximation:
 *   V(T_cjc, 0) ≈ Seebeck_coefficient * T_cjc at low temps
 *
 * Type J: ~50.2 uV/degC at 0-50 degC CJC range
 * Type K: ~39.5 uV/degC at 0-50 degC CJC range
 * Type T: ~38.7 uV/degC at 0-50 degC CJC range
 * Type E: ~58.7 uV/degC at 0-50 degC CJC range
 *
 * Reference: ASTM E230 Table 1
 *===========================================================================*/

int pointio_ai_cold_junction_compensation(double tc_voltage_mv,
    double cjc_temp_c, int tc_type, double *temp_c)
{
    if (!temp_c) return -1;

    /* Seebeck coefficients at ~25 degC (uV/degC → mV/degC) */
    double seebeck_mv_per_c;
    switch (tc_type) {
    case 0: seebeck_mv_per_c = 0.0502; break;  /* Type J */
    case 1: seebeck_mv_per_c = 0.0395; break;  /* Type K */
    case 2: seebeck_mv_per_c = 0.0387; break;  /* Type T */
    case 3: seebeck_mv_per_c = 0.0587; break;  /* Type E */
    default: return -1;
    }

    /* Compensated voltage: V(T_hot, 0) = V_meas + V(T_cjc, 0) */
    double cjc_voltage_mv = seebeck_mv_per_c * cjc_temp_c;
    double compensated_mv = tc_voltage_mv + cjc_voltage_mv;

    /* Convert compensated voltage to temperature using polynomial */
    const double *coeffs;
    int order;

    if (compensated_mv >= 0) {
        switch (tc_type) {
        case 0: /* Type J positive */
            coeffs = tc_j_coeffs;
            order = 8;
            break;
        case 1: /* Type K positive */
            coeffs = tc_k_coeffs_pos;
            order = 9;
            break;
        case 2: /* Type T positive */
            coeffs = tc_t_coeffs_pos;
            order = 6;
            break;
        case 3: /* Type E positive */
            coeffs = tc_e_coeffs_pos;
            order = 9;
            break;
        default:
            return -1;
        }
    } else {
        switch (tc_type) {
        case 0: /* Type J - use same coeffs for full range */
            coeffs = tc_j_coeffs;
            order = 8;
            break;
        case 1: /* Type K negative */
            coeffs = tc_k_coeffs_neg;
            order = 7;
            break;
        case 2: /* Type T negative */
            coeffs = tc_t_coeffs_neg;
            order = 7;
            break;
        case 3: /* Type E negative */
            coeffs = tc_e_coeffs_neg;
            order = 7;
            break;
        default:
            return -1;
        }
    }

    *temp_c = poly_eval(coeffs, order, compensated_mv);
    return 0;
}

/*===========================================================================
 * L5: Process Alarm Evaluation (ISA-18.2)
 *
 * Alarm management per ISA-18.2 / IEC 62682.
 * Four alarm levels: High-High (HH), High (H), Low (L), Low-Low (LL).
 *
 * Hysteresis (deadband) prevents alarm chattering:
 *   - Alarm triggers when value > threshold
 *   - Alarm clears when value < threshold - deadband
 *   - Symmetric logic for low alarms
 *
 * This uses a simple state machine per channel:
 *   prev_alarm bits: bit0=H active, bit1=HH active, bit2=L active, bit3=LL active
 *===========================================================================*/

int pointio_ai_evaluate_alarms(double value_eu,
    const pointio_ai_config_t *config, int *prev_alarm, int *alarm)
{
    if (!config || !prev_alarm || !alarm) return -1;

    double db = config->alarm_deadband;
    if (db < 0.0) db = 0.0;

    /* Reset alarm output */
    *alarm = 0;  /* 0 = normal */

    /* High-High alarm */
    if (config->enable_alarm) {
        if (value_eu >= config->alarm_high_high) {
            *alarm = 2;  /* HH */
            *prev_alarm = (*prev_alarm & ~0x03) | 0x02;  /* Set HH bit */
        } else if (value_eu >= config->alarm_high) {
            if ((*prev_alarm & 0x02) && value_eu >= config->alarm_high_high - db) {
                /* Still in HH hysteresis zone */
                *alarm = 2;
            } else if (value_eu < config->alarm_high_high - db) {
                /* Exited HH hysteresis, now in H */
                *alarm = 1;
                *prev_alarm = (*prev_alarm & ~0x03) | 0x01;
            } else {
                *alarm = 1;
                *prev_alarm = (*prev_alarm & ~0x03) | 0x01;
            }
        } else if (value_eu <= config->alarm_low_low) {
            *alarm = 4;  /* LL */
            *prev_alarm = (*prev_alarm & ~0x0C) | 0x08;
        } else if (value_eu <= config->alarm_low) {
            if ((*prev_alarm & 0x08) && value_eu <= config->alarm_low_low + db) {
                *alarm = 4;  /* Still in LL hysteresis */
            } else if (value_eu > config->alarm_low_low + db) {
                *alarm = 3;  /* L */
                *prev_alarm = (*prev_alarm & ~0x0C) | 0x04;
            } else {
                *alarm = 3;
                *prev_alarm = (*prev_alarm & ~0x0C) | 0x04;
            }
        } else {
            /* Check hysteresis for clearing H */
            if ((*prev_alarm & 0x01) && value_eu >= config->alarm_high - db) {
                *alarm = 1;  /* Still in H hysteresis */
            } else if ((*prev_alarm & 0x02) && value_eu >= config->alarm_high - db) {
                *alarm = 1;  /* HH cleared, now in H */
                *prev_alarm = (*prev_alarm & ~0x03) | 0x01;
            } else {
                /* All alarms clear */
                *prev_alarm = 0;
            }

            /* Check hysteresis for clearing L */
            if ((*prev_alarm & 0x04) && value_eu <= config->alarm_low + db) {
                *alarm = 3;  /* Still in L hysteresis */
            } else if ((*prev_alarm & 0x08) && value_eu <= config->alarm_low + db) {
                *alarm = 3;  /* LL cleared, now in L */
                *prev_alarm = (*prev_alarm & ~0x0C) | 0x04;
            } else {
                /* Already checked H clear above, this is fine */
            }
        }
    }

    return 0;
}
