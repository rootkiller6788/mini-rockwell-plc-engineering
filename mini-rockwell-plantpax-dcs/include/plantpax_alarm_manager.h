/**
 * @file plantpax_alarm_manager.h
 * @brief PlantPAx Alarm Management ? ISA-18.2 Lifecycle, Alarm States, Shelving, Flood Prevention
 *
 * Module: mini-rockwell-plantpax-dcs
 * Knowledge Coverage: L1 Definitions, L2 Alarm Philosophy, L4 ISA-18.2/EEMUA 191, L5 Rationalization
 *
 * Alarm Management in PlantPAx follows ISA-18.2 and EEMUA 191 standards.
 *
 * ISA-18.2 Alarm Management Lifecycle:
 *   A. Philosophy -> B. Identification -> C. Rationalization ->
 *   D. Detailed Design -> E. Implementation -> F. Operation ->
 *   G. Maintenance -> H. Monitoring & Assessment ->
 *   I. Management of Change -> J. Audit
 *
 * Key Alarm KPIs (EEMUA 191):
 *   - Average alarm rate: < 1 per 10 minutes (steady state)
 *   - Peak alarm rate: < 10 per 10 minutes
 *   - Standing alarms: < 10
 *   - Nuisance alarms: < 5%
 *   - Stale alarms: < 5 alarms > 24 hours
 *
 * Reference:
 *   ISA-18.2 Management of Alarm Systems for the Process Industries
 *   EEMUA 191 Alarm Systems: A Guide to Design, Management and Procurement
 * Curriculum: Purdue ME 575, RMIT Aachen, MIT 6.302
 */

#ifndef PLANTPAX_ALARM_MANAGER_H
#define PLANTPAX_ALARM_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "plantpax_architecture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1: Alarm Core Definitions
 * ------------------------------------------------------------------------- */

/** Alarm priority (ISA-18.2 classification) */
typedef enum {
    PAX_ALARM_PRIORITY_DIAGNOSTIC = 0,  /**< Non-process diagnostic */
    PAX_ALARM_PRIORITY_LOW = 1,         /**< Low urgency, logged */
    PAX_ALARM_PRIORITY_MEDIUM = 2,      /**< Medium urgency, operator action */
    PAX_ALARM_PRIORITY_HIGH = 3,        /**< High urgency, immediate action */
    PAX_ALARM_PRIORITY_CRITICAL = 4     /**< Safety/environmental critical */
} pax_alarm_priority_t;

/** Alarm state machine states */
typedef enum {
    PAX_ALARM_STATE_NORMAL = 0,        /**< No alarm condition */
    PAX_ALARM_STATE_ACTIVE = 1,        /**< Alarm condition present */
    PAX_ALARM_STATE_ACKNOWLEDGED = 2,  /**< Acknowledged but not cleared */
    PAX_ALARM_STATE_RETURNED = 3,      /**< Condition cleared, not acknowledged */
    PAX_ALARM_STATE_SHELVED = 4,       /**< Temporarily suppressed */
    PAX_ALARM_STATE_DISABLED = 5,      /**< Permanently suppressed */
    PAX_ALARM_STATE_OUT_OF_SERVICE = 6 /**< Equipment out of service */
} pax_alarm_state_t;

/** Alarm type classification */
typedef enum {
    PAX_ALARM_TYPE_PROCESS = 0,        /**< Process variable limit */
    PAX_ALARM_TYPE_DEVICE = 1,         /**< Device malfunction */
    PAX_ALARM_TYPE_SYSTEM = 2,         /**< System/controller fault */
    PAX_ALARM_TYPE_SAFETY = 3,         /**< Safety-related */
    PAX_ALARM_TYPE_BATCH = 4,          /**< Batch/timing */
    PAX_ALARM_TYPE_COMMUNICATION = 5   /**< Network/communication */
} pax_alarm_type_t;

/** Shelving modes */
typedef enum {
    PAX_SHELVE_NONE = 0,
    PAX_SHELVE_TIMED = 1,              /**< Auto-unshelved after duration */
    PAX_SHELVE_CONDITION = 2,          /**< Unshelved by condition */
    PAX_SHELVE_MANUAL = 3              /**< Requires manual unshelve */
} pax_shelve_mode_t;

/* ---------------------------------------------------------------------------
 * L1: Alarm Data Structures
 * ------------------------------------------------------------------------- */

/** Individual alarm tag definition */
typedef struct {
    uint32_t alarm_id;
    char alarm_tag[64];
    char description[256];
    pax_alarm_type_t type;
    pax_alarm_priority_t priority;
    pax_alarm_state_t state;
    double setpoint;                   /**< Alarm trip point */
    double deadband;                   /**< Hysteresis deadband */
    double on_delay_s;                 /**< Delay before activation */
    double off_delay_s;                /**< Delay before clearing */
    char consequence[256];             /**< Consequence of inaction (ISA-18.2) */
    char operator_action[256];         /**< Required operator response */
    double time_to_respond_s;          /**< Max allowed response time */
    double current_value;              /**< Current PV associated with alarm */
    uint64_t activation_time;          /**< Timestamp when activated */
    uint64_t ack_time;                 /**< Timestamp when acknowledged */
    uint64_t clear_time;               /**< Timestamp when cleared */
    uint64_t shelve_expiry;            /**< Timestamp when shelving expires */
    pax_shelve_mode_t shelve_mode;
    uint32_t activation_count;         /**< Lifetime activation count */
    bool is_standing;                  /**< Standing alarm (active > 24h) */
    bool is_nuisance;                  /**< Flagged as nuisance */
    bool is_chattering;                /**< Rapid on/off cycling */
    uint32_t chatter_count_1h;         /**< Activations in last hour */
    uint32_t suppressed_by_id;         /**< Suppressed by parent alarm */
    double time_in_alarm_hours;        /**< Total time this alarm has been active */
} pax_alarm_t;

/** Alarm class (group of related alarms) */
typedef struct {
    uint32_t class_id;
    char class_name[64];
    uint32_t alarm_ids[64];
    uint32_t num_alarms;
    bool enable_flood_protection;      /**< Auto-suppress during floods */
    uint32_t max_rate_per_10min;       /**< Flood threshold */
    uint32_t rate_counter;
    uint64_t rate_window_start;
} pax_alarm_class_t;

/** Alarm system statistics (EEMUA 191 KPIs) */
typedef struct {
    uint32_t total_alarms_configured;
    uint32_t alarms_active;
    uint32_t alarms_acknowledged;
    uint32_t alarms_shelved;
    uint32_t alarms_disabled;
    uint32_t standing_alarms;
    uint32_t nuisance_alarms;
    uint32_t chattering_alarms;
    double avg_rate_per_10min;
    double peak_rate_per_10min;
    uint32_t activations_24h;
    uint32_t operator_responses_24h;
    double avg_response_time_s;
    double overload_pct;               /**< Time in flood condition */
} pax_alarm_stats_t;

/** Alarm shelving configuration */
typedef struct {
    pax_shelve_mode_t mode;
    double duration_s;                 /**< For timed shelving */
    char reason[256];                  /**< Mandatory shelving reason */
    uint32_t operator_id;              /**< Operator who shelved */
    uint64_t shelve_timestamp;
    uint64_t expiry_timestamp;
} pax_alarm_shelve_config_t;

/* ---------------------------------------------------------------------------
 * L1: ISA-18.2 / EEMUA 191 Constants
 * ------------------------------------------------------------------------- */

#define PAX_ALARM_MAX_ALARMS             2048
#define PAX_ALARM_MAX_CLASSES            64
#define PAX_ALARM_EEMUA_AVG_RATE_MAX     6.0    /**< Per hour (1/10min * 6) */
#define PAX_ALARM_EEMUA_PEAK_RATE_MAX    60.0   /**< Per hour (10/10min * 6) */
#define PAX_ALARM_EEMUA_STANDING_MAX     10
#define PAX_ALARM_CHATTER_THRESHOLD      5      /**< Activations in 1 hour */
#define PAX_ALARM_STALE_THRESHOLD_HOURS  24.0
#define PAX_ALARM_MAX_RESPONSE_TIME_S    600.0  /**< 10 minutes default */
#define PAX_ALARM_SHELVE_MAX_DURATION_S  86400.0 /**< 24 hours max */

/* ---------------------------------------------------------------------------
 * L2/L5: Alarm Management API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize an alarm tag with default values.
 *
 * Sets up an alarm with all required fields including
 * ISA-18.2 consequence of inaction and operator action.
 */
void pax_alarm_init(pax_alarm_t *alarm, uint32_t id,
                     const char *tag, const char *description,
                     pax_alarm_type_t type, pax_alarm_priority_t priority,
                     double setpoint, double deadband,
                     double on_delay_s, double off_delay_s);

/**
 * @brief Evaluate alarm condition from current process value.
 *
 * Checks PV against setpoint with deadband hysteresis:
 *   - Activation: PV exceeds setpoint for on_delay seconds
 *   - Clear: PV returns inside deadband for off_delay seconds
 *
 * @param alarm The alarm to evaluate
 * @param pv Current process value
 * @param elapsed_on_ms Time PV has been exceeding setpoint
 * @param elapsed_off_ms Time PV has been back inside deadband
 * @return New alarm state after evaluation
 */
pax_alarm_state_t pax_alarm_evaluate(pax_alarm_t *alarm, double pv,
                                      double elapsed_on_ms,
                                      double elapsed_off_ms);

/**
 * @brief Acknowledge an active alarm.
 *
 * ISA-18.2: Acknowledgment transitions ACTIVE -> ACKNOWLEDGED.
 * Operator must still wait for condition to clear.
 *
 * @return 0 on success, -1 if alarm is not in ACK-able state
 */
int pax_alarm_acknowledge(pax_alarm_t *alarm);

/**
 * @brief Shelve an alarm (temporarily suppress).
 *
 * Shelving suppresses alarm annunciation for a defined period.
 * ISA-18.2 requires: mandatory reason, automatic unshelving,
 * and operator accountability.
 */
int pax_alarm_shelve(pax_alarm_t *alarm,
                      const pax_alarm_shelve_config_t *config);

/**
 * @brief Un-shelve an alarm.
 *
 * Restores normal alarm behavior. Can be automatic (timed expiry)
 * or manual.
 */
int pax_alarm_unshelve(pax_alarm_t *alarm);

/**
 * @brief Detect chattering alarm condition.
 *
 * Chattering = repeated activation/clearing within a short window.
 * If chatter_count_1h > CHATTER_THRESHOLD, flag as nuisance.
 */
bool pax_alarm_detect_chatter(pax_alarm_t *alarm, double window_hours);

/**
 * @brief Compute EEMUA 191 alarm KPI statistics.
 *
 * Aggregates statistics across all configured alarms.
 */
void pax_alarm_compute_statistics(const pax_alarm_t *alarms,
                                   uint32_t num_alarms,
                                   pax_alarm_stats_t *stats);

/**
 * @brief Implement alarm flood suppression.
 *
 * When alarm rate exceeds threshold, suppress lower-priority alarms
 * to prevent operator overload. Higher priority alarms remain active.
 */
int pax_alarm_flood_suppress(pax_alarm_class_t *cls,
                              pax_alarm_t *alarms,
                              uint32_t num_alarms);

/**
 * @brief Alarm rationalization: verify alarm meets justification criteria.
 *
 * ISA-18.2 rationalization checks:
 *   - Does the alarm indicate a deviation that requires operator action?
 *   - Is the setpoint correct?
 *   - Is the priority appropriate?
 *   - Is the response time achievable?
 *
 * @return 0 = properly rationalized, non-zero = issue found
 */
int pax_alarm_rationalize(const pax_alarm_t *alarm);

/**
 * @brief Convert alarm state to human-readable string.
 */
const char *pax_alarm_state_name(pax_alarm_state_t state);

/**
 * @brief Convert alarm priority to human-readable string.
 */
const char *pax_alarm_priority_name(pax_alarm_priority_t priority);

#ifdef __cplusplus
}
#endif

#endif /* PLANTPAX_ALARM_MANAGER_H */
