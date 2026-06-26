/**
 * @file    logix_pide_instruction.c
 * @brief   ControlLogix Enhanced PID (PIDE) Implementation
 * L5-L6: PID Algorithm with velocity form, anti-windup, bumpless transfer
 *
 * Reference: 1756-RM006; Astrom & Hagglund (1995) Ch. 3, 5
 */

#include <string.h>
#include <math.h>
#include "logix_pide_instruction.h"

void logix_pide_init(logix_pide_t *pide, double ts_sec)
{
    if (!pide) return;

    memset(pide, 0, sizeof(*pide));

    /* Default tuning: proportional only, Kp=1.0 */
    pide->kp = 1.0;
    pide->ki = 0.0;
    pide->kd = 0.0;
    pide->ts_sec = (ts_sec > 0.0) ? ts_sec : 0.1;

    /* Default mode: Manual (safe startup) */
    pide->mode = PIDE_MODE_MANUAL;
    pide->form = PIDE_FORM_VELOCITY;
    pide->gain_form = PIDE_GAIN_INDEPENDENT;

    /* Output limits: 0-100% */
    pide->cv_min = 0.0;
    pide->cv_max = 100.0;
    pide->cv_roc_limit = 0.0;  /* No rate limiting */
    pide->cv = 0.0;
    pide->cv_prev = 0.0;

    /* Anti-windup: clamping by default */
    pide->antiwindup_method = PIDE_ANTIWINDUP_CLAMPING;
    pide->backcalc_gain = 1.0;

    /* Setpoint defaults */
    pide->sp = 0.0;
    pide->sp_ratio = 1.0;
    pide->sp_bias = 0.0;
    pide->pv_eu_min = 0.0;
    pide->pv_eu_max = 100.0;

    /* Feedforward disabled */
    pide->ffwd = 0.0;
    pide->ffwd_ratio = 0.0;
    pide->ffwd_bias = 0.0;

    /* PV filter disabled */
    pide->pv_filter_enabled = false;
    pide->pv_filter_tau_sec = 0.0;
    pide->pv_filtered = 0.0;

    /* Internal state */
    pide->error_prev = 0.0;
    pide->pv_prev = 0.0;
    pide->pv_prev2 = 0.0;
    pide->integral_sum = 0.0;
    pide->initialized = false;

    pide->status_word = 0;
}

double logix_pide_execute(logix_pide_t *pide)
{
    if (!pide) return 0.0;

    double ts = pide->ts_sec;
    if (ts <= 0.0) ts = 0.1;

    /* --- Step 1: Apply PV filtering (first-order lag) --- */
    double pv_raw = pide->pv;
    double pv_used;
    if (pide->pv_filter_enabled && pide->pv_filter_tau_sec > 0.0) {
        /* Backward Euler: alpha = Ts / (Ts + tau) */
        double alpha = ts / (ts + pide->pv_filter_tau_sec);
        pide->pv_filtered = alpha * pv_raw + (1.0 - alpha) * pide->pv_filtered;
        pv_used = pide->pv_filtered;
    } else {
        pv_used = pv_raw;
    }

    /* --- Step 2: Compute effective setpoint --- */
    double sp_eff = pide->sp * pide->sp_ratio + pide->sp_bias;

    /* Cascade override: if in Cascade mode, use cascade SP */
    if (pide->mode == PIDE_MODE_CASCADE) {
        sp_eff = pide->cascade_sp;
    }

    /* --- Step 3: PV tracking in Manual --- */
    if (pide->mode == PIDE_MODE_MANUAL || pide->mode == PIDE_MODE_HAND) {
        /* SP tracks PV for bumpless transfer to Auto */
        pide->sp = pv_used;
        sp_eff = pv_used;
    }

    /* --- Step 4: Calculate error --- */
    double error = sp_eff - pv_used;
    pide->error = error;

    /* --- Step 5: Compute PID terms --- */
    double delta_cv = 0.0;
    double p_term = 0.0, i_term = 0.0, d_term = 0.0;

    if (pide->form == PIDE_FORM_VELOCITY) {
        /* Velocity (incremental) form:
         * Delta_CV = Kp*(E(n)-E(n-1)) + Ki*Ts*E(n) + Kd/Ts*(2*PV(n-1)-PV(n)-PV(n-2))
         * Note: derivative on PV to avoid derivative kick */

        /* Proportional on error change */
        if (pide->initialized) {
            p_term = pide->kp * (error - pide->error_prev);
        } else {
            p_term = 0.0;
        }

        /* Integral */
        i_term = pide->ki * ts * error;

        /* Derivative on PV (not error), with protection against div-by-zero */
        if (ts > 1e-12 && pide->initialized) {
            d_term = (pide->kd / ts) * (2.0 * pide->pv_prev - pv_used - pide->pv_prev2);
        } else {
            d_term = 0.0;
        }

        delta_cv = p_term + i_term + d_term;

        /* Feedforward term */
        double ffwd_eff = pide->ffwd * pide->ffwd_ratio + pide->ffwd_bias;
        pide->ffwd_term = ffwd_eff;
        delta_cv += ffwd_eff;

        /* Accumulate integral term */
        pide->integral_sum += i_term;

    } else {
        /* Positional form:
         * CV = Kp*E + Ki*Ts*SUM(E) + Kd/Ts*(PV(n-1)-PV(n)) + FFWD */

        p_term = pide->kp * error;

        /* Integral with anti-windup clamping */
        i_term = pide->ki * ts * error;
        double proposed_integral = pide->integral_sum + i_term;

        /* Clamp integral sum if output would saturate */
        double cv_no_integral = p_term;
        double cv_max_integral = pide->cv_max - cv_no_integral;
        double cv_min_integral = pide->cv_min - cv_no_integral;

        if (proposed_integral > cv_max_integral) {
            i_term = cv_max_integral - pide->integral_sum;
            if (i_term < 0.0) i_term = 0.0;
            pide->windup_occurred = true;
        } else if (proposed_integral < cv_min_integral) {
            i_term = cv_min_integral - pide->integral_sum;
            if (i_term > 0.0) i_term = 0.0;
            pide->windup_occurred = true;
        }

        pide->integral_sum += i_term;

        /* Derivative on PV */
        if (ts > 1e-12 && pide->initialized) {
            d_term = (pide->kd / ts) * (pide->pv_prev - pv_used);
        } else {
            d_term = 0.0;
        }

        double ffwd_eff = pide->ffwd * pide->ffwd_ratio + pide->ffwd_bias;
        pide->ffwd_term = ffwd_eff;
        delta_cv = p_term + pide->integral_sum + d_term + ffwd_eff - pide->cv_prev;
    }

    /* Step 6: Apply output limits */
    double cv_new = pide->cv_prev + delta_cv;

    /* Output clamping */
    if (cv_new > pide->cv_max) {
        cv_new = pide->cv_max;
        pide->cv_high_limit_active = true;
    } else if (cv_new < pide->cv_min) {
        cv_new = pide->cv_min;
        pide->cv_low_limit_active = true;
    } else {
        pide->cv_high_limit_active = false;
        pide->cv_low_limit_active = false;
    }

    /* Rate-of-change limiting */
    if (pide->cv_roc_limit > 0.0) {
        double max_step = pide->cv_roc_limit * ts;
        double step = cv_new - pide->cv_prev;
        if (step > max_step) {
            cv_new = pide->cv_prev + max_step;
            pide->cv_roc_limit_active = true;
        } else if (step < -max_step) {
            cv_new = pide->cv_prev - max_step;
            pide->cv_roc_limit_active = true;
        } else {
            pide->cv_roc_limit_active = false;
        }
    }

    /* Step 7: Update internal state */
    pide->error_prev = error;
    pide->pv_prev2 = pide->pv_prev;
    pide->pv_prev = pv_used;
    pide->cv = cv_new;
    pide->cv_prev = cv_new;
    pide->p_term = p_term;
    pide->i_term = i_term;
    pide->d_term = d_term;
    pide->initialized = true;

    /* Step 8: Update status word */
    pide->status_word = 0;
    if (pide->initialized) pide->status_word |= PIDE_STATUS_INITIALIZED;
    if (pide->mode == PIDE_MODE_CASCADE)  pide->status_word |= PIDE_STATUS_CASCADE_MODE;
    if (pide->mode == PIDE_MODE_MANUAL)   pide->status_word |= PIDE_STATUS_MANUAL_MODE;
    if (pide->cv_high_limit_active)      pide->status_word |= PIDE_STATUS_CV_HI_LIMIT;
    if (pide->cv_low_limit_active)       pide->status_word |= PIDE_STATUS_CV_LO_LIMIT;
    if (pide->cv_roc_limit_active)       pide->status_word |= PIDE_STATUS_ROC_LIMIT;
    if (pide->windup_occurred)           pide->status_word |= PIDE_STATUS_WINDUP;

    return cv_new;
}

void logix_pide_set_mode(logix_pide_t *pide, pide_mode_t new_mode)
{
    if (!pide) return;

    pide_mode_t old_mode = pide->mode;

    /* Bumpless transfer: Manual/Auto transition */
    if (old_mode == PIDE_MODE_MANUAL && new_mode == PIDE_MODE_AUTO) {
        /* Pre-load integral term to current CV to avoid bump */
        /* integral_sum = CV - Kp*E - D_term (approximate) */
        pide->integral_sum = pide->cv - pide->kp * pide->error;
    }

    if (new_mode == PIDE_MODE_MANUAL || new_mode == PIDE_MODE_HAND) {
        /* SP tracks PV for bumpless return to Auto */
        pide->sp = pide->pv;
    }

    pide->mode = new_mode;
}

void logix_pide_convert_gains(logix_pide_t *pide)
{
    if (!pide) return;

    if (pide->gain_form == PIDE_GAIN_INDEPENDENT) {
        /* Convert independent -> dependent (Kc, Ti, Td) */
        /* Kc = Kp, Ti = Kc/Ki, Td = Kd/Kc
         * For this implementation, we store the converted values
         * back into the kp/ki/kd fields. */
        double kc = pide->kp;

        /* Integral time = Kc / Ki */
        /* We store the reciprocal; real implementation would use separate fields */
        if (fabs(pide->ki) > 1e-12) {
            pide->ki = kc / pide->ki;  /* Now holds Ti (sec) */
        }

        /* Derivative time = Kd / Kc */
        pide->kd = pide->kd / ((fabs(kc) > 1e-12) ? kc : 1.0);  /* Now holds Td (sec) */

        pide->gain_form = PIDE_GAIN_DEPENDENT;
    } else {
        /* Convert dependent -> independent (Kp, Ki, Kd) */
        double kc = pide->kp;

        /* Ki = Kc / Ti */
        if (fabs(pide->ki) > 1e-12) {
            pide->ki = kc / pide->ki;  /* Ki in 1/sec */
        }

        /* Kd = Kc * Td */
        pide->kd = kc * pide->kd;  /* Kd in sec */

        pide->gain_form = PIDE_GAIN_INDEPENDENT;
    }
}

void logix_pide_set_pv_filter(logix_pide_t *pide, double tau_sec)
{
    if (!pide) return;

    if (tau_sec <= 0.0) {
        pide->pv_filter_enabled = false;
        pide->pv_filter_tau_sec = 0.0;
    } else {
        pide->pv_filter_enabled = true;
        pide->pv_filter_tau_sec = tau_sec;
    }
}

void logix_pide_set_feedforward(logix_pide_t *pide,
                                  double ratio, double bias)
{
    if (!pide) return;
    pide->ffwd_ratio = ratio;
    pide->ffwd_bias = bias;
}

void logix_pide_set_roc_limit(logix_pide_t *pide, double roc_limit_per_sec)
{
    if (!pide) return;
    pide->cv_roc_limit = (roc_limit_per_sec > 0.0) ? roc_limit_per_sec : 0.0;
}

uint16_t logix_pide_get_status(const logix_pide_t *pide)
{
    if (!pide) return 0;
    return pide->status_word;
}

bool logix_pide_is_tracking(const logix_pide_t *pide)
{
    if (!pide) return false;
    return (pide->mode == PIDE_MODE_MANUAL ||
            pide->mode == PIDE_MODE_HAND);
}

double logix_pide_closed_loop_bandwidth(const logix_pide_t *pide,
                                          double process_gain,
                                          double process_tau)
{
    if (!pide) return 0.0;

    /* For PI control of first-order process:
     *   G(s) = K / (tau*s + 1)
     *   C(s) = Kp + Ki/s
     *
     * Open loop: L(s) = C(s) * G(s) = (Kp*s + Ki) * K / (s*(tau*s + 1))
     *
     * Closed-loop bandwidth approximation (crossover frequency):
     *   omega_c ≈ K * Kp / tau   (simplified, for Ki << Kp)
     *
     * Reference: Skogestad (2003) "Simple analytic rules for PID tuning",
     *   J. Process Control, vol. 13, pp. 291-309
     */
    if (process_tau < 1e-12) process_tau = 1e-12;
    double omega_bw = fabs(process_gain * pide->kp) / process_tau;

    /* Floor: don't claim bandwidth below 0.001 rad/s */
    if (omega_bw < 0.001) omega_bw = 0.001;

    return omega_bw;
}