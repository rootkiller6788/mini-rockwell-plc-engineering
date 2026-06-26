/**
 * @file ftview_alarm.h
 * @brief FactoryTalk View HMI -- Alarm Management (L1/L2/L3/L4/L5)
 *
 * Knowledge points:
 *   L1.8  -- Alarm definition: condition, severity, message, group
 *   L1.9  -- Alarm severity levels (per ISA-18.2 / EEMUA 191)
 *   L2.4  -- ISA-18.2 alarm state machine: NORMAL -> ALARM -> ACK -> RTN-UNACK -> NORMAL
 *   L2.5  -- Alarm suppression: shelving, out-of-service, designed suppression
 *   L3.3  -- Alarm FIFO queue with priority-based insertion
 *   L3.4  -- Alarm log ring buffer with timestamp indexing
 *   L4.2  -- ISA-18.2-2016 alarm management lifecycle (philosophy through audit)
 *   L4.3  -- EEMUA 191 alarm performance metrics (avg alarms/hr, peak alarms/10min)
 *   L5.3  -- Alarm deadband / hysteresis to prevent chattering
 *   L5.4  -- On-delay / off-delay alarm conditioning timers
 *
 * Reference: ISA-18.2-2016 Management of Alarm Systems for the Process Industries
 *            EEMUA Publication 191 Alarm Systems -- A Guide to Design, Management and Procurement
 */

#ifndef FTVIEW_ALARM_H
#define FTVIEW_ALARM_H

#include "ftview_common.h"
#include "ftview_tag.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define FTVIEW_ALARM_MAX_COUNT        64
#define FTVIEW_ALARM_MSG_MAX_LEN     256
#define FTVIEW_ALARM_GROUP_MAX_LEN    64
#define FTVIEW_ALARM_QUEUE_SIZE      256
#define FTVIEW_ALARM_LOG_SIZE        512

/* =====================================================================
 * L1.9 -- Alarm Severity Levels (ISA-18.2 / EEMUA 191)
 * ===================================================================== */

typedef enum {
    FTVIEW_ALARM_SEV_DIAGNOSTIC = 0,  /**< I/O diagnostic, journal only */
    FTVIEW_ALARM_SEV_LO         = 1,  /**< Low severity advisory */
    FTVIEW_ALARM_SEV_MEDIUM     = 2,  /**< Medium severity warning */
    FTVIEW_ALARM_SEV_HIGH       = 3,  /**< High severity alarm */
    FTVIEW_ALARM_SEV_CRITICAL   = 4   /**< Emergency / safety-critical */
} ftview_alarm_severity_t;

/* =====================================================================
 * L2.4 -- ISA-18.2 Alarm State Machine
 *
 * States: NORMAL -> UNACK_ALM -> ACK_ALM -> UNACK_RTN -> NORMAL
 * Shelved and Out-of-Service are orthogonal suppression states.
 * ===================================================================== */

typedef enum {
    FTVIEW_ALARM_STATE_NORMAL      = 0,  /**< No active condition */
    FTVIEW_ALARM_STATE_UNACK_ALM   = 1,  /**< In alarm, unacknowledged */
    FTVIEW_ALARM_STATE_ACK_ALM     = 2,  /**< In alarm, acknowledged */
    FTVIEW_ALARM_STATE_UNACK_RTN   = 3,  /**< Returned-to-normal, unacknowledged */
    FTVIEW_ALARM_STATE_SHELVED     = 4,  /**< Temporarily suppressed */
    FTVIEW_ALARM_STATE_OO_SERVICE  = 5   /**< Out of service / maintenance */
} ftview_alarm_state_t;

/* =====================================================================
 * Alarm condition types
 * ===================================================================== */

typedef enum {
    FTVIEW_ALARM_COND_HIHI    = 0,  /**< High-high limit exceeded */
    FTVIEW_ALARM_COND_HI      = 1,  /**< High limit exceeded */
    FTVIEW_ALARM_COND_LO      = 2,  /**< Low limit exceeded */
    FTVIEW_ALARM_COND_LOLO    = 3,  /**< Low-low limit exceeded */
    FTVIEW_ALARM_COND_DEV     = 4,  /**< Deviation from setpoint */
    FTVIEW_ALARM_COND_ROC     = 5,  /**< Rate-of-change */
    FTVIEW_ALARM_COND_DIGITAL = 6,  /**< Digital / state alarm */
    FTVIEW_ALARM_COND_BAD_QUAL= 7,  /**< Bad quality / comm failure */
    FTVIEW_ALARM_COND_CMD_FAIL= 8   /**< Command failure */
} ftview_alarm_condition_t;

/* =====================================================================
 * L1.8 -- Alarm Definition
 * ===================================================================== */

typedef struct {
    uint32_t              id;
    char                  tag_name[FTVIEW_TAG_NAME_MAX_LEN];
    char                  message[FTVIEW_ALARM_MSG_MAX_LEN];
    char                  group[FTVIEW_ALARM_GROUP_MAX_LEN];
    ftview_alarm_severity_t severity;
    ftview_alarm_condition_t condition;
    double                threshold;       /**< trip point (HI, LO, etc.) */
    double                threshold2;      /**< secondary threshold (HIHI, LOLO) */
    double                deadband;        /**< L5.3 -- hysteresis band */
    uint32_t              on_delay_ms;     /**< L5.4 -- condition must persist */
    uint32_t              off_delay_ms;    /**< L5.4 -- return delay before clear */
    bool                  enabled;
    uint32_t              shelf_timeout_s; /**< auto-unshelve after N seconds */
} ftview_alarm_def_t;

/* =====================================================================
 * L2.4 -- Live Alarm Instance (run-time state)
 * ===================================================================== */

typedef struct {
    uint32_t              alarm_def_id;    /**< back-reference to definition */
    ftview_alarm_state_t  state;
    ftview_value_t        trip_value;      /**< value that triggered */
    double                current_value;   /**< live PV */
    int64_t               active_time_ms;  /**< when alarm became active */
    int64_t               ack_time_ms;     /**< when acknowledged (0 if not) */
    int64_t               rtn_time_ms;     /**< when returned to normal (0 if not) */
    int64_t               shelf_expiry_ms; /**< when shelving expires (0 if not) */
    bool                  ack_required;    /**< ISA-18.2: unack alarms must be acked */
    char                  ack_user[64];    /**< who acknowledged */
    char                  ack_comment[128];
} ftview_alarm_instance_t;

/* =====================================================================
 * L3.3 -- Alarm Queue (priority-sorted, bounded size)
 * ===================================================================== */

/** A queued alarm event for operator attention */
typedef struct {
    uint32_t              alarm_id;
    int64_t               timestamp_ms;
    ftview_alarm_state_t  transition_to;   /**< state we transitioned TO */
    ftview_alarm_severity_t severity;
    char                  tag_name[FTVIEW_TAG_NAME_MAX_LEN];
    char                  message[FTVIEW_ALARM_MSG_MAX_LEN];
} ftview_alarm_queue_entry_t;

/* =====================================================================
 * L3.4 -- Alarm Log Entry (persistent, time-indexed)
 * ===================================================================== */

typedef struct {
    uint64_t              sequence;        /**< monotonically increasing */
    int64_t               timestamp_ms;
    uint32_t              alarm_id;
    ftview_alarm_state_t  state;
    double                value;
    char                  tag_name[FTVIEW_TAG_NAME_MAX_LEN];
    char                  message[FTVIEW_ALARM_MSG_MAX_LEN];
    char                  ack_user[64];
} ftview_alarm_log_entry_t;

/* =====================================================================
 * Alarm Manager (top-level container)
 * ===================================================================== */

typedef struct {
    ftview_alarm_def_t      defs[FTVIEW_ALARM_MAX_COUNT];
    uint32_t                def_count;
    ftview_alarm_instance_t instances[FTVIEW_ALARM_MAX_COUNT];
    uint32_t                instance_count;
    ftview_alarm_queue_entry_t queue[FTVIEW_ALARM_QUEUE_SIZE];
    uint32_t                queue_head;
    uint32_t                queue_tail;
    uint32_t                queue_count;
    ftview_alarm_log_entry_t  log[FTVIEW_ALARM_LOG_SIZE];
    uint64_t                log_sequence;
    uint32_t                log_write_idx;
    uint32_t                log_count;
} ftview_alarm_mgr_t;

/* ───────── API ───────── */

void ftview_alarm_mgr_init(ftview_alarm_mgr_t *mgr);

/** Add alarm definition. Returns FTVIEW_ERR_DUPLICATE_TAG if tag already alarmed. */
ftview_err_t ftview_alarm_add_def(ftview_alarm_mgr_t *mgr,
                                   const ftview_alarm_def_t *def);

/** L2.4 -- Evaluate all alarm conditions against current tag values.
 *  This implements the core ISA-18.2 state machine transitions.
 *  Returns number of state changes detected. */
uint32_t ftview_alarm_evaluate(ftview_alarm_mgr_t *mgr,
                                const ftview_tagdb_t *tagdb);

/** Acknowledge an active alarm (operator action).
 *  ISA-18.2: transitions UNACK_ALM -> ACK_ALM or UNACK_RTN -> NORMAL. */
ftview_err_t ftview_alarm_acknowledge(ftview_alarm_mgr_t *mgr,
                                       uint32_t alarm_id,
                                       const char *user,
                                       const char *comment);

/** L2.5 -- Shelve an alarm for a duration (seconds). 0 = indefinite. */
ftview_err_t ftview_alarm_shelve(ftview_alarm_mgr_t *mgr,
                                  uint32_t alarm_id,
                                  uint32_t duration_s);

/** L2.5 -- Unshelve alarm, restoring normal evaluation. */
ftview_err_t ftview_alarm_unshelve(ftview_alarm_mgr_t *mgr,
                                    uint32_t alarm_id);

/** Place alarm out of service (maintenance suppression). */
ftview_err_t ftview_alarm_out_of_service(ftview_alarm_mgr_t *mgr,
                                          uint32_t alarm_id);

/** Return alarm to service. */
ftview_err_t ftview_alarm_in_service(ftview_alarm_mgr_t *mgr,
                                      uint32_t alarm_id);

/** L5.3 -- Compute deadband-adjusted threshold crossing check.
 *  Returns true if value crosses threshold considering hysteresis.
 *  For HI alarm:  (value > thresh) and (value < thresh - deadband on falling)
 *  For LO alarm:  (value < thresh) and (value > thresh + deadband on rising) */
bool ftview_alarm_deadband_check(double value, double threshold,
                                  double deadband, bool is_high_alarm,
                                  bool was_in_alarm);

/** L5.4 -- On-delay timer: returns true once condition is continuously true
 *  for on_delay_ms. Internal state tracked via *timer_acc_ms. */
bool ftview_alarm_on_delay_timer(bool condition, uint32_t on_delay_ms,
                                  int64_t now_ms, int64_t *timer_acc_ms);

/** L5.4 -- Off-delay timer: returns true once condition is continuously false
 *  for off_delay_ms (used for return-to-normal delay). */
bool ftview_alarm_off_delay_timer(bool condition, uint32_t off_delay_ms,
                                   int64_t now_ms, int64_t *timer_acc_ms);

/** L4.3 -- Compute alarm rate metrics (alarms per hour over window_ms) */
double ftview_alarm_rate_per_hour(const ftview_alarm_mgr_t *mgr,
                                   int64_t window_ms, int64_t now_ms);

/** Query the alarm queue (returns entries and count). Caller frees. */
uint32_t ftview_alarm_queue_peek(const ftview_alarm_mgr_t *mgr,
                                  ftview_alarm_queue_entry_t *out,
                                  uint32_t max_out);

/** Pop one entry from the alarm queue. Returns false if empty. */
bool ftview_alarm_queue_pop(ftview_alarm_mgr_t *mgr,
                              ftview_alarm_queue_entry_t *out);

/** Get alarm log entries within a time range. */
uint32_t ftview_alarm_log_query(const ftview_alarm_mgr_t *mgr,
                                 int64_t start_ms, int64_t end_ms,
                                 ftview_alarm_log_entry_t *out,
                                 uint32_t max_out);

#endif /* FTVIEW_ALARM_H */
