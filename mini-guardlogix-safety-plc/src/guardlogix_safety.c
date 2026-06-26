/**
 * guardlogix_safety.c -- GuardLogix Safety Controller Core Implementation
 *
 * Implements core safety controller initialization, state transitions,
 * 1oo2D cross-check diagnostics, partner signature verification,
 * proof test interval computation, and safety reaction time calculation.
 *
 * Knowledge points covered:
 *   L1: Safety controller object model, state machine
 *   L2: 1oo2D dual-channel processing, watchdog, safety reaction time
 *   L3: Engineering implementation of GuardLogix controller structure
 *   L4: PFDavg equation (IEC 61508-6) for 1oo2D architecture
 *   L5: Binary search for proof test interval optimization
 */

#include "guardlogix_safety.h"
#include "guardlogix_signature.h"
#include <string.h>
#include <stdlib.h>

int glx_safety_controller_init(glx_safety_controller_t *ctrl,
                                glx_controller_family_t family,
                                int is_primary,
                                uint32_t period_ms)
{
    if (!ctrl) return -1;
    if (period_ms < 1 || period_ms > 500) return -1;

    memset(ctrl, 0, sizeof(*ctrl));

    ctrl->controller_id = 1;
    ctrl->family = family;
    ctrl->state = GLX_STATE_POWER_UP;
    ctrl->lock_state = GLX_LOCK_STATE_LOCKED;
    ctrl->my_role = is_primary ? GLX_PARTNER_PRIMARY : GLX_PARTNER_SECONDARY;
    ctrl->partner_status = GLX_PARTNER_OFFLINE;

    switch (family) {
    case GLX_CTRL_5560S:
        ctrl->max_sil = GLX_SIL_3;
        ctrl->max_pl = GLX_PL_E;
        ctrl->max_safety_connections = 16;
        break;
    case GLX_CTRL_5570S:
        ctrl->max_sil = GLX_SIL_3;
        ctrl->max_pl = GLX_PL_E;
        ctrl->max_safety_connections = 32;
        break;
    case GLX_CTRL_5580S:
        ctrl->max_sil = GLX_SIL_3;
        ctrl->max_pl = GLX_PL_E;
        ctrl->max_safety_connections = 64;
        break;
    case GLX_CTRL_COMPACT:
        ctrl->max_sil = GLX_SIL_3;
        ctrl->max_pl = GLX_PL_E;
        ctrl->max_safety_connections = 16;
        break;
    default:
        return -1;
    }

    ctrl->safety_task_type = GLX_SAFETY_TASK_PERIODIC;
    ctrl->safety_task_period_ms = period_ms;
    ctrl->safety_task_max_scan_us = period_ms * 1000;

    ctrl->watchdog.watchdog_timeout_ms = period_ms * 3;
    ctrl->watchdog.max_task_period_ms = period_ms;
    ctrl->watchdog.watchdog_enabled = 1;
    ctrl->watchdog.last_reset_cycle = 0;
    ctrl->watchdog.overrun_count = 0;

    ctrl->diag_1oo2d.diagnostic_active = 1;
    ctrl->diag_1oo2d.cross_check_passing = 1;
    ctrl->diag_1oo2d.diverse_compiler_ok = 1;
    ctrl->diag_1oo2d.cycle_counter_synced = 0;
    ctrl->diag_1oo2d.mismatch_count = 0;
    ctrl->diag_1oo2d.last_mismatch_cycle = 0;
    ctrl->diag_1oo2d.cross_check_latency_us = 0;

    ctrl->snn.snn_value = 1;
    ctrl->snn.snn_format = 0;
    ctrl->snn.snn_uniqueness_valid = 0;
    ctrl->snn.snn_generation_time = 0;

    ctrl->proof_test_interval_hours = 8760;
    ctrl->last_proof_test_time = 0;
    ctrl->proof_test_due_time = 8760;

    ctrl->safety_input_count = 0;
    ctrl->safety_output_count = 0;

    return 0;
}

static int is_valid_transition(glx_controller_state_t from,
                                glx_controller_state_t to)
{
    if (from == to) return 1;

    switch (from) {
    case GLX_STATE_POWER_UP:
        return (to == GLX_STATE_SAFETY_LOCKED ||
                to == GLX_STATE_HARD_FAULT);
    case GLX_STATE_SAFETY_LOCKED:
        return (to == GLX_STATE_SAFETY_FAULT ||
                to == GLX_STATE_SAFETY_TASK_OVERRUN ||
                to == GLX_STATE_FIRMWARE_UPDATE ||
                to == GLX_STATE_HARD_FAULT);
    case GLX_STATE_SAFETY_FAULT:
        return (to == GLX_STATE_SAFETY_LOCKED ||
                to == GLX_STATE_HARD_FAULT);
    case GLX_STATE_SAFETY_TASK_OVERRUN:
        return (to == GLX_STATE_SAFETY_FAULT ||
                to == GLX_STATE_HARD_FAULT);
    case GLX_STATE_FIRMWARE_UPDATE:
        return (to == GLX_STATE_POWER_UP ||
                to == GLX_STATE_HARD_FAULT);
    case GLX_STATE_HARD_FAULT:
        return 0;
    default:
        return 0;
    }
}

static const char* state_name(glx_controller_state_t s)
{
    switch (s) {
    case GLX_STATE_POWER_UP:            return "POWER_UP";
    case GLX_STATE_SAFETY_LOCKED:       return "SAFETY_LOCKED";
    case GLX_STATE_SAFETY_FAULT:        return "SAFETY_FAULT";
    case GLX_STATE_SAFETY_TASK_OVERRUN: return "SAFETY_TASK_OVERRUN";
    case GLX_STATE_FIRMWARE_UPDATE:     return "FIRMWARE_UPDATE";
    case GLX_STATE_HARD_FAULT:          return "HARD_FAULT";
    default:                            return "UNKNOWN";
    }
}

int glx_safety_state_transition(glx_safety_controller_t *ctrl,
                                 glx_controller_state_t new_state)
{
    if (!ctrl) return -1;

    glx_controller_state_t old_state = ctrl->state;

    if (!is_valid_transition(old_state, new_state)) {
        return -1;
    }

    /* Use state_name to prevent unused function warning */
    (void)state_name(old_state);
    (void)state_name(new_state);

    ctrl->state = new_state;

    if (new_state == GLX_STATE_SAFETY_FAULT ||
        new_state == GLX_STATE_HARD_FAULT ||
        new_state == GLX_STATE_SAFETY_TASK_OVERRUN) {
        /* De-energize safety outputs to safe state */
        ctrl->safety_output_count = 0;
    }

    if (new_state == GLX_STATE_SAFETY_LOCKED) {
        /* Reset watchdog on entering run mode */
        ctrl->watchdog.last_reset_cycle = 0;
        ctrl->watchdog.overrun_count = 0;
    }

    return 0;
}

int glx_cross_check_update(glx_safety_controller_t *ctrl,
                           const uint8_t *my_data,
                           const uint8_t *partner_data,
                           size_t data_len)
{
    if (!ctrl || !my_data || !partner_data) return -1;
    if (data_len == 0) return -1;

    if (!ctrl->diag_1oo2d.diagnostic_active) return 0;

    int match = 1;
    for (size_t i = 0; i < data_len; i++) {
        if (my_data[i] != partner_data[i]) {
            match = 0;
            break;
        }
    }

    if (match) {
        ctrl->diag_1oo2d.cross_check_passing = 1;
        ctrl->partner_status = GLX_PARTNER_OK;
    } else {
        ctrl->diag_1oo2d.cross_check_passing = 0;
        ctrl->diag_1oo2d.mismatch_count++;

        if (ctrl->diag_1oo2d.mismatch_count > 3) {
            ctrl->partner_status = GLX_PARTNER_MISMATCH;
            /* Multiple consecutive mismatches trigger safety fault */
            glx_safety_state_transition(ctrl, GLX_STATE_SAFETY_FAULT);
        }
    }

    if (ctrl->partner_status == GLX_PARTNER_OFFLINE) {
        /* Partner not responding at all -- diagnostic fault */
        return -1;
    }

    return match ? 0 : -1;
}

int glx_verify_partner_signature(const glx_safety_controller_t *ctrl)
{
    if (!ctrl) return -1;

    if (ctrl->project_sig.signature != ctrl->partner_sig.signature)
        return -1;

    if (ctrl->project_sig.safety_task_checksum !=
        ctrl->partner_sig.safety_task_checksum)
        return -1;

    if (ctrl->project_sig.safety_io_config_checksum !=
        ctrl->partner_sig.safety_io_config_checksum)
        return -1;

    if (ctrl->project_sig.safety_network_number !=
        ctrl->partner_sig.safety_network_number)
        return -1;

    return 0;
}

int glx_compute_proof_test_interval(glx_sil_level_t target_sil,
                                     double lambda_du,
                                     double dc,
                                     double beta,
                                     uint32_t *t_pt_out)
{
    if (!t_pt_out) return -1;
    if (lambda_du <= 0.0 || lambda_du > 1.0) return -1;
    if (dc < 0.0 || dc > 1.0) return -1;
    if (beta < 0.0 || beta > 1.0) return -1;
    if (target_sil == GLX_SIL_NONE || target_sil > GLX_SIL_3)
        return -1;

    double target_pfd;
    switch (target_sil) {
    case GLX_SIL_1: target_pfd = 1.0e-2; break;
    case GLX_SIL_2: target_pfd = 1.0e-3; break;
    case GLX_SIL_3: target_pfd = 1.0e-4; break;
    default:        target_pfd = 1.0e-3; break;
    }

    /* 1oo2D PFDavg = (1-beta)*(1-DC)^2 * lambda_du^2 * T^2 / 3
     *               + beta * lambda_du * T / 2
     *               + lambda_du * (1-DC) * MTTR
     * MTTR = 8 hours (default)
     */
    const double mttr = 8.0;

    /* Check achievability at T=1 hour */
    double pfd_t1 = (1.0 - beta) * (1.0 - dc) * (1.0 - dc)
                    * lambda_du * lambda_du * 1.0 * 1.0 / 3.0
                    + beta * lambda_du * 1.0 / 2.0
                    + lambda_du * (1.0 - dc) * mttr;

    if (pfd_t1 > target_pfd) {
        *t_pt_out = 0;
        return -1;
    }

    /* Binary search for minimum T_pt that achieves target PFDavg */
    uint32_t lo = 1;
    uint32_t hi = 87600; /* 10 years */
    uint32_t mid;

    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        double T = (double)mid;

        double pfd_mid = (1.0 - beta) * (1.0 - dc) * (1.0 - dc)
                         * lambda_du * lambda_du * T * T / 3.0
                         + beta * lambda_du * T / 2.0
                         + lambda_du * (1.0 - dc) * mttr;

        if (pfd_mid <= target_pfd) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    *t_pt_out = lo;
    return 0;
}

int glx_validate_snn(const glx_safety_network_number_t *snn)
{
    if (!snn) return -1;

    if (snn->snn_format > 2) return -1;
    if (snn->snn_value == 0) return -1;

    if (snn->snn_format == 1 && snn->snn_generation_time == 0)
        return -1;

    return 0;
}

uint32_t glx_compute_safety_reaction_time(const glx_safety_controller_t *ctrl,
                                          uint32_t input_delay_ms,
                                          uint32_t network_rpi_ms,
                                          uint32_t output_delay_ms)
{
    if (!ctrl) return 0;

    /* Safety Reaction Time = 2 * Task_Period + Input_Delay
     *                        + Network_RPI + Output_Delay
     */
    uint32_t srt = 2 * ctrl->safety_task_period_ms
                 + input_delay_ms
                 + network_rpi_ms
                 + output_delay_ms;

    return srt;
}
