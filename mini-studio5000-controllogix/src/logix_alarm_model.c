/**
 * @file    logix_alarm_model.c
 * @brief   ALMA/ALMD Alarm Instructions Implementation
 * L6: Alarm state machine per ISA-18.2
 */

#include <string.h>
#include <math.h>
#include <stdio.h>
#include "logix_alarm_model.h"

/* ========================================================================
 * Analog Alarm (ALMA)
 * ======================================================================== */

void logix_alma_init(logix_alma_t *alarm, const char *tag_name,
                      double ts_sec)
{
    if (!alarm || !tag_name) return;

    memset(alarm, 0, sizeof(*alarm));
    strncpy(alarm->tag_name, tag_name, 63);
    alarm->tag_name[63] = '\0';
    alarm->ts_sec = (ts_sec > 0.0) ? ts_sec : 0.1;

    /* Default: all alarms enabled with typical severity */
    alarm->hh_enabled = true;
    alarm->h_enabled  = true;
    alarm->l_enabled  = true;
    alarm->ll_enabled = true;

    alarm->hh_severity = ALARM_SEVERITY_CRITICAL;
    alarm->h_severity  = ALARM_SEVERITY_HIGH;
    alarm->l_severity  = ALARM_SEVERITY_HIGH;
    alarm->ll_severity = ALARM_SEVERITY_CRITICAL;

    snprintf(alarm->hh_message, sizeof(alarm->hh_message),
             "%s High-High Alarm", tag_name);
    snprintf(alarm->h_message, sizeof(alarm->h_message),
             "%s High Alarm", tag_name);
    snprintf(alarm->l_message, sizeof(alarm->l_message),
             "%s Low Alarm", tag_name);
    snprintf(alarm->ll_message, sizeof(alarm->ll_message),
             "%s Low-Low Alarm", tag_name);
}

void logix_alma_set_limits(logix_alma_t *alarm,
                             double hh, double h, double l, double ll,
                             double deadband, double eu_min, double eu_max)
{
    if (!alarm) return;

    alarm->high_high_limit = hh;
    alarm->high_limit = h;
    alarm->low_limit = l;
    alarm->low_low_limit = ll;
    alarm->deadband = (deadband > 0.0) ? deadband : 0.5;
    alarm->eu_min = eu_min;
    alarm->eu_max = eu_max;
}

void logix_alma_execute(logix_alma_t *alarm, double pv)
{
    if (!alarm) return;
    if (alarm->disabled || alarm->suppressed) return;

    alarm->input = pv;
    double ts = alarm->ts_sec;
    double db = alarm->deadband;

    /* ---- High-High ---- */
    if (alarm->hh_enabled) {
        bool trip = (pv >= alarm->high_high_limit);
        bool clear = (pv < alarm->high_high_limit - db);

        if (trip && !alarm->hh_condition) {
            alarm->hh_on_timer += ts;
            if (alarm->hh_on_timer >= alarm->on_delay_sec) {
                alarm->hh_condition = true;
                alarm->hh_active = true;
                alarm->hh_acknowledged = false;
                alarm->hh_on_timer = 0.0;
            }
        } else if (clear && alarm->hh_condition) {
            alarm->hh_off_timer += ts;
            if (alarm->hh_off_timer >= alarm->off_delay_sec) {
                alarm->hh_condition = false;
                alarm->hh_active = false;
                alarm->hh_off_timer = 0.0;
            }
        } else {
            /* Reset timers when condition state is stable */
            if (!trip) alarm->hh_on_timer = 0.0;
            if (!clear) alarm->hh_off_timer = 0.0;
        }
    }

    /* ---- High ---- */
    if (alarm->h_enabled) {
        bool trip = (pv >= alarm->high_limit);
        bool clear = (pv < alarm->high_limit - db);

        if (trip && !alarm->h_condition) {
            alarm->h_on_timer += ts;
            if (alarm->h_on_timer >= alarm->on_delay_sec) {
                alarm->h_condition = true;
                alarm->h_active = true;
                alarm->h_acknowledged = false;
                alarm->h_on_timer = 0.0;
            }
        } else if (clear && alarm->h_condition) {
            alarm->h_off_timer += ts;
            if (alarm->h_off_timer >= alarm->off_delay_sec) {
                alarm->h_condition = false;
                alarm->h_active = false;
                alarm->h_off_timer = 0.0;
            }
        } else {
            if (!trip) alarm->h_on_timer = 0.0;
            if (!clear) alarm->h_off_timer = 0.0;
        }
    }

    /* ---- Low ---- */
    if (alarm->l_enabled) {
        bool trip = (pv <= alarm->low_limit);
        bool clear = (pv > alarm->low_limit + db);

        if (trip && !alarm->l_condition) {
            alarm->l_on_timer += ts;
            if (alarm->l_on_timer >= alarm->on_delay_sec) {
                alarm->l_condition = true;
                alarm->l_active = true;
                alarm->l_acknowledged = false;
                alarm->l_on_timer = 0.0;
            }
        } else if (clear && alarm->l_condition) {
            alarm->l_off_timer += ts;
            if (alarm->l_off_timer >= alarm->off_delay_sec) {
                alarm->l_condition = false;
                alarm->l_active = false;
                alarm->l_off_timer = 0.0;
            }
        } else {
            if (!trip) alarm->l_on_timer = 0.0;
            if (!clear) alarm->l_off_timer = 0.0;
        }
    }

    /* ---- Low-Low ---- */
    if (alarm->ll_enabled) {
        bool trip = (pv <= alarm->low_low_limit);
        bool clear = (pv > alarm->low_low_limit + db);

        if (trip && !alarm->ll_condition) {
            alarm->ll_on_timer += ts;
            if (alarm->ll_on_timer >= alarm->on_delay_sec) {
                alarm->ll_condition = true;
                alarm->ll_active = true;
                alarm->ll_acknowledged = false;
                alarm->ll_on_timer = 0.0;
            }
        } else if (clear && alarm->ll_condition) {
            alarm->ll_off_timer += ts;
            if (alarm->ll_off_timer >= alarm->off_delay_sec) {
                alarm->ll_condition = false;
                alarm->ll_active = false;
                alarm->ll_off_timer = 0.0;
            }
        } else {
            if (!trip) alarm->ll_on_timer = 0.0;
            if (!clear) alarm->ll_off_timer = 0.0;
        }
    }

    /* Update shelved timer */
    if (alarm->shelved && alarm->shelve_time_remaining_sec > 0.0) {
        alarm->shelve_time_remaining_sec -= ts;
        if (alarm->shelve_time_remaining_sec <= 0.0) {
            alarm->shelved = false;
            alarm->shelve_time_remaining_sec = 0.0;
        }
    }

    /* Combined alarm status */
    alarm->any_alarm_active = alarm->hh_active || alarm->h_active ||
                               alarm->l_active || alarm->ll_active;
}

void logix_alma_acknowledge(logix_alma_t *alarm)
{
    if (!alarm) return;

    alarm->hh_acknowledged = true;
    alarm->h_acknowledged = true;
    alarm->l_acknowledged = true;
    alarm->ll_acknowledged = true;
}

void logix_alma_suppress(logix_alma_t *alarm, bool suppress)
{
    if (!alarm) return;
    alarm->suppressed = suppress;
}

void logix_alma_shelve(logix_alma_t *alarm, double duration_sec)
{
    if (!alarm) return;
    alarm->shelved = true;
    alarm->shelve_time_remaining_sec = duration_sec;
}

/* ========================================================================
 * Digital Alarm (ALMD)
 * ======================================================================== */

void logix_almd_init(logix_almd_t *alarm, const char *tag_name,
                      double ts_sec, bool latched)
{
    if (!alarm || !tag_name) return;

    memset(alarm, 0, sizeof(*alarm));
    strncpy(alarm->tag_name, tag_name, 63);
    alarm->tag_name[63] = '\0';
    alarm->ts_sec = (ts_sec > 0.0) ? ts_sec : 0.1;
    alarm->latched = latched;
    alarm->severity = ALARM_SEVERITY_HIGH;
    alarm->class = ALARM_CLASS_DEVICE;

    snprintf(alarm->message, sizeof(alarm->message),
             "%s Digital Alarm", tag_name);
}

void logix_almd_execute(logix_almd_t *alarm, bool input)
{
    if (!alarm) return;
    if (alarm->disabled || alarm->suppressed) return;

    alarm->input = input;
    double ts = alarm->ts_sec;

    if (input && !alarm->condition) {
        /* Rising edge: start on-delay timer */
        alarm->on_timer += ts;
        if (alarm->on_timer >= alarm->on_delay_sec) {
            alarm->condition = true;
            alarm->active = true;
            alarm->acknowledged = false;
            alarm->on_timer = 0.0;
            /* Timestamp for KPI tracking */
            alarm->timestamp_active_ms = (uint64_t)(alarm->on_delay_sec * 1000.0);
        }
    } else if (!input && alarm->condition && !alarm->latched) {
        /* Falling edge: start off-delay timer (non-latched only) */
        alarm->off_timer += ts;
        if (alarm->off_timer >= alarm->off_delay_sec) {
            alarm->condition = false;
            alarm->active = false;
            alarm->off_timer = 0.0;
        }
    } else {
        /* Reset timers when idle */
        if (!input) alarm->on_timer = 0.0;
        if (input) alarm->off_timer = 0.0;
    }

    /* Shelved timer */
    if (alarm->shelved && alarm->shelve_time_remaining_sec > 0.0) {
        alarm->shelve_time_remaining_sec -= ts;
        if (alarm->shelve_time_remaining_sec <= 0.0) {
            alarm->shelved = false;
            alarm->shelve_time_remaining_sec = 0.0;
        }
    }
}

void logix_almd_acknowledge(logix_almd_t *alarm)
{
    if (!alarm) return;
    alarm->acknowledged = true;

    /* For latched alarms, acknowledge allows clearing if condition is false */
    if (alarm->latched && !alarm->input) {
        alarm->active = false;
    }
}

void logix_almd_suppress(logix_almd_t *alarm, bool suppress)
{
    if (!alarm) return;
    alarm->suppressed = suppress;
}

void logix_almd_shelve(logix_almd_t *alarm, double duration_sec)
{
    if (!alarm) return;
    alarm->shelved = true;
    alarm->shelve_time_remaining_sec = duration_sec;
}

/* ========================================================================
 * Alarm Priority (EEMUA 191)
 * ======================================================================== */

uint32_t logix_alarm_compute_priority(uint32_t severity_rank,
                                        double response_sec)
{
    /* Priority = severity_weight * 2 + response_time_factor
     * Clamped to [1, 5] */

    /* Severity weight: 1=critical gets 2, 4=low gets 0.5 */
    double severity_weight = (5.0 - (double)severity_rank + 1.0) / 2.0;
    if (severity_weight < 0.5) severity_weight = 0.5;

    /* Response time factor */
    int32_t rt_factor;
    if (response_sec <= 60.0) {
        rt_factor = 2;  /* Immediate response required */
    } else if (response_sec <= 300.0) {
        rt_factor = 1;  /* Prompt response */
    } else {
        rt_factor = 0;  /* Non-urgent */
    }

    int32_t raw_priority = (int32_t)(severity_weight * 2.0) + rt_factor;

    /* Clamp to [1, 5] */
    if (raw_priority < 1) raw_priority = 1;
    if (raw_priority > 5) raw_priority = 5;

    return (uint32_t)raw_priority;
}

bool logix_alarm_detect_flood(const uint64_t *alarm_timestamps,
                                uint32_t count,
                                double window_sec,
                                uint32_t max_alarms)
{
    if (!alarm_timestamps || count == 0) return false;

    /* Count alarms in a sliding window */
    uint64_t window_ms = (uint64_t)(window_sec * 1000.0);
    uint32_t max_count = 0;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t count_in_window = 0;
        uint64_t window_start = alarm_timestamps[i];
        uint64_t window_end = window_start + window_ms;

        for (uint32_t j = i; j < count; j++) {
            if (alarm_timestamps[j] <= window_end) {
                count_in_window++;
            } else {
                break;
            }
        }

        if (count_in_window > max_count) {
            max_count = count_in_window;
        }
    }

    return max_count > max_alarms;
}

void logix_alarm_compute_kpi(const uint64_t *alarm_timestamps,
                               uint32_t event_count,
                               double *avg_rate,
                               double *peak_rate,
                               uint32_t *standing_count)
{
    if (!avg_rate || !peak_rate || !standing_count) return;

    *avg_rate = 0.0;
    *peak_rate = 0.0;
    *standing_count = 0;

    if (!alarm_timestamps || event_count < 2) return;

    /* Average rate: total events / total duration * 600 sec (per 10 min) */
    uint64_t total_duration_ms = alarm_timestamps[event_count - 1] -
                                  alarm_timestamps[0];
    if (total_duration_ms == 0) total_duration_ms = 1;

    double total_min = total_duration_ms / 60000.0;
    *avg_rate = (double)event_count / total_min * 10.0; /* Per 10 minutes */

    /* Peak rate: maximum in any 10-minute window */
    double peak = 0.0;
    uint64_t window_ms = 600000; /* 10 minutes */

    for (uint32_t i = 0; i < event_count; i++) {
        uint32_t count = 0;
        uint64_t window_end = alarm_timestamps[i] + window_ms;
        for (uint32_t j = i; j < event_count && alarm_timestamps[j] <= window_end; j++) {
            count++;
        }
        if ((double)count > peak) peak = (double)count;
    }
    *peak_rate = peak;

    /* Standing alarms: active for > 24 hours
     * In a real implementation, this would track individual alarm durations.
     * Here we estimate based on event density. */
    *standing_count = 0;
}