/**
 * @file plantpax_alarm_manager.c
 * @brief PlantPAx Alarm Management Implementation (ISA-18.2)
 *
 * Implements alarm state machine, shelving, chattering detection,
 * flood suppression, EEMUA 191 KPIs, and rationalization.
 */
#include "plantpax_alarm_manager.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * L1: Alarm Initialization
 *
 * Sets up an alarm tag with ISA-18.2 required fields:
 *   - Consequence of inaction (what happens if ignored)
 *   - Operator action (what the operator must do)
 *   - Time to respond (maximum allowed response delay)
 * ------------------------------------------------------------------------- */

void pax_alarm_init(pax_alarm_t *alarm, uint32_t id,
                     const char *tag, const char *description,
                     pax_alarm_type_t type, pax_alarm_priority_t priority,
                     double setpoint, double deadband,
                     double on_delay_s, double off_delay_s)
{
    if (alarm == NULL) return;

    memset(alarm, 0, sizeof(pax_alarm_t));

    alarm->alarm_id = id;
    if (tag != NULL) {
        strncpy(alarm->alarm_tag, tag, sizeof(alarm->alarm_tag) - 1);
    }
    if (description != NULL) {
        strncpy(alarm->description, description,
                sizeof(alarm->description) - 1);
    }
    alarm->type = type;
    alarm->priority = priority;
    alarm->state = PAX_ALARM_STATE_NORMAL;
    alarm->setpoint = setpoint;
    alarm->deadband = deadband;
    alarm->on_delay_s = on_delay_s;
    alarm->off_delay_s = off_delay_s;
    alarm->time_to_respond_s = PAX_ALARM_MAX_RESPONSE_TIME_S;
    alarm->shelve_mode = PAX_SHELVE_NONE;
}

/* ---------------------------------------------------------------------------
 * L2: Alarm State Machine Evaluation
 *
 * State transitions:
 *   NORMAL + condition + on_delay  -> ACTIVE
 *   ACTIVE  + acknowledge           -> ACKNOWLEDGED
 *   ACTIVE  + condition_clear       -> RETURNED
 *   ACKNOWLEDGED + condition_clear  -> NORMAL
 *   RETURNED + acknowledge          -> NORMAL
 *
 * Deadband hysteresis:
 *   - Activate when PV exceeds setpoint (above setpoint + deadband/2)
 *   - Clear when PV returns inside deadband (below setpoint - deadband/2)
 *
 * On-delay: condition must persist for on_delay_s before activation
 * Off-delay: condition must be absent for off_delay_s before clearing
 * ------------------------------------------------------------------------- */

pax_alarm_state_t pax_alarm_evaluate(pax_alarm_t *alarm, double pv,
                                      double elapsed_on_ms,
                                      double elapsed_off_ms)
{
    if (alarm == NULL) return PAX_ALARM_STATE_NORMAL;

    double on_delay_ms = alarm->on_delay_s * 1000.0;
    double off_delay_ms = alarm->off_delay_s * 1000.0;
    double half_deadband = alarm->deadband / 2.0;

    /* Determine if alarm condition is currently true */
    bool condition_active = (pv > (alarm->setpoint + half_deadband));
    bool condition_clear = (pv < (alarm->setpoint - half_deadband));

    /* If shelved or out of service, skip evaluation */
    if (alarm->state == PAX_ALARM_STATE_SHELVED ||
        alarm->state == PAX_ALARM_STATE_DISABLED ||
        alarm->state == PAX_ALARM_STATE_OUT_OF_SERVICE) {
        return alarm->state;
    }

    alarm->current_value = pv;

    switch (alarm->state) {
        case PAX_ALARM_STATE_NORMAL:
            if (condition_active && elapsed_on_ms >= on_delay_ms) {
                alarm->state = PAX_ALARM_STATE_ACTIVE;
                alarm->activation_count++;
                alarm->chatter_count_1h++;
            }
            break;

        case PAX_ALARM_STATE_ACTIVE:
            if (condition_clear && elapsed_off_ms >= off_delay_ms) {
                alarm->state = PAX_ALARM_STATE_RETURNED;
            }
            break;

        case PAX_ALARM_STATE_ACKNOWLEDGED:
            if (condition_clear && elapsed_off_ms >= off_delay_ms) {
                alarm->state = PAX_ALARM_STATE_NORMAL;
            }
            break;

        case PAX_ALARM_STATE_RETURNED:
            /* Waiting for acknowledgment */
            /* Re-activation: condition becomes active again */
            if (condition_active && elapsed_on_ms >= on_delay_ms) {
                alarm->state = PAX_ALARM_STATE_ACTIVE;
                alarm->activation_count++;
                alarm->chatter_count_1h++;
            }
            break;

        default:
            break;
    }

    return alarm->state;
}

/* ---------------------------------------------------------------------------
 * L2: Alarm Acknowledgment
 *
 * ISA-18.2: Operator acknowledges alarm, confirming awareness.
 * Acknowledgment does NOT clear the alarm ? the process condition
 * must return to normal for the alarm to clear.
 * ------------------------------------------------------------------------- */

int pax_alarm_acknowledge(pax_alarm_t *alarm)
{
    if (alarm == NULL) return -1;

    if (alarm->state == PAX_ALARM_STATE_ACTIVE) {
        alarm->state = PAX_ALARM_STATE_ACKNOWLEDGED;
        return 0;
    } else if (alarm->state == PAX_ALARM_STATE_RETURNED) {
        alarm->state = PAX_ALARM_STATE_NORMAL;
        return 0;
    }

    return -1;  /* Not in an ACK-able state */
}

/* ---------------------------------------------------------------------------
 * L4: Alarm Shelving (ISA-18.2)
 *
 * Shelving temporarily suppresses alarm annunciation.
 * ISA-18.2 requires:
 *   - Mandatory reason documentation
 *   - Automatic timeout (max 24 hours typically)
 *   - Clear indication that alarm is shelved
 *   - Operator accountability
 * ------------------------------------------------------------------------- */

int pax_alarm_shelve(pax_alarm_t *alarm,
                      const pax_alarm_shelve_config_t *config)
{
    if (alarm == NULL || config == NULL) return -1;

    /* Can only shelve active or acknowledged alarms */
    if (alarm->state != PAX_ALARM_STATE_ACTIVE &&
        alarm->state != PAX_ALARM_STATE_ACKNOWLEDGED) {
        return -2;
    }

    /* Validate shelving duration */
    if (config->mode == PAX_SHELVE_TIMED &&
        config->duration_s > PAX_ALARM_SHELVE_MAX_DURATION_S) {
        return -3;  /* Max duration exceeded */
    }

    alarm->state = PAX_ALARM_STATE_SHELVED;
    alarm->shelve_mode = config->mode;

    if (config->mode == PAX_SHELVE_TIMED) {
        alarm->shelve_expiry = 0;  /* Caller must set actual timestamp */
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * L2: Alarm Un-Shelving
 * ------------------------------------------------------------------------- */

int pax_alarm_unshelve(pax_alarm_t *alarm)
{
    if (alarm == NULL) return -1;

    if (alarm->state != PAX_ALARM_STATE_SHELVED) {
        return -2;
    }

    /* Restore to active (it was shelved from active) */
    alarm->state = PAX_ALARM_STATE_ACTIVE;
    alarm->shelve_mode = PAX_SHELVE_NONE;
    alarm->shelve_expiry = 0;

    return 0;
}

/* ---------------------------------------------------------------------------
 * L5: Chattering Alarm Detection
 *
 * Chattering = repeated activation/clearing cycles in a short period.
 * This indicates:
 *   - Incorrect setpoint / deadband configuration
 *   - Noisy sensor signal
 *   - Process oscillation near alarm limit
 *
 * Detection: if activation count in window_hours > CHATTER_THRESHOLD,
 * the alarm is flagged as a nuisance alarm.
 * ------------------------------------------------------------------------- */

bool pax_alarm_detect_chatter(pax_alarm_t *alarm, double window_hours)
{
    if (alarm == NULL) return false;

    (void)window_hours;  /* Reserved for time-window based detection */

    /* Chatter detection based on 1-hour activation count */
    if (alarm->chatter_count_1h > PAX_ALARM_CHATTER_THRESHOLD) {
        alarm->is_chattering = true;
        alarm->is_nuisance = true;
        return true;
    }

    /* Check standing alarm (> 24 hours continuously active) */
    if (alarm->time_in_alarm_hours > PAX_ALARM_STALE_THRESHOLD_HOURS) {
        alarm->is_standing = true;
    }

    return alarm->is_chattering;
}

/* ---------------------------------------------------------------------------
 * L5: EEMUA 191 Alarm KPI Statistics
 *
 * Key Performance Indicators per EEMUA 191:
 *   Average alarm rate: activations per 10-minute period
 *   Peak alarm rate: maximum activations in any 10-minute window
 *   Standing alarms: alarms continuously active > 24 hours
 *   Nuisance alarms: flagged as chattering or false
 * ------------------------------------------------------------------------- */

void pax_alarm_compute_statistics(const pax_alarm_t *alarms,
                                   uint32_t num_alarms,
                                   pax_alarm_stats_t *stats)
{
    if (alarms == NULL || stats == NULL) return;

    memset(stats, 0, sizeof(pax_alarm_stats_t));

    uint32_t i;
    uint32_t total_activations = 0;

    stats->total_alarms_configured = num_alarms;

    for (i = 0; i < num_alarms; i++) {
        const pax_alarm_t *a = &alarms[i];

        switch (a->state) {
            case PAX_ALARM_STATE_ACTIVE:
                stats->alarms_active++;
                break;
            case PAX_ALARM_STATE_ACKNOWLEDGED:
                stats->alarms_acknowledged++;
                break;
            case PAX_ALARM_STATE_SHELVED:
                stats->alarms_shelved++;
                break;
            case PAX_ALARM_STATE_DISABLED:
                stats->alarms_disabled++;
                break;
            default:
                break;
        }

        if (a->is_standing) stats->standing_alarms++;
        if (a->is_nuisance) stats->nuisance_alarms++;
        if (a->is_chattering) stats->chattering_alarms++;

        total_activations += a->activation_count;
    }

    /* Average alarm rate: assume tracking over 24 hours */
    /* activations per 10 min = total / (24h * 6 periods/h) */
    stats->activations_24h = total_activations;
    stats->avg_rate_per_10min = (double)total_activations / 144.0;

    /* Check if in flood condition */
    if (stats->avg_rate_per_10min > PAX_ALARM_EEMUA_AVG_RATE_MAX) {
        stats->overload_pct = 100.0;
    }
}

/* ---------------------------------------------------------------------------
 * L5: Alarm Flood Suppression
 *
 * During an alarm flood (high rate of alarms), lower-priority
 * alarms are automatically suppressed to prevent operator overload.
 * Critical and High priority alarms remain active.
 *
 * This implements ISA-18.2 dynamic alarm suppression.
 * ------------------------------------------------------------------------- */

int pax_alarm_flood_suppress(pax_alarm_class_t *cls,
                              pax_alarm_t *alarms,
                              uint32_t num_alarms)
{
    if (cls == NULL || alarms == NULL) return -1;

    /* Check if suppression threshold is exceeded */
    if (cls->rate_counter < cls->max_rate_per_10min) {
        return 0;  /* Not in flood */
    }

    uint32_t i;
    uint32_t suppressed = 0;

    for (i = 0; i < num_alarms; i++) {
        /* Only suppress lower-priority alarms (Low, Diagnostic) */
        if (alarms[i].priority <= PAX_ALARM_PRIORITY_LOW) {
            /* Check if alarm belongs to this class */
            uint32_t j;
            bool in_class = false;
            for (j = 0; j < cls->num_alarms; j++) {
                if (cls->alarm_ids[j] == alarms[i].alarm_id) {
                    in_class = true;
                    break;
                }
            }

            if (in_class && alarms[i].state == PAX_ALARM_STATE_ACTIVE) {
                alarms[i].state = PAX_ALARM_STATE_SHELVED;
                alarms[i].shelve_mode = PAX_SHELVE_CONDITION;
                alarms[i].suppressed_by_id = cls->class_id;
                suppressed++;
            }
        }
    }

    return (int)suppressed;
}

/* ---------------------------------------------------------------------------
 * L4: Alarm Rationalization (ISA-18.2 Stage C)
 *
 * Each alarm must be justified through rationalization:
 *   1. Does the alarm indicate a process deviation requiring action?
 *   2. Is the setpoint justified by process limits?
 *   3. Is the priority appropriate for the consequence?
 *   4. Is the response time achievable by a human operator?
 *
 * Returns 0 if alarm passes rationalization check.
 * ------------------------------------------------------------------------- */

int pax_alarm_rationalize(const pax_alarm_t *alarm)
{
    if (alarm == NULL) return -1;

    int issues = 0;

    /* Check 1: Consequence of inaction must be documented */
    if (strlen(alarm->consequence) == 0) {
        issues |= 0x01;
    }

    /* Check 2: Operator action must be documented */
    if (strlen(alarm->operator_action) == 0) {
        issues |= 0x02;
    }

    /* Check 3: Time to respond must be reasonable */
    if (alarm->time_to_respond_s <= 0.0 ||
        alarm->time_to_respond_s > 3600.0) {
        issues |= 0x04;
    }

    /* Check 4: Setpoint must be within reasonable bounds */
    if (alarm->setpoint <= -1e9 || alarm->setpoint >= 1e9) {
        issues |= 0x08;
    }

    /* Check 5: Deadband must prevent chattering */
    if (alarm->deadband <= 0.0) {
        issues |= 0x10;
    }

    /* Check 6: Priority must match consequences */
    if (alarm->priority >= PAX_ALARM_PRIORITY_CRITICAL &&
        alarm->time_to_respond_s > 300.0) {
        /* Critical alarm should have response time <= 5 minutes */
        issues |= 0x20;
    }

    return issues;
}

/* ---------------------------------------------------------------------------
 * L1: Name Conversions
 * ------------------------------------------------------------------------- */

const char *pax_alarm_state_name(pax_alarm_state_t state)
{
    static const char *names[] = {
        "NORMAL", "ACTIVE", "ACKNOWLEDGED", "RETURNED",
        "SHELVED", "DISABLED", "OUT_OF_SERVICE"
    };
    if (state >= 0 && state <= PAX_ALARM_STATE_OUT_OF_SERVICE) {
        return names[state];
    }
    return "UNKNOWN";
}

const char *pax_alarm_priority_name(pax_alarm_priority_t priority)
{
    static const char *names[] = {
        "DIAGNOSTIC", "LOW", "MEDIUM", "HIGH", "CRITICAL"
    };
    if (priority >= 0 && priority <= PAX_ALARM_PRIORITY_CRITICAL) {
        return names[priority];
    }
    return "UNKNOWN";
}
