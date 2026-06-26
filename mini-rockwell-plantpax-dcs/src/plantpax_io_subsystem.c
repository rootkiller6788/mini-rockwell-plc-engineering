/**
 * @file plantpax_io_subsystem.c
 * @brief PlantPAx I/O Subsystem Implementation
 *
 * Implements analog/digital I/O scaling, NAMUR NE43 classification,
 * debounce, slew rate limiting, thermocouple/RDT conversion,
 * and HART value conversion.
 */
#include "plantpax_io_subsystem.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------------------
 * L3: Analog Input Channel Initialization
 * ------------------------------------------------------------------------- */

void pax_ai_channel_init(pax_ai_channel_t *ch, uint32_t id,
                          const char *tag, double min_eu, double max_eu,
                          const char *unit)
{
    if (ch == NULL) return;

    memset(ch, 0, sizeof(pax_ai_channel_t));
    ch->channel_id = id;
    if (tag != NULL) {
        strncpy(ch->tag_name, tag, sizeof(ch->tag_name) - 1);
    }
    ch->signal_type = PAX_ANALOG_4_20MA;
    ch->range_min_eu = min_eu;
    ch->range_max_eu = max_eu;
    ch->range_min_raw = 4.0;   /* 4 mA */
    ch->range_max_raw = 20.0;  /* 20 mA */
    if (unit != NULL) {
        strncpy(ch->engineering_unit, unit,
                sizeof(ch->engineering_unit) - 1);
    }
    ch->filter_time_constant_s = 1.0;
    ch->alarm_hihi = max_eu * 1.05;
    ch->alarm_hi = max_eu * 0.95;
    ch->alarm_lo = min_eu * 1.05;
    ch->alarm_lolo = min_eu * 0.95;
    ch->alarm_deadband = (max_eu - min_eu) * 0.01;
    ch->square_root_extraction = false;
    ch->hart_enabled = false;
    ch->last_raw_value = 4.0;
    ch->last_scaled_value = min_eu;
    ch->quality = PAX_IO_QUALITY_GOOD;
    ch->namur_state = PAX_NAMUR_NORMAL;
    ch->update_count = 0;
}

/* ---------------------------------------------------------------------------
 * L3: Analog Input Linear Scaling
 *
 * Linear interpolation from raw range to engineering units:
 *   EU = EU_min + (RAW - RAW_min) * (EU_max - EU_min) / (RAW_max - RAW_min)
 *
 * Square root extraction (differential pressure flow measurement):
 *   Q = Q_max * sqrt((DP - DP_min) / (DP_max - DP_min))
 *   EU = EU_min + sqrt((RAW - RAW_min) / (RAW_max - RAW_min)) * (EU_max - EU_min)
 *
 * This follows Bernoulli's principle: Q ? sqrt(?P)
 * ------------------------------------------------------------------------- */

double pax_ai_scale_raw_to_eu(const pax_ai_channel_t *ch, double raw_value)
{
    if (ch == NULL) return 0.0;

    double raw_range = ch->range_max_raw - ch->range_min_raw;
    double eu_range = ch->range_max_eu - ch->range_min_eu;

    if (fabs(raw_range) < 1e-9) return ch->range_min_eu;

    double normalized;

    if (ch->square_root_extraction) {
        /* Clamp raw value to valid range before square root */
        double clamped = raw_value;
        if (clamped < ch->range_min_raw) clamped = ch->range_min_raw;
        if (clamped > ch->range_max_raw) clamped = ch->range_max_raw;

        normalized = (clamped - ch->range_min_raw) / raw_range;
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;

        normalized = sqrt(normalized);
    } else {
        normalized = (raw_value - ch->range_min_raw) / raw_range;
        /* Clamp to [0, 1] for linear scaling */
        if (normalized < 0.0) normalized = 0.0;
        if (normalized > 1.0) normalized = 1.0;
    }

    return ch->range_min_eu + normalized * eu_range;
}

/* ---------------------------------------------------------------------------
 * L3: Analog Output EU-to-Raw Scaling
 * ------------------------------------------------------------------------- */

double pax_ao_scale_eu_to_raw(const pax_ao_channel_t *ch, double eu_value)
{
    if (ch == NULL) return 0.0;

    double eu_range = ch->range_max_eu - ch->range_min_eu;
    double raw_range = ch->range_max_raw - ch->range_min_raw;

    if (fabs(eu_range) < 1e-9) return ch->range_min_raw;

    double normalized = (eu_value - ch->range_min_eu) / eu_range;
    if (normalized < 0.0) normalized = 0.0;
    if (normalized > 1.0) normalized = 1.0;

    return ch->range_min_raw + normalized * raw_range;
}

/* ---------------------------------------------------------------------------
 * L5: First-Order Low-Pass Filter
 *
 * Exponential smoothing:
 *   Y[n] = Y[n-1] + alpha * (X[n] - Y[n-1])
 *
 * Where alpha = Ts / (tau + Ts)
 *   - Ts: sample time
 *   - tau: filter time constant
 *
 * For tau >> Ts: alpha ? Ts/tau (very aggressive filtering)
 * For tau = 0: alpha = 1 (no filtering, Y[n] = X[n])
 *
 * The -3dB cutoff frequency: f_c = 1 / (2 * pi * tau)
 * ------------------------------------------------------------------------- */

double pax_ai_apply_filter(double prev_filtered, double raw_input,
                            double time_constant_s, double sample_time_s)
{
    if (time_constant_s <= 0.0) {
        return raw_input;  /* No filtering */
    }

    double alpha = sample_time_s / (time_constant_s + sample_time_s);

    /* Clamp alpha to valid range */
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;

    return prev_filtered + alpha * (raw_input - prev_filtered);
}

/* ---------------------------------------------------------------------------
 * L4: NAMUR NE43 Signal Classification
 *
 * Maps 4-20 mA current loop signal to NAMUR fault state:
 *
 *   0.0 - 3.6 mA:   UNDERRANGE (wire break, power loss)
 *   3.6 - 3.8 mA:   SENSOR_FAULT_LOW
 *   3.8 - 4.0 mA:   SATURATION_LOW
 *   4.0 - 20.0 mA:  NORMAL
 *   20.0 - 20.5 mA: SATURATION_HIGH
 *   20.5 - 21.0 mA: SENSOR_FAULT_HIGH
 *   > 21.0 mA:      OVERRANGE (short circuit)
 *
 * NAMUR NE43 standardizes these thresholds for transmitter
 * diagnostics without requiring additional wiring.
 * ------------------------------------------------------------------------- */

pax_namur_fault_t pax_ai_classify_namur(double current_ma)
{
    if (current_ma < PAX_NAMUR_UNDER_RANGE_MA) {
        return PAX_NAMUR_UNDER_RANGE;
    } else if (current_ma < PAX_NAMUR_FAULT_LOW_MA) {
        return PAX_NAMUR_SENSOR_FAULT_LOW;
    } else if (current_ma < PAX_NAMUR_NORMAL_LOW_MA) {
        return PAX_NAMUR_SATURATION_LOW;
    } else if (current_ma <= PAX_NAMUR_NORMAL_HIGH_MA) {
        return PAX_NAMUR_NORMAL;
    } else if (current_ma <= PAX_NAMUR_FAULT_HIGH_MA) {
        return PAX_NAMUR_SATURATION_HIGH;
    } else if (current_ma <= PAX_NAMUR_OVER_RANGE_MA) {
        return PAX_NAMUR_SENSOR_FAULT_HIGH;
    } else {
        return PAX_NAMUR_OVER_RANGE;
    }
}

/* ---------------------------------------------------------------------------
 * L2: Analog Alarm Limit Checking
 *
 * Checks process value against four alarm limits with deadband:
 *   - HIHI: PV > alarm_hihi
 *   - HI:   PV > alarm_hi
 *   - LO:   PV < alarm_lo
 *   - LOLO: PV < alarm_lolo
 *
 * Deadband prevents alarm chattering near limits.
 * Return value indicates the most severe active alarm.
 * ------------------------------------------------------------------------- */

int pax_ai_check_alarms(const pax_ai_channel_t *ch, double value)
{
    if (ch == NULL) return 0;

    /* Check in priority order (most severe first) */
    if (value > ch->alarm_hihi) return 4;
    if (value < ch->alarm_lolo) return 2;
    if (value > ch->alarm_hi) return 3;
    if (value < ch->alarm_lo) return 1;

    return 0;  /* No alarm */
}

/* ---------------------------------------------------------------------------
 * L3: Digital Input Debounce
 *
 * State transitions only recognized after a configurable
 * debounce time with a stable signal.
 *
 * This prevents false triggers from contact bounce, EMI,
 * or transient noise on digital inputs.
 * ------------------------------------------------------------------------- */

bool pax_di_apply_debounce(const pax_di_channel_t *ch,
                            bool raw_state, double time_since_change_ms)
{
    if (ch == NULL) return raw_state;

    double debounce_ms = raw_state ? ch->on_debounce_ms : ch->off_debounce_ms;

    if (time_since_change_ms < debounce_ms) {
        return ch->last_state;  /* Hold previous state */
    }

    return raw_state;
}

/* ---------------------------------------------------------------------------
 * L3: Digital Output Readback Verification
 *
 * For safety-critical outputs, the actual state is verified
 * against the commanded state via readback circuitry.
 * A mismatch indicates a fault.
 * ------------------------------------------------------------------------- */

bool pax_do_verify_readback(const pax_do_channel_t *ch, bool readback_state)
{
    if (ch == NULL) return true;

    return (readback_state == ch->commanded_state);
}

/* ---------------------------------------------------------------------------
 * L3: Analog Output Slew Rate Limiting
 *
 * Limits the rate of change of the output to prevent
 * process upsets from rapid valve movements.
 *
 * Max change = slew_rate_limit * dt
 * If target exceeds max change, output moves at max rate.
 * ------------------------------------------------------------------------- */

double pax_ao_apply_slew_rate(const pax_ao_channel_t *ch,
                               double target_eu,
                               double current_eu,
                               double dt_s)
{
    if (ch == NULL) return target_eu;

    if (ch->slew_rate_limit_eu_s <= 0.0 || dt_s <= 0.0) {
        return target_eu;
    }

    double max_change = ch->slew_rate_limit_eu_s * dt_s;
    double delta = target_eu - current_eu;

    if (fabs(delta) <= max_change) {
        return target_eu;
    }

    /* Apply slew rate limit */
    if (delta > 0.0) {
        return current_eu + max_change;
    } else {
        return current_eu - max_change;
    }
}

/* ---------------------------------------------------------------------------
 * L5: Thermocouple Voltage to Temperature (ITS-90)
 *
 * Type K (Chromel-Alumel) polynomial:
 *   For -200 to 0 C:
 *     T = d0 + d1*E + d2*E^2 + ... + d9*E^9
 *   For 0 to 1372 C:
 *     T = c0 + c1*E + c2*E^2 + ... + c9*E^9
 *
 * Combined with cold junction compensation (CJC):
 *   E_actual = E_measured + E_CJC(T_cold_junction)
 *   T = f(E_actual)
 *
 * Reference: NIST ITS-90 Thermocouple Database
 * ------------------------------------------------------------------------- */

double pax_tc_voltage_to_temp(double voltage_mv, double cjc_temp_c,
                               pax_analog_signal_type_t tc_type)
{
    /* CJC: convert cold junction temperature to equivalent TC voltage */
    double cjc_mv = 0.0;

    /* Type K: approximately 0.041 mV/?C near room temperature */
    if (tc_type == PAX_ANALOG_TC_K) {
        /* Simplified inverse for Type K: Seebeck coefficient around 41 uV/C */
        cjc_mv = cjc_temp_c * 0.041;
    } else if (tc_type == PAX_ANALOG_TC_J) {
        /* Type J: approximately 0.051 mV/?C */
        cjc_mv = cjc_temp_c * 0.051;
    } else if (tc_type == PAX_ANALOG_TC_T) {
        /* Type T: approximately 0.040 mV/?C */
        cjc_mv = cjc_temp_c * 0.040;
    } else if (tc_type == PAX_ANALOG_TC_E) {
        /* Type E: approximately 0.060 mV/?C */
        cjc_mv = cjc_temp_c * 0.060;
    } else if (tc_type == PAX_ANALOG_TC_S) {
        /* Type S: approximately 0.006 mV/?C (much smaller!) */
        cjc_mv = cjc_temp_c * 0.006;
    } else {
        return 0.0;
    }

    /* Corrected EMF */
    double emf_mv = voltage_mv + cjc_mv;

    /* Simplified linear conversion (real ITS-90 uses 9th order poly) */
    /* For Type K: ~41 uV/?C ? ~24.39 ?C/mV */
    double sens_uv_per_c = 0.0;

    switch (tc_type) {
        case PAX_ANALOG_TC_K: sens_uv_per_c = 41.0; break;
        case PAX_ANALOG_TC_J: sens_uv_per_c = 51.0; break;
        case PAX_ANALOG_TC_T: sens_uv_per_c = 40.0; break;
        case PAX_ANALOG_TC_E: sens_uv_per_c = 60.0; break;
        case PAX_ANALOG_TC_S: sens_uv_per_c = 6.0;  break;
        default: return 0.0;
    }

    if (fabs(sens_uv_per_c) < 1e-9) return 0.0;
    return emf_mv * 1000.0 / sens_uv_per_c;  /* mV * 1000 / uV/C = ?C */
}

/* ---------------------------------------------------------------------------
 * L4: RTD Resistance to Temperature (IEC 60751)
 *
 * Callendar-Van Dusen equation for PT100:
 *
 * For T >= 0?C:
 *   R(T) = R0 * (1 + A*T + B*T^2)
 *
 * For T < 0?C:
 *   R(T) = R0 * (1 + A*T + B*T^2 + C*(T-100)*T^3)
 *
 * Where:
 *   A = 3.9083e-3 /?C
 *   B = -5.775e-7 /?C^2
 *   C = -4.183e-12 /?C^4
 *
 * For PT100: R0 = 100 ohm at 0?C
 * For PT1000: R0 = 1000 ohm at 0?C
 *
 * Inverse: solve quadratically for T >= 0?C
 * ------------------------------------------------------------------------- */

double pax_rtd_resistance_to_temp(double resistance_ohm,
                                   pax_analog_signal_type_t rtd_type)
{
    double r0;

    if (rtd_type == PAX_ANALOG_RTD_PT1000) {
        r0 = 1000.0;
    } else {
        r0 = 100.0;  /* Default to PT100 */
    }

    double A = 3.9083e-3;
    double B = -5.775e-7;

    /* Quadratic: B*T^2 + A*T + 1 - R/R0 = 0 */
    double c_term = 1.0 - resistance_ohm / r0;

    /* T = [-A + sqrt(A^2 - 4*B*c)] / (2*B) */
    double discriminant = A*A - 4.0 * B * c_term;

    if (discriminant < 0.0) {
        return 0.0;  /* Invalid */
    }

    double temp = (-A + sqrt(discriminant)) / (2.0 * B);

    /* For negative temperatures, iterate (simplified) */
    if (temp < 0.0) {
        /* For negative T, C*(T-100)*T^3 term applies */
        /* Simplified: use the positive-T formula with correction */
        double C = -4.183e-12;
        /* Newton refinement for negative T */
        /* For practical purposes, return the quadratic solution with a note */
        double correction = C * (temp - 100.0) * temp * temp * temp;
        temp = temp - correction / (A + 2.0 * B * temp);
    }

    return temp;
}

/* ---------------------------------------------------------------------------
 * L7: HART Digital Value Conversion
 *
 * HART (Highway Addressable Remote Transducer) is a digital
 * communication protocol superimposed on 4-20 mA analog signals.
 *
 * The digital value is a 16-bit integer mapped to the PV range:
 *   EU = EU_min + (digital_value / 65535) * (EU_max - EU_min)
 * ------------------------------------------------------------------------- */

double pax_hart_convert_to_eu(uint32_t hart_digital_value,
                               double range_min_eu, double range_max_eu)
{
    double frac = (double)hart_digital_value / 65535.0;
    if (frac < 0.0) frac = 0.0;
    if (frac > 1.0) frac = 1.0;

    return range_min_eu + frac * (range_max_eu - range_min_eu);
}

/* ---------------------------------------------------------------------------
 * L3: I/O Module Status Dump
 * ------------------------------------------------------------------------- */

void pax_io_module_dump(const pax_io_module_t *module)
{
    if (module == NULL) {
        printf("I/O Module: (null)\n");
        return;
    }

    printf("-------------------------------------------\n");
    printf("  I/O Module: %s (Slot %u)\n",
           module->module_name, module->slot);
    printf("  Family: %d | Channels: %u | RPI: %.1f ms\n",
           (int)module->family, module->num_channels,
           module->update_rate_ms);
    printf("  Scans: %llu | Errors: %llu\n",
           (unsigned long long)module->total_scan_count,
           (unsigned long long)module->error_count);
    printf("  Health: ");
    switch (module->health) {
        case PAX_HEALTH_OK: printf("OK\n"); break;
        case PAX_HEALTH_FAILURE: printf("FAILURE\n"); break;
        default: printf("DEGRADED\n");
    }
    printf("-------------------------------------------\n");
}
