/**
 * @file    logix_instruction_set.c
 * @brief   ControlLogix Instruction Set (Timer, Counter, TOT, D2SD, D3SD, LDLG, SRTP, SCL)
 * L5: Process and general instruction algorithms
 */

#include <string.h>
#include <math.h>
#include "logix_instruction_set.h"

/* ========================================================================
 * Timer Instructions
 * ======================================================================== */

void logix_timer_init(logix_timer_t *timer, logix_timer_type_t type,
                       uint32_t preset_ms)
{
    if (!timer) return;
    memset(timer, 0, sizeof(*timer));
    timer->type = type;
    timer->pre = preset_ms;
}

bool logix_timer_execute(logix_timer_t *timer, bool enable,
                          uint64_t current_time_ms)
{
    if (!timer) return false;

    timer->en = enable;

    switch (timer->type) {
        case TIMER_TYPE_TON:
            if (enable) {
                /* Accumulate time */
                if (timer->last_time_ms > 0 && current_time_ms > timer->last_time_ms) {
                    timer->acc += (uint32_t)(current_time_ms - timer->last_time_ms);
                }
                timer->tt = (timer->acc < timer->pre);
                timer->dn = (timer->acc >= timer->pre);
            } else {
                timer->acc = 0;
                timer->tt = false;
                timer->dn = false;
            }
            break;

        case TIMER_TYPE_TOF:
            if (enable) {
                timer->acc = 0;
                timer->tt = true;
                timer->dn = true;
            } else {
                if (timer->last_time_ms > 0 && current_time_ms > timer->last_time_ms) {
                    timer->acc += (uint32_t)(current_time_ms - timer->last_time_ms);
                }
                timer->tt = (timer->acc < timer->pre);
                timer->dn = (timer->acc < timer->pre);
            }
            break;

        case TIMER_TYPE_RTO:
            if (enable) {
                if (timer->last_time_ms > 0 && current_time_ms > timer->last_time_ms) {
                    timer->acc += (uint32_t)(current_time_ms - timer->last_time_ms);
                }
                timer->tt = (timer->acc < timer->pre);
                timer->dn = (timer->acc >= timer->pre);
            }
            /* RTO: ACC retained when enable goes false */
            break;
    }

    /* Clamp accumulator to preset */
    if (timer->acc > timer->pre) timer->acc = timer->pre;

    timer->last_time_ms = current_time_ms;
    return timer->dn;
}

void logix_timer_reset(logix_timer_t *timer)
{
    if (!timer) return;
    timer->acc = 0;
    timer->dn = false;
    timer->tt = false;
    timer->en = false;
}

/* ========================================================================
 * Counter Instructions
 * ======================================================================== */

void logix_counter_init(logix_counter_t *ctr, int32_t preset)
{
    if (!ctr) return;
    memset(ctr, 0, sizeof(*ctr));
    ctr->pre = preset;
}

void logix_counter_execute(logix_counter_t *ctr, bool cu, bool cd, bool res)
{
    if (!ctr) return;

    if (res) {
        ctr->acc = 0;
        ctr->dn = false;
        ctr->ov = false;
        ctr->un = false;
        ctr->cu_prev = false;
        ctr->cd_prev = false;
        return;
    }

    /* Rising edge detect on CU */
    if (cu && !ctr->cu_prev) {
        if (ctr->acc < INT32_MAX) {
            ctr->acc++;
        } else {
            ctr->ov = true;
        }
    }

    /* Rising edge detect on CD */
    if (cd && !ctr->cd_prev) {
        if (ctr->acc > INT32_MIN) {
            ctr->acc--;
        } else {
            ctr->un = true;
        }
    }

    ctr->cu_prev = cu;
    ctr->cd_prev = cd;
    ctr->dn = (ctr->acc >= ctr->pre);
}

void logix_counter_reset(logix_counter_t *ctr)
{
    if (!ctr) return;
    ctr->acc = 0;
    ctr->dn = false;
    ctr->ov = false;
    ctr->un = false;
}

/* ========================================================================
 * Totalizer (TOT)
 *
 * Integrates rate signal over time using trapezoidal rule:
 *   Total(n) = Total(n-1) + (Rate(n) + Rate(n-1)) / 2 * Ts * Gain * Factor
 * ======================================================================== */

void logix_tot_init(logix_tot_t *tot, double gain, double time_base_factor,
                     double ts_sec)
{
    if (!tot) return;
    memset(tot, 0, sizeof(*tot));
    tot->gain = (gain != 0.0) ? gain : 1.0;
    tot->time_base_factor = (time_base_factor != 0.0) ? time_base_factor : 1.0;
    tot->ts_sec = (ts_sec > 0.0) ? ts_sec : 1.0;
}

double logix_tot_execute(logix_tot_t *tot, double rate, bool enable, bool reset)
{
    if (!tot) return 0.0;

    if (reset) {
        tot->total = 0.0;
        tot->rate_prev = 0.0;
        tot->target_reached_1 = false;
        tot->target_reached_2 = false;
    }

    tot->rate = rate;

    if (enable && !reset) {
        /* Trapezoidal integration */
        double avg_rate = (rate + tot->rate_prev) / 2.0;
        double increment = avg_rate * tot->ts_sec * tot->gain * tot->time_base_factor;

        tot->total += increment;

        /* Check for overflow/underflow */
        if (tot->total > 1e15) {
            tot->total_overflow = true;
            tot->total = 1e15; /* Clamp */
        }
        if (tot->total < 0.0 && tot->target <= 0.0) {
            /* Allow negative totals for consumption totalizers */
        }

        /* Target deviation checks */
        if (tot->target_dev_1 > 0.0) {
            tot->target_reached_1 = (fabs(tot->total - tot->target) <= tot->target_dev_1);
        }
        if (tot->target_dev_2 > 0.0) {
            tot->target_reached_2 = (fabs(tot->total - tot->target) <= tot->target_dev_2);
        }
    }

    tot->rate_prev = rate;
    tot->enable = enable;
    return tot->total;
}

void logix_tot_set_target(logix_tot_t *tot, double target,
                           double dev1, double dev2)
{
    if (!tot) return;
    tot->target = target;
    tot->target_dev_1 = dev1;
    tot->target_dev_2 = dev2;
}

/* ========================================================================
 * Discrete 2-State Device (D2SD)
 * ======================================================================== */

void logix_d2sd_init(logix_d2sd_t *dev, uint32_t transition_time_ms)
{
    if (!dev) return;
    memset(dev, 0, sizeof(*dev));
    dev->state = D2SD_STATE_READY;
    dev->transition_time_ms = transition_time_ms;
}

d2sd_state_t logix_d2sd_execute(logix_d2sd_t *dev, bool cmd,
                                  bool fbk, bool permissive,
                                  bool fault_reset, uint64_t time_ms)
{
    if (!dev) return D2SD_STATE_FAULTED;

    dev->command_0 = cmd;
    dev->feedback_0 = fbk;
    dev->permissive_ok = permissive;
    dev->fault_reset = fault_reset;

    switch (dev->state) {
        case D2SD_STATE_READY:
            if (cmd && permissive) {
                dev->state = D2SD_STATE_STARTING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            } else if (cmd && !permissive) {
                dev->state = D2SD_STATE_INTERLOCKED;
                dev->interlock_trip = true;
            }
            break;

        case D2SD_STATE_STARTING:
            dev->elapsed_time_ms = (uint32_t)(time_ms - dev->state_entry_time_ms);
            if (fbk) {
                dev->state = D2SD_STATE_RUNNING;
            } else if (dev->elapsed_time_ms > dev->transition_time_ms) {
                dev->state = D2SD_STATE_FAULTED;
                dev->fault_code = 1; /* Fail to start */
                dev->fault_0 = true;
            } else if (!cmd) {
                dev->state = D2SD_STATE_STOPPING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            }
            break;

        case D2SD_STATE_RUNNING:
            if (!cmd) {
                dev->state = D2SD_STATE_STOPPING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            } else if (!fbk) {
                /* Lost feedback while running */
                dev->state = D2SD_STATE_FAULTED;
                dev->fault_code = 2; /* Fail while running */
                dev->fault_0 = true;
            } else if (!permissive) {
                dev->state = D2SD_STATE_INTERLOCKED;
                dev->interlock_trip = true;
            }
            break;

        case D2SD_STATE_STOPPING:
            dev->elapsed_time_ms = (uint32_t)(time_ms - dev->state_entry_time_ms);
            if (!fbk) {
                dev->state = D2SD_STATE_READY;
            } else if (dev->elapsed_time_ms > dev->transition_time_ms) {
                dev->state = D2SD_STATE_FAULTED;
                dev->fault_code = 2; /* Fail to stop */
                dev->fault_0 = true;
            }
            break;

        case D2SD_STATE_FAULTED:
            if (fault_reset) {
                dev->state = D2SD_STATE_READY;
                dev->fault_0 = false;
                dev->fault_code = 0;
            }
            break;

        case D2SD_STATE_INTERLOCKED:
            if (permissive) {
                dev->state = D2SD_STATE_READY;
                dev->interlock_trip = false;
            }
            break;
    }

    dev->alarm_0 = (dev->state == D2SD_STATE_FAULTED);
    return dev->state;
}

/* ========================================================================
 * Discrete 3-State Device (D3SD)
 * ======================================================================== */

void logix_d3sd_init(logix_d3sd_t *dev, uint32_t open_time_ms,
                      uint32_t close_time_ms)
{
    if (!dev) return;
    memset(dev, 0, sizeof(*dev));
    dev->state = D3SD_STATE_CLOSED;
    dev->open_time_ms = open_time_ms;
    dev->close_time_ms = close_time_ms;
}

d3sd_state_t logix_d3sd_execute(logix_d3sd_t *dev, bool cmd_open,
                                  bool cmd_close, bool cmd_stop,
                                  bool fbk_open, bool fbk_close,
                                  bool permissive, bool fault_reset,
                                  uint64_t time_ms)
{
    if (!dev) return D3SD_STATE_FAULTED;

    dev->cmd_open = cmd_open;
    dev->cmd_close = cmd_close;
    dev->cmd_stop = cmd_stop;
    dev->fbk_open = fbk_open;
    dev->fbk_close = fbk_close;
    dev->permissive_ok = permissive;
    dev->fault_reset = fault_reset;

    switch (dev->state) {
        case D3SD_STATE_CLOSED:
            if (cmd_open && permissive) {
                dev->state = D3SD_STATE_OPENING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            }
            break;

        case D3SD_STATE_OPENING:
            dev->elapsed_time_ms = (uint32_t)(time_ms - dev->state_entry_time_ms);
            if (fbk_open) {
                dev->state = D3SD_STATE_OPEN;
            } else if (cmd_stop) {
                dev->state = D3SD_STATE_STOPPED;
            } else if (dev->elapsed_time_ms > dev->open_time_ms) {
                dev->state = D3SD_STATE_FAULTED;
                dev->fault_code = 1; /* Fail to open */
                dev->fault_0 = true;
            }
            break;

        case D3SD_STATE_OPEN:
            if (cmd_close && permissive) {
                dev->state = D3SD_STATE_CLOSING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            }
            break;

        case D3SD_STATE_CLOSING:
            dev->elapsed_time_ms = (uint32_t)(time_ms - dev->state_entry_time_ms);
            if (fbk_close) {
                dev->state = D3SD_STATE_CLOSED;
            } else if (cmd_stop) {
                dev->state = D3SD_STATE_STOPPED;
            } else if (dev->elapsed_time_ms > dev->close_time_ms) {
                dev->state = D3SD_STATE_FAULTED;
                dev->fault_code = 2; /* Fail to close */
                dev->fault_0 = true;
            }
            break;

        case D3SD_STATE_STOPPED:
            if (cmd_open && permissive) {
                dev->state = D3SD_STATE_OPENING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            } else if (cmd_close && permissive) {
                dev->state = D3SD_STATE_CLOSING;
                dev->state_entry_time_ms = time_ms;
                dev->elapsed_time_ms = 0;
            }
            break;

        case D3SD_STATE_FAULTED:
            if (fault_reset) {
                dev->state = (fbk_open) ? D3SD_STATE_OPEN :
                            (fbk_close) ? D3SD_STATE_CLOSED :
                            D3SD_STATE_STOPPED;
                dev->fault_0 = false;
                dev->fault_code = 0;
            }
            break;

        case D3SD_STATE_INTERLOCKED:
            if (permissive) {
                dev->state = D3SD_STATE_STOPPED;
                dev->interlock_trip = false;
            }
            break;
    }

    dev->alarm_0 = (dev->state == D3SD_STATE_FAULTED);
    return dev->state;
}

/* ========================================================================
 * Low-Pass Lead-Lag Filter (LDLG)
 *
 * H(s) = K * (T_lead*s + 1) / (T_lag*s + 1)^order
 *
 * Order 1 (backward Euler):
 *   y(n) = a * y(n-1) + b * [K*x(n) + c*(K*x(n) - K*x(n-1))]
 *   where: a = T_lag/(T_lag+Ts), b = Ts/(T_lag+Ts), c = T_lead/Ts
 * ======================================================================== */

void logix_ldlg_init(logix_ldlg_t *filter, double k, double t_lead_sec,
                      double t_lag_sec, uint32_t order, double ts_sec)
{
    if (!filter) return;
    memset(filter, 0, sizeof(*filter));
    filter->k = (k != 0.0) ? k : 1.0;
    filter->t_lead_sec = (t_lead_sec >= 0.0) ? t_lead_sec : 0.0;
    filter->t_lag_sec = (t_lag_sec > 0.0) ? t_lag_sec : 1.0;
    filter->order = (order >= 1 && order <= 2) ? order : 1;
    filter->ts_sec = (ts_sec > 0.0) ? ts_sec : 0.1;
}

double logix_ldlg_execute(logix_ldlg_t *filter, double input)
{
    if (!filter) return 0.0;
    if (filter->hold) return filter->output;

    filter->input = input;
    double ts = filter->ts_sec;
    double tau = filter->t_lag_sec;
    double k = filter->k;

    /* Backward difference coefficients */
    double a = tau / (tau + ts);
    double b = ts / (tau + ts);

    if (filter->order == 1) {
        if (!filter->initialized) {
            /* Initialize: y = K * x (steady state) */
            filter->output = k * input;
            filter->x_prev = input;
            filter->y_prev = filter->output;
            filter->initialized = true;
        } else {
            /* Lead term: c * (x(n) - x(n-1)) */
            double lead_term = 0.0;
            if (filter->t_lead_sec > 0.0) {
                double c = k * filter->t_lead_sec / ts;
                lead_term = c * (input - filter->x_prev);
            }

            /* y(n) = a * y(n-1) + b * (k*x(n) + lead_term) */
            filter->output = a * filter->y_prev + b * (k * input + lead_term);

            filter->x_prev = input;
            filter->y_prev = filter->output;
        }
    } else {
        /* Second order: cascade two first-order filters */
        if (!filter->initialized) {
            filter->output = k * input;
            filter->x_prev = input;
            filter->x_prev2 = input;
            filter->y_prev = filter->output;
            filter->y_prev2 = filter->output;
            filter->initialized = true;
        } else {
            /* First stage */
            double lead_term1 = 0.0;
            if (filter->t_lead_sec > 0.0) {
                double c = k * filter->t_lead_sec / ts;
                lead_term1 = c * (input - filter->x_prev);
            }
            double y1 = a * filter->y_prev + b * (k * input + lead_term1);

            /* Second stage */
            double lead_term2 = 0.0;
            if (filter->t_lead_sec > 0.0) {
                double c = k * filter->t_lead_sec / ts;
                lead_term2 = c * (y1 - filter->x_prev2);
            }
            filter->output = a * filter->y_prev2 + b * (k * y1 + lead_term2);

            filter->x_prev = input;
            filter->x_prev2 = y1;
            filter->y_prev = y1;
            filter->y_prev2 = filter->output;
        }
    }

    return filter->output;
}

void logix_ldlg_reset(logix_ldlg_t *filter)
{
    if (!filter) return;
    filter->initialized = false;
    filter->output = 0.0;
    filter->x_prev = 0.0;
    filter->x_prev2 = 0.0;
    filter->y_prev = 0.0;
    filter->y_prev2 = 0.0;
}

void logix_ldlg_hold(logix_ldlg_t *filter, bool hold)
{
    if (!filter) return;
    filter->hold = hold;
}

/* ========================================================================
 * Split Range Time Proportioning (SRTP)
 * ======================================================================== */

void logix_srtp_init(logix_srtp_t *srtp, double split_point,
                      double deadband_pct, double cycle_period_sec,
                      double min_on_sec, double min_off_sec,
                      double ts_sec)
{
    if (!srtp) return;
    memset(srtp, 0, sizeof(*srtp));
    srtp->split_point = (split_point > 0.0) ? split_point : 50.0;
    srtp->deadband_pct = (deadband_pct >= 0.0) ? deadband_pct : 2.0;
    srtp->cycle_period_sec = (cycle_period_sec > 0.0) ? cycle_period_sec : 10.0;
    srtp->min_on_time_sec = (min_on_sec >= 0.0) ? min_on_sec : 0.1;
    srtp->min_off_time_sec = (min_off_sec >= 0.0) ? min_off_sec : 0.1;
    srtp->ts_sec = (ts_sec > 0.0) ? ts_sec : 0.1;
}

void logix_srtp_execute(logix_srtp_t *srtp, double cv_input,
                         bool *output_0, bool *output_1)
{
    if (!srtp || !output_0 || !output_1) return;

    srtp->input_cv = cv_input;

    /* Deadband zone around split point */
    double half_db = srtp->deadband_pct / 2.0;
    double db_low = srtp->split_point - half_db;
    double db_high = srtp->split_point + half_db;

    /* Update cycle timing */
    srtp->elapsed_cycle += srtp->ts_sec;
    if (srtp->elapsed_cycle >= srtp->cycle_period_sec) {
        srtp->elapsed_cycle -= srtp->cycle_period_sec;
        srtp->accum_time_0 = 0.0;
        srtp->accum_time_1 = 0.0;
    }

    /* Device 0 (below split point, e.g., cooling) */
    if (cv_input < db_low && cv_input >= 0.0) {
        /* Duty cycle: how much of the cycle should be on */
        double duty = 1.0 - (cv_input / db_low);
        if (duty < 0.0) duty = 0.0;
        if (duty > 1.0) duty = 1.0;

        double on_time_target = duty * srtp->cycle_period_sec;

        /* Apply minimum on/off time constraints */
        if (on_time_target < srtp->min_on_time_sec && on_time_target > 0.0) {
            on_time_target = srtp->min_on_time_sec;
        }

        *output_0 = (srtp->accum_time_0 < on_time_target);
        *output_1 = false;

        if (*output_0) {
            srtp->accum_time_0 += srtp->ts_sec;
        }
    }
    /* Device 1 (above split point, e.g., heating) */
    else if (cv_input > db_high && cv_input <= 100.0) {
        double range = 100.0 - db_high;
        double duty = (cv_input - db_high) / range;
        if (duty < 0.0) duty = 0.0;
        if (duty > 1.0) duty = 1.0;

        double on_time_target = duty * srtp->cycle_period_sec;

        if (on_time_target < srtp->min_on_time_sec && on_time_target > 0.0) {
            on_time_target = srtp->min_on_time_sec;
        }

        *output_0 = false;
        *output_1 = (srtp->accum_time_1 < on_time_target);

        if (*output_1) {
            srtp->accum_time_1 += srtp->ts_sec;
        }
    }
    /* Deadband: both off */
    else {
        *output_0 = false;
        *output_1 = false;
    }

    /* Minimum off-time: if output is forced off due to cycle reset,
     * enforce minimum off-time before turning on again */
    (void)srtp->min_off_time_sec; /* Reserved for enhanced implementation */
}

/* ========================================================================
 * Scale (SCL)
 * ======================================================================== */

void logix_scl_init(logix_scl_t *scl, double in_min, double in_max,
                     double out_min, double out_max, bool limit_input)
{
    if (!scl) return;
    memset(scl, 0, sizeof(*scl));
    scl->in_min = in_min;
    scl->in_max = in_max;
    scl->out_min = out_min;
    scl->out_max = out_max;
    scl->limit_input = limit_input;
}

double logix_scl_execute(logix_scl_t *scl, double input)
{
    if (!scl) return 0.0;

    scl->input = input;
    double in_range = scl->in_max - scl->in_min;

    /* Guard against division by zero */
    if (fabs(in_range) < 1e-15) {
        scl->output = scl->out_min;
        scl->input_in_range = false;
        return scl->output;
    }

    /* Apply input limiting if configured */
    double effective_input = input;
    if (scl->limit_input) {
        if (input < scl->in_min) effective_input = scl->in_min;
        if (input > scl->in_max) effective_input = scl->in_max;
    }

    scl->input_in_range = (input >= scl->in_min && input <= scl->in_max);

    /* Linear scaling:
     * Output = (Input - InMin) / (InMax - InMin) * (OutMax - OutMin) + OutMin */
    double out_range = scl->out_max - scl->out_min;
    scl->output = (effective_input - scl->in_min) / in_range * out_range + scl->out_min;

    return scl->output;
}