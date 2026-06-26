/**
 * @file ftview_security.h
 * @brief FactoryTalk View HMI -- Security & User Management (L1/L2/L4)
 *
 * Knowledge points:
 *   L1.10 -- User account definition: username, role, access level
 *   L1.11 -- Security privilege codes (A-Z per FactoryTalk Security model)
 *   L2.6  -- Role-Based Access Control (RBAC): user -> role -> privileges
 *   L2.7  -- Authentication flow: login, session, timeout, logout
 *   L3.5  -- PBKDF2-HMAC-SHA256 password hashing (NIST SP 800-132)
 *   L3.6  -- Audit trail: event type, user, timestamp, detail
 *   L4.4  -- ISA-62443-3-3 SR 1.1: user identification and authentication
 *   L4.5  -- ISA-62443-3-3 SR 2.1: authorization enforcement (RBAC)
 *   L4.6  -- 21 CFR Part 11: electronic signatures, audit trails
 *
 * Reference: ISA-62443-3-3 System Security Requirements
 *            NIST SP 800-132 PBKDF2
 *            21 CFR Part 11 Electronic Records; Electronic Signatures
 */

#ifndef FTVIEW_SECURITY_H
#define FTVIEW_SECURITY_H

#include "ftview_common.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#define FTVIEW_SEC_MAX_USERS      128
#define FTVIEW_SEC_MAX_ROLES       32
#define FTVIEW_SEC_USERNAME_LEN    64
#define FTVIEW_SEC_PASSHASH_LEN    64
#define FTVIEW_SEC_SALT_LEN        16
#define FTVIEW_SEC_AUDIT_MAX      256
#define FTVIEW_SEC_SESSION_TIMEOUT_MS (15 * 60 * 1000)  /**< 15 minutes */

/* =====================================================================
 * L1.10 -- Security Privilege Codes
 *
 * FactoryTalk Security uses letter-coded privileges.
 * Each letter maps to a bit position in a 32-bit mask.
 * ===================================================================== */

typedef enum {
    FTVIEW_PRIV_NONE           = 0,
    FTVIEW_PRIV_VIEW           = (1U << 0),   /**< A: view displays */
    FTVIEW_PRIV_ACK_ALARMS     = (1U << 1),   /**< B: acknowledge alarms */
    FTVIEW_PRIV_CHANGE_SP      = (1U << 2),   /**< C: change setpoints */
    FTVIEW_PRIV_START_STOP     = (1U << 3),   /**< D: start/stop equipment */
    FTVIEW_PRIV_MANUAL_MODE    = (1U << 4),   /**< E: manual mode override */
    FTVIEW_PRIV_EDIT_RECIPE    = (1U << 5),   /**< F: modify recipes */
    FTVIEW_PRIV_CONFIGURE      = (1U << 6),   /**< G: system configuration */
    FTVIEW_PRIV_ADMIN          = (1U << 7),   /**< H: user administration */
    FTVIEW_PRIV_ESIGN          = (1U << 8),   /**< I: electronic signature */
    FTVIEW_PRIV_SHELVE_ALARM   = (1U << 9),   /**< J: shelve alarms */
    FTVIEW_PRIV_OO_SERVICE     = (1U << 10),  /**< K: place out of service */
    FTVIEW_PRIV_DOWNLOAD       = (1U << 11),  /**< L: download to PLC */
    FTVIEW_PRIV_VIEW_AUDIT     = (1U << 12),  /**< M: view audit trail */
    FTVIEW_PRIV_OVERRIDE_ILOCK = (1U << 13),  /**< N: override interlock */
    FTVIEW_PRIV_EMERGENCY      = (1U << 14),  /**< O: emergency actions */
    FTVIEW_PRIV_ALL            = 0x7FFF
} ftview_privilege_t;

/* =====================================================================
 * L1.11 -- Security Role
 * ===================================================================== */

typedef struct {
    uint32_t           id;
    char               name[32];
    uint32_t           privilege_mask;  /**< bitmask of ftview_privilege_t */
    bool               auto_logout;     /**< auto-logout after inactivity */
    uint32_t           session_timeout_ms;
} ftview_role_t;

/* =====================================================================
 * L1.10 -- User Account
 * ===================================================================== */

typedef struct {
    uint32_t           id;
    char               username[FTVIEW_SEC_USERNAME_LEN];
    uint8_t            passhash[FTVIEW_SEC_PASSHASH_LEN];  /**< PBKDF2 output */
    uint8_t            salt[FTVIEW_SEC_SALT_LEN];
    uint32_t           role_id;
    bool               enabled;
    bool               locked_out;       /**< after consecutive failures */
    uint32_t           failed_attempts;
    int64_t            lockout_until_ms;
    int64_t            last_login_ms;
} ftview_user_t;

/* =====================================================================
 * L3.6 -- Audit Trail Entry
 * ===================================================================== */

typedef enum {
    FTVIEW_AUDIT_LOGIN          = 0,
    FTVIEW_AUDIT_LOGOUT         = 1,
    FTVIEW_AUDIT_LOGIN_FAIL     = 2,
    FTVIEW_AUDIT_ACK_ALARM      = 3,
    FTVIEW_AUDIT_CHANGE_SP      = 4,
    FTVIEW_AUDIT_START_EQ       = 5,
    FTVIEW_AUDIT_ESIGN          = 6,
    FTVIEW_AUDIT_CONFIGURE      = 7,
    FTVIEW_AUDIT_DOWNLOAD       = 8,
    FTVIEW_AUDIT_OVERRIDE       = 9,
    FTVIEW_AUDIT_SHELVE_ALARM   = 10,
    FTVIEW_AUDIT_USER_MGMT      = 11,
    FTVIEW_AUDIT_EMERGENCY      = 12
} ftview_audit_event_t;

typedef struct {
    uint64_t            sequence;
    int64_t             timestamp_ms;
    ftview_audit_event_t event;
    char                username[FTVIEW_SEC_USERNAME_LEN];
    char                detail[256];
    bool                success;
    char                station_id[32];   /**< workstation/host name */
} ftview_audit_entry_t;

/* =====================================================================
 * L2.6 -- Active Session
 * ===================================================================== */

typedef struct {
    uint32_t           session_id;
    uint32_t           user_id;
    char               username[FTVIEW_SEC_USERNAME_LEN];
    uint32_t           role_id;
    uint32_t           privilege_mask;
    int64_t            login_time_ms;
    int64_t            last_activity_ms;
    bool               active;
} ftview_session_t;

/* =====================================================================
 * Security Manager
 * ===================================================================== */

#define FTVIEW_SEC_SESSION_POOL_SIZE 32

typedef struct {
    ftview_user_t     users[FTVIEW_SEC_MAX_USERS];
    uint32_t          user_count;
    ftview_role_t     roles[FTVIEW_SEC_MAX_ROLES];
    uint32_t          role_count;
    ftview_session_t  sessions[FTVIEW_SEC_SESSION_POOL_SIZE];
    uint32_t          session_count;
    ftview_audit_entry_t audit[FTVIEW_SEC_AUDIT_MAX];
    uint64_t          audit_sequence;
    uint32_t          audit_count;
    uint32_t          max_failed_attempts;  /**< before lockout */
    uint32_t          lockout_duration_ms;
} ftview_security_mgr_t;

/* ───────── API ───────── */

void ftview_security_mgr_init(ftview_security_mgr_t *mgr);

/** Add a role definition. Returns FTVIEW_ERR_DUPLICATE_TAG on name conflict. */
ftview_err_t ftview_security_add_role(ftview_security_mgr_t *mgr,
                                       const ftview_role_t *role);

/** L3.5 -- Add a user with password. Password is hashed via PBKDF2-HMAC-SHA256
 *  with a random salt. Raw password is never stored. */
ftview_err_t ftview_security_add_user(ftview_security_mgr_t *mgr,
                                       const char *username,
                                       const char *password,
                                       uint32_t role_id);

/** L2.7 -- Authenticate user and create session.
 *  Returns session_id on success (>= 1), 0 on failure.
 *  Enforces: account enabled, not locked out, correct password,
 *  session pool not full. Failed attempts increment counter. */
uint32_t ftview_security_login(ftview_security_mgr_t *mgr,
                                const char *username,
                                const char *password,
                                const char *station_id);

/** Terminate session */
ftview_err_t ftview_security_logout(ftview_security_mgr_t *mgr,
                                     uint32_t session_id,
                                     const char *station_id);

/** L2.6 -- Check if a session has a specific privilege */
bool ftview_security_check_privilege(const ftview_security_mgr_t *mgr,
                                      uint32_t session_id,
                                      ftview_privilege_t priv);

/** Refresh session activity timestamp (heartbeat) */
ftview_err_t ftview_security_session_ping(ftview_security_mgr_t *mgr,
                                           uint32_t session_id);

/** L2.7 -- Expire idle sessions (call periodically) */
uint32_t ftview_security_expire_sessions(ftview_security_mgr_t *mgr,
                                          int64_t now_ms);

/** L3.6 -- Record an audit event */
ftview_err_t ftview_security_audit_log(ftview_security_mgr_t *mgr,
                                        ftview_audit_event_t event,
                                        const char *username,
                                        const char *detail,
                                        const char *station_id,
                                        bool success);

/** Query audit trail by time range */
uint32_t ftview_security_audit_query(const ftview_security_mgr_t *mgr,
                                      int64_t start_ms, int64_t end_ms,
                                      ftview_audit_entry_t *out,
                                      uint32_t max_out);

/** L3.5 -- Compute PBKDF2-HMAC-SHA256 hash (simplified, single iteration
 *  for embedded targets; production uses >= 100,000 iterations).
 *  out must be 32 bytes. */
void ftview_security_hash_password(const char *password,
                                    const uint8_t *salt, size_t salt_len,
                                    uint8_t *out);

/** Verify password against stored hash */
bool ftview_security_verify_password(const char *password,
                                      const uint8_t *stored_hash,
                                      const uint8_t *salt, size_t salt_len);

/** 21 CFR Part 11 -- Generate electronic signature record */
ftview_err_t ftview_security_esign(ftview_security_mgr_t *mgr,
                                    uint32_t session_id,
                                    const char *meaning,
                                    char *signature_out, size_t sig_len);

#endif /* FTVIEW_SECURITY_H */
