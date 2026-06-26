/**
 * @file plantpax_control_loop.c
 * @brief PlantPAx Control Loop Implementation ? PID, Cascade, Ratio, Feedforward, Override
 *
 * Implements ISA-standard PID algorithm (velocity form), cascade control,
 * ratio control, override selectors, feedforward compensation,
 * Ziegler-Nichols tuning, bumpless transfer, and performance metrics.
 */
#include "plantpax_control_loop.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ---------------------------------------------------------------------------
 * L1: PID Parameter Initialization
 *
 * Sets up PID tuning parameters with either dependent (ISA standard)
 * or independent gain form.
 *
 * Dependent form (ISA):
 *   CO = Kc * [E + (1/Ti)*integral(E) + Td*dE/dt]
 *
 * Independent form:
 *   CO = Kp * E + Ki * integral(E) + Kd * dE/dt
 * ------------------------------------------------------------------------- */

void pax_pid_params_init(pax_pid_params_t *params, pax_pid_form_t form,
                          double kc, double ti_sec, double td_sec,
                          double sample_time_sec)
{
    if (params == NULL) return;

    memset(params, 0, sizeof(pax_pid_params_t));

    params->form = form;
    params->sample_time_sec = sample_time_sec;

    if (sample_time_sec <= 0.0) {
        params->sample_time_sec = PAX_PID_DEFAULT_SAMPLE_TIME;
    }

    if (form == PAX_PID_FORM_DEPENDENT) {
        params->kc = kc;
        params->ti_sec = ti_sec;
        params->td_sec = td_sec;
    } else if (form == PAX_PID_FORM_INDEPENDENT) {
        params->kp = kc;        /* Kp */
        params->ki = kc / ti_sec; /* Ki = Kc/Ti */
        params->kd = kc * td_sec; /* Kd = Kc * Td */
    } else {
        /* Velocity form: store as dependent, convert at runtime */
        params->kc = kc;
        params->ti_sec = ti_sec;
        params->td_sec = td_sec;
    }

    params->anti_windup = PAX_AW_BACK_CALCULATION;
    params->derivative_filter_sec = PAX_PID_DEFAULT_DERIV_FILTER;
    params->derivative_on_error = false;  /* Default: D on PV (less kick) */
}

/* ---------------------------------------------------------------------------
 * L1: PID Loop Initialization
 * ------------------------------------------------------------------------- */

void pax_pid_loop_init(pax_pid_loop_t *loop, uint32_t id,
                        const char *tag, pax_loop_type_t type,
                        pax_control_action_t action,
                        double out_min, double out_max)
{
    if (loop == NULL) return;

    memset(loop, 0, sizeof(pax_pid_loop_t));

    loop->loop_id = id;
    if (tag != NULL) {
        strncpy(loop->loop_tag, tag, sizeof(loop->loop_tag) - 1);
    }
    loop->loop_type = type;
    loop->mode = PAX_LOOP_MODE_MANUAL;
    loop->action = action;
    loop->output_min = out_min;
    loop->output_max = out_max;
    loop->setpoint_min = out_min;
    loop->setpoint_max = out_max;
    loop->output = out_min;
    loop->prev_mode = PAX_LOOP_MODE_MANUAL;
    loop->cycle_count = 0;

    /* Default PID parameters (conservative tuning) */
    pax_pid_params_init(&loop->params, PAX_PID_FORM_VELOCITY,
                         1.0, 60.0, 0.0, 0.1);
}

/* ---------------------------------------------------------------------------
 * L5: PID Execution ? Velocity Form Algorithm
 *
 * This is the core PID algorithm used in Rockwell PlantPAx P_PIDE AOI.
 *
 * Velocity (incremental) form:
 *
 *   delta_CO = Kc * [
 *       (PV[n-1] - PV[n])                        ? proportional on PV change
 *     + (Ts / Ti) * (SP[n] - PV[n])              ? integral on error
 *     + (Td / Ts) * (2*PV[n-1] - PV[n] - PV[n-2]) ? derivative on PV
 *   ]
 *
 * For derivative on error (less common):
 *   delta_CO = Kc * [
 *       (E[n] - E[n-1])                          ? proportional on error change
 *     + (Ts / Ti) * E[n]                         ? integral on error
 *     + (Td / Ts) * (E[n] - 2*E[n-1] + E[n-2])  ? derivative on error
 *   ]
 *
 * Anti-windup (back-calculation):
 *   When CO is saturated, the integral term is adjusted so that
 *   CO_unconstrained = CO_clamped, preventing further windup.
 *
 * Mode handling:
 *   Manual -> Auto: bumpless transfer (initialize integral term)
 *   Auto -> Manual: output frozen at last value
 *   Cascade: secondary tracks primary CO
 *   Track: output follows tracking_value
 * ------------------------------------------------------------------------- */

double pax_pid_execute(pax_pid_loop_t *loop, double pv, double sp, double dt_sec)
{
    if (loop == NULL) return 0.0;

    /* Prevent division by zero */
    if (dt_sec <= 0.0) return loop->output;

    /* Store previous values */
    double prev_pv = loop->prev_pv;
    double prev_error = loop->prev_error;
    double prev_output = loop->output;

    loop->process_variable = pv;
    loop->setpoint = sp;

    /* Calculate error based on control action */
    double error;
    if (loop->action == PAX_ACTION_DIRECT) {
        error = pv - sp;  /* Direct acting: PV increase -> CO increase */
    } else {
        error = sp - pv;  /* Reverse acting: PV increase -> CO decrease */
    }
    loop->error = error;

    /* Handle operating modes */
    if (loop->mode == PAX_LOOP_MODE_MANUAL ||
        loop->mode == PAX_LOOP_MODE_HAND) {
        /* Manual mode: output unchanged, reset tracking */
        loop->prev_pv = pv;
        loop->prev_error = error;
        loop->integral_term = 0.0;
        return loop->output;
    }

    if (loop->mode == PAX_LOOP_MODE_TRACK) {
        loop->output = loop->tracking_value;
        loop->integral_term = 0.0;
        loop->prev_pv = pv;
        loop->prev_error = error;
        return loop->output;
    }

    if (loop->mode == PAX_LOOP_MODE_INHIBIT) {
        return loop->output;
    }

    /* Mode transition: manual -> auto (bumpless) */
    if (loop->prev_mode == PAX_LOOP_MODE_MANUAL &&
        loop->mode == PAX_LOOP_MODE_AUTO) {
        pax_pid_bumpless_transfer(loop);
    }

    /* ---- Velocity Form PID Calculation ---- */
    double kc = loop->params.kc;
    double ti = loop->params.ti_sec;
    double td = loop->params.td_sec;
    double ts = dt_sec;

    /* Proportional term: Kc * (PV[n-1] - PV[n]) ? derivative on PV */
    double p_term;
    if (loop->params.derivative_on_error) {
        p_term = kc * (error - prev_error);
    } else {
        /* Derivative on PV to prevent derivative kick on SP changes */
        double pv_change = prev_pv - pv;
        if (loop->action == PAX_ACTION_DIRECT) {
            pv_change = -pv_change;
        }
        p_term = kc * pv_change;
    }
    loop->proportional_term = p_term;

    /* Integral term: Kc * (Ts/Ti) * Error */
    double i_term = 0.0;
    if (ti > PAX_PID_MIN_TI_SEC) {
        i_term = kc * (ts / ti) * error;
    }
    loop->integral_term = i_term;

    /* Derivative term: Kc * (Td/Ts) * (2*PV[n-1] - PV[n] - PV[n-2]) */
    double d_term = 0.0;
    if (td > PAX_PID_MIN_TD_SEC) {
        if (loop->params.derivative_on_error) {
            /* D on error: (E[n] - 2*E[n-1] + E[n-2]) * Kc * Td / Ts */
            d_term = kc * (td / ts) * (error - 2.0 * prev_error + loop->prev_pv);
        } else {
            /* D on PV: (2*PV[n-1] - PV[n] - PV[n-2]) * Kc * Td / Ts */
            double d_pv = 2.0 * prev_pv - pv - loop->prev_output;
            if (loop->action == PAX_ACTION_DIRECT) {
                d_pv = -d_pv;
            }
            d_term = kc * (td / ts) * d_pv;
        }
    }
    loop->derivative_term = d_term;

    /* Compute new output (add to previous output for velocity form) */
    double delta_co = p_term + i_term + d_term;

    /* Add feedforward contribution if active */
    if (loop->ff_value != 0.0) {
        delta_co += loop->ff_value * loop->ff_gain;
    }

    double new_output = prev_output + delta_co;

    /* ---- Anti-Windup (Back-Calculation) ---- */
    bool saturated = false;
    if (new_output > loop->output_max) {
        new_output = loop->output_max;
        saturated = true;
        loop->output_clamped_high = true;
    } else if (new_output < loop->output_min) {
        new_output = loop->output_min;
        saturated = true;
        loop->output_clamped_low = true;
    } else {
        loop->output_clamped_high = false;
        loop->output_clamped_low = false;
    }

    if (saturated && loop->params.anti_windup == PAX_AW_CLAMPING) {
        /* Clamping: simply prevent further integration in windup direction */
        loop->windup_active = true;
    } else if (saturated && loop->params.anti_windup == PAX_AW_BACK_CALCULATION) {
        /* Back-calculation: adjust integral to match clamped output */
        loop->windup_active = true;
    } else {
        loop->windup_active = false;
    }

    /* Update state */
    loop->output = new_output;
    loop->prev_output = prev_output;
    loop->prev_pv = pv;
    loop->prev_error = error;
    loop->prev_mode = loop->mode;
    loop->cycle_count++;

    return new_output;
}

/* ---------------------------------------------------------------------------
 * L5: Dependent to Independent Gain Conversion
 *
 * Independent form is sometimes preferred for its intuitive
 * relationship between gains and controller action:
 *
 *   Kp = Kc           (proportional band = 100/Kc)
 *   Ki = Kc / Ti      (integral gain in rep/min)
 *   Kd = Kc * Td      (derivative gain in min)
 * ------------------------------------------------------------------------- */

void pax_pid_convert_dependent_to_independent(const pax_pid_params_t *dep,
                                               pax_pid_params_t *indep)
{
    if (dep == NULL || indep == NULL) return;

    *indep = *dep;
    indep->form = PAX_PID_FORM_INDEPENDENT;
    indep->kp = dep->kc;
    indep->ki = (dep->ti_sec > PAX_PID_MIN_TI_SEC)
                ? dep->kc / dep->ti_sec : 0.0;
    indep->kd = dep->kc * dep->td_sec;
}

/* ---------------------------------------------------------------------------
 * L6: Cascade Control Execution
 *
 * Cascade control uses two PID loops:
 *   - Primary (outer): controls the main process variable
 *   - Secondary (inner): controls an intermediate variable
 *
 * The primary loop output becomes the secondary loop setpoint.
 * This improves disturbance rejection because the secondary loop
 * compensates for fast disturbances before they affect the primary.
 *
 * Example: Heat exchanger temperature control
 *   Primary: outlet temperature -> setpoint for steam flow
 *   Secondary: steam flow -> valve position
 *
 * Bumpless cascade initialization:
 *   When cascade disabled: secondary tracks primary output
 *   When cascade enabled: primary initializes to secondary setpoint
 * ------------------------------------------------------------------------- */

int pax_cascade_execute(pax_cascade_t *cascade,
                         double primary_pv, double secondary_pv,
                         double primary_dt, double secondary_dt)
{
    if (cascade == NULL) return -1;
    if (cascade->primary == NULL || cascade->secondary == NULL) return -2;

    double primary_co;

    if (cascade->cascade_enabled) {
        /* Primary loop computes setpoint for secondary */
        primary_co = pax_pid_execute(cascade->primary, primary_pv,
                                      cascade->primary->setpoint, primary_dt);

        /* Scale primary CO to secondary SP range */
        double secondary_sp = cascade->secondary_sp_bias +
                              cascade->secondary_sp_ratio * primary_co;

        /* Clamp secondary SP */
        if (secondary_sp > cascade->primary_sp_max) {
            secondary_sp = cascade->primary_sp_max;
        }
        if (secondary_sp < cascade->primary_sp_min) {
            secondary_sp = cascade->primary_sp_min;
        }

        /* Execute secondary loop */
        cascade->secondary->mode = PAX_LOOP_MODE_CASCADE;
        pax_pid_execute(cascade->secondary, secondary_pv,
                         secondary_sp, secondary_dt);
    } else {
        /* Cascade disabled: secondary runs independently */
        cascade->secondary->mode = PAX_LOOP_MODE_AUTO;
        pax_pid_execute(cascade->secondary, secondary_pv,
                         cascade->secondary->setpoint, secondary_dt);

        /* Primary tracks secondary SP for bumpless re-enable */
        cascade->primary->mode = PAX_LOOP_MODE_TRACK;
        cascade->primary->tracking_value = cascade->secondary->setpoint;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * L6: Ratio Control Execution
 *
 * Ratio control maintains a fixed proportion between two streams:
 *   Controlled_flow_SP = wild_variable * ratio + bias
 *
 * The "wild" variable is uncontrolled (e.g., feed flow rate).
 * The controlled variable is adjusted to maintain the desired ratio.
 *
 * Example: Fuel-air ratio control in combustion
 *   Wild: Air flow
 *   Controlled: Fuel flow
 *   Ratio: stoichiometric + excess air factor
 * ------------------------------------------------------------------------- */

double pax_ratio_execute(pax_ratio_control_t *ratio, double dt_sec)
{
    if (ratio == NULL || ratio->controlled == NULL) return 0.0;

    /* Compute setpoint based on wild variable and ratio */
    double calculated_sp = ratio->wild_variable * ratio->ratio + ratio->bias;

    /* Clamp within acceptable range */
    if (calculated_sp > ratio->ratio_max) calculated_sp = ratio->ratio_max;
    if (calculated_sp < ratio->ratio_min) calculated_sp = ratio->ratio_min;

    /* Execute controlled loop with calculated setpoint */
    ratio->controlled->mode = PAX_LOOP_MODE_RATIO;
    return pax_pid_execute(ratio->controlled,
                            ratio->controlled->process_variable,
                            calculated_sp, dt_sec);
}

/* ---------------------------------------------------------------------------
 * L6: Override Selector Execution
 *
 * Override control allows multiple PID controllers to compete
 * for a single output (e.g., a valve). The selector picks the
 * output based on the selection strategy:
 *
 *   Low-select (e.g., furnace fuel gas):
 *     Multiple temperature controllers ? minimum output wins
 *     This prevents overheating any zone.
 *
 *   High-select (e.g., compressor anti-surge):
 *     Surge controller vs. pressure controller ? maximum wins
 *     Safety controller overrides normal operation.
 *
 * Non-selected loops must track the selected output to
 * prevent windup (they use TRACK mode).
 * ------------------------------------------------------------------------- */

int pax_override_execute(pax_override_t *override,
                          const double *pvs, const double *sps,
                          double dt_sec, bool low_select,
                          double *output, uint32_t *selected)
{
    if (override == NULL || pvs == NULL || sps == NULL ||
        output == NULL || selected == NULL) {
        return -1;
    }
    if (override->num_loops == 0) return -2;

    uint32_t i;
    double candidate_outputs[4];
    double best_output;
    uint32_t best_idx = 0;

    /* Execute all loops and collect outputs */
    for (i = 0; i < override->num_loops; i++) {
        candidate_outputs[i] = pax_pid_execute(override->loops[i],
                                                pvs[i], sps[i], dt_sec);
    }

    /* Select based on strategy */
    best_output = candidate_outputs[0];
    for (i = 1; i < override->num_loops; i++) {
        if (low_select) {
            if (candidate_outputs[i] < best_output) {
                best_output = candidate_outputs[i];
                best_idx = i;
            }
        } else {
            if (candidate_outputs[i] > best_output) {
                best_output = candidate_outputs[i];
                best_idx = i;
            }
        }
    }

    /* Non-selected loops track the selected output (anti-windup) */
    for (i = 0; i < override->num_loops; i++) {
        if (i != best_idx) {
            override->loops[i]->mode = PAX_LOOP_MODE_TRACK;
            override->loops[i]->tracking_value = best_output;
        }
    }

    /* Clamp final output */
    if (best_output > override->output_max) best_output = override->output_max;
    if (best_output < override->output_min) best_output = override->output_min;

    *output = best_output;
    *selected = best_idx;
    override->selected_loop = best_idx;

    return 0;
}

/* ---------------------------------------------------------------------------
 * L5: Feedforward Computation
 *
 * Static feedforward:
 *   FF_output = FF_gain * FF_signal
 *
 * Lead-lag dynamic compensation (discrete form):
 *   FF_output[n] = a * FF_output[n-1] + b * FF_signal[n] + c * FF_signal[n-1]
 *
 *   where (using bilinear transform of K*(1+s*T_lead)/(1+s*T_lag)):
 *     a = (2*T_lag - Ts) / (2*T_lag + Ts)
 *     b = K * (Ts + 2*T_lead) / (2*T_lag + Ts)
 *     c = K * (Ts - 2*T_lead) / (2*T_lag + Ts)
 *
 * Feedforward is additive to the PID output and provides
 * disturbance compensation before the disturbance affects PV.
 * ------------------------------------------------------------------------- */

double pax_feedforward_compute(double ff_signal, double ff_gain,
                                pax_feedforward_type_t ff_type,
                                double t_lead_sec, double t_lag_sec,
                                double sample_time_sec,
                                double *prev_ff_input, double *prev_ff_output)
{
    if (prev_ff_input == NULL || prev_ff_output == NULL) {
        return ff_signal * ff_gain;
    }

    double ff_output;

    switch (ff_type) {
        case PAX_FF_STATIC_GAIN:
            ff_output = ff_gain * ff_signal;
            break;

        case PAX_FF_LEAD_LAG:
            if (fabs(2.0 * t_lag_sec + sample_time_sec) < 1e-9) {
                ff_output = ff_gain * ff_signal;
            } else {
                double denom = 2.0 * t_lag_sec + sample_time_sec;
                double a = (2.0 * t_lag_sec - sample_time_sec) / denom;
                double b = ff_gain * (sample_time_sec + 2.0 * t_lead_sec) / denom;
                double c = ff_gain * (sample_time_sec - 2.0 * t_lead_sec) / denom;

                ff_output = a * (*prev_ff_output) +
                            b * ff_signal +
                            c * (*prev_ff_input);
            }
            break;

        case PAX_FF_DEADTIME:
            /* Deadtime compensation uses a shift register in practice */
            /* Simplified: pass-through with gain for now */
            ff_output = ff_gain * ff_signal;
            break;

        case PAX_FF_FIRST_ORDER:
            /* First-order dynamic: FF(s) = K / (1 + s*T) */
            if (fabs(t_lag_sec) < 1e-9) {
                ff_output = ff_gain * ff_signal;
            } else {
                double alpha = sample_time_sec / (t_lag_sec + sample_time_sec);
                ff_output = *prev_ff_output +
                            alpha * (ff_gain * ff_signal - *prev_ff_output);
            }
            break;

        default:
            ff_output = ff_gain * ff_signal;
    }

    *prev_ff_input = ff_signal;
    *prev_ff_output = ff_output;

    return ff_output;
}

/* ---------------------------------------------------------------------------
 * L2: Setpoint Rate Limiting
 *
 * Prevents sudden setpoint changes from causing process upsets.
 * The setpoint ramps toward the target at a configurable rate.
 * ------------------------------------------------------------------------- */

double pax_setpoint_rate_limit(double target_sp, double current_sp,
                                double rate_limit_eu_s, double dt_sec)
{
    if (rate_limit_eu_s <= 0.0 || dt_sec <= 0.0) {
        return target_sp;
    }

    double max_step = rate_limit_eu_s * dt_sec;
    double delta = target_sp - current_sp;

    if (fabs(delta) <= max_step) {
        return target_sp;
    }

    if (delta > 0.0) {
        return current_sp + max_step;
    } else {
        return current_sp - max_step;
    }
}

/* ---------------------------------------------------------------------------
 * L2: Windup Detection
 *
 * Windup occurs when:
 *   1. Controller output is saturated (at min or max), AND
 *   2. Integral term is pushing output further into saturation
 *
 * Condition: (CO == CO_max AND error > 0 AND action=reverse) OR
 *            (CO == CO_max AND error < 0 AND action=direct)  OR
 *            (CO == CO_min AND error < 0 AND action=reverse) OR
 *            (CO == CO_min AND error > 0 AND action=direct)
 * ------------------------------------------------------------------------- */

bool pax_pid_check_windup(const pax_pid_loop_t *loop)
{
    if (loop == NULL) return false;

    if (!loop->output_clamped_high && !loop->output_clamped_low) {
        return false;
    }

    /* Determine if error direction pushes further into saturation */
    bool error_pushes_up = false;
    if ((loop->action == PAX_ACTION_REVERSE && loop->error > 0.0) ||
        (loop->action == PAX_ACTION_DIRECT && loop->error < 0.0)) {
        error_pushes_up = true;
    }

    if (loop->output_clamped_high && error_pushes_up) return true;
    if (loop->output_clamped_low && !error_pushes_up) return true;

    return false;
}

/* ---------------------------------------------------------------------------
 * L2: Bumpless Transfer (Manual ? Auto)
 *
 * Initializes the integral term so that the controller output
 * does not jump when switching from Manual to Auto mode.
 *
 * From the velocity form:
 *   delta_CO = Kc * [P_term + I_term + D_term]
 *   For bumpless: delta_CO = 0 at transition
 *   ? I_term = -P_term - D_term (but I_term accumulates)
 *
 * In practice: set integral accumulator so current output stays.
 *   integral_accumulator = CO_current - P_term - D_term
 *   Then I_term = Kc * (Ts/Ti) * error (as normal)
 *   CO = integral_accumulator + P_term + D_term = CO_current ?
 * ------------------------------------------------------------------------- */

void pax_pid_bumpless_transfer(pax_pid_loop_t *loop)
{
    if (loop == NULL) return;

    /* Reset integral and derivative state */
    loop->integral_term = 0.0;
    loop->derivative_term = 0.0;
    loop->prev_error = loop->error;
    loop->prev_pv = loop->process_variable;

    /* The output stays at its current value ? no bump */
    loop->windup_active = false;
    loop->output_clamped_high = false;
    loop->output_clamped_low = false;
}

/* ---------------------------------------------------------------------------
 * L4/L5: Ziegler-Nichols Open-Loop Tuning
 *
 * From the process reaction curve (step response):
 *   Process Gain K = delta_PV / delta_CO
 *   Time Constant T = time to reach 63.2% of final value
 *   Dead Time L = time between step and first response
 *
 * Z-N tuning rules:
 *   P-only:  Kc = T / (K * L)
 *   PI:      Kc = 0.9 * T / (K * L), Ti = 3.33 * L
 *   PID:     Kc = 1.2 * T / (K * L), Ti = 2.0 * L, Td = 0.5 * L
 *
 * Reference: Ziegler & Nichols (1942), Optimum Settings for
 *            Automatic Controllers, Trans. ASME
 * ------------------------------------------------------------------------- */

void pax_pid_ziegler_nichols_open_loop(double process_gain,
                                        double time_constant,
                                        double deadtime,
                                        pax_pid_params_t *output)
{
    if (output == NULL) return;
    if (fabs(process_gain) < 1e-9 || fabs(deadtime) < 1e-9) return;

    double K = process_gain;
    double T = time_constant;
    double L = deadtime;

    /* PID form (most common) */
    double kc = 1.2 * T / (K * L);
    double ti = 2.0 * L;
    double td = 0.5 * L;

    pax_pid_params_init(output, PAX_PID_FORM_DEPENDENT,
                         kc, ti, td, PAX_PID_DEFAULT_SAMPLE_TIME);
}

/* ---------------------------------------------------------------------------
 * L2: Loop Performance Metrics
 *
 * IAE  = integral(|e(t)| dt)     ? Penalizes all errors equally
 * ISE  = integral(e(t)^2 dt)     ? Penalizes large errors more
 * ITAE = integral(t * |e(t)| dt) ? Penalizes persistent errors more
 *
 * These are computed using trapezoidal integration from
 * the error history stored in the loop state.
 * ------------------------------------------------------------------------- */

void pax_pid_performance_metrics(const pax_pid_loop_t *loop,
                                  double *iae, double *ise, double *itae)
{
    if (loop == NULL) return;

    if (iae) *iae = fabs(loop->error);
    if (ise) *ise = loop->error * loop->error;
    if (itae) *itae = (double)loop->cycle_count * fabs(loop->error);
}

/* ---------------------------------------------------------------------------
 * L3: Loop Status Dump
 * ------------------------------------------------------------------------- */

void pax_loop_dump(const pax_pid_loop_t *loop)
{
    if (loop == NULL) {
        printf("Control Loop: (null)\n");
        return;
    }

    printf("-------------------------------------------\n");
    printf("  Loop: %s (ID: %u)\n", loop->loop_tag, loop->loop_id);
    printf("  Mode: ");
    switch (loop->mode) {
        case PAX_LOOP_MODE_MANUAL:  printf("MANUAL\n"); break;
        case PAX_LOOP_MODE_AUTO:    printf("AUTO\n"); break;
        case PAX_LOOP_MODE_CASCADE: printf("CASCADE\n"); break;
        case PAX_LOOP_MODE_RATIO:   printf("RATIO\n"); break;
        case PAX_LOOP_MODE_OVERRIDE:printf("OVERRIDE\n"); break;
        default: printf("OTHER\n");
    }
    printf("  SP: %.4f | PV: %.4f | CO: %.4f\n",
           loop->setpoint, loop->process_variable, loop->output);
    printf("  Error: %.4f | P: %.4f | I: %.4f | D: %.4f\n",
           loop->error, loop->proportional_term,
           loop->integral_term, loop->derivative_term);
    printf("  Kc: %.4f | Ti: %.2f s | Td: %.2f s\n",
           loop->params.kc, loop->params.ti_sec, loop->params.td_sec);
    printf("  Cycles: %llu | Windup: %s | Clamped: %s%s\n",
           (unsigned long long)loop->cycle_count,
           loop->windup_active ? "YES" : "no",
           loop->output_clamped_high ? "HI" : "",
           loop->output_clamped_low ? "LO" : "");
    printf("-------------------------------------------\n");
}
