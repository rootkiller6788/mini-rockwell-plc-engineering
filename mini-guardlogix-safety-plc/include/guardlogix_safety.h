/**
 * guardlogix_safety.h — GuardLogix Safety PLC Core Definitions
 *
 * Covers L1 Definitions: GuardLogix architecture, safety controller types,
 * safety partner relationship, SIL/PL safety classifications, safety task
 * types, safety tag attributes, safety status categories.
 *
 * Reference: Rockwell Automation GuardLogix 5580 Safety Reference Manual
 *            IEC 61508:2010 Functional Safety of E/E/PE Systems
 *            IEC 61511:2016 Functional Safety — Safety Instrumented Systems
 *
 * GuardLogix is Rockwell Automation's safety-certified PLC platform built on
 * the Logix5000 control engine with dual-channel safety architecture.
 * It achieves SIL 3 / PL e certification via 1oo2D (one-out-of-two with
 * diagnostics) architecture with diverse processing.
 */

#ifndef GUARDLOGIX_SAFETY_H
#define GUARDLOGIX_SAFETY_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ==========================================================================
 * L1: Core Definitions — Safety PLC Architecture
 * ========================================================================== */

/**
 * Safety Integrity Level — per IEC 61508-1 Table 2/3
 *
 * SIL 1: PFDavg 10^-2 to <10^-1, RRF 10-100
 * SIL 2: PFDavg 10^-3 to <10^-2, RRF 100-1000
 * SIL 3: PFDavg 10^-4 to <10^-3, RRF 1000-10000
 * SIL 4: PFDavg 10^-5 to <10^-4, RRF 10000-100000
 *
 * GuardLogix supports up to SIL 3 (PLe per ISO 13849-1).
 */
typedef enum {
    GLX_SIL_NONE  = 0,
    GLX_SIL_1     = 1,
    GLX_SIL_2     = 2,
    GLX_SIL_3     = 3,
    GLX_SIL_4     = 4
} glx_sil_level_t;

/**
 * Performance Level — per ISO 13849-1
 */
typedef enum {
    GLX_PL_A = 0,
    GLX_PL_B = 1,
    GLX_PL_C = 2,
    GLX_PL_D = 3,
    GLX_PL_E = 4
} glx_pl_level_t;

/**
 * GuardLogix controller family identifiers.
 */
typedef enum {
    GLX_CTRL_5560S  = 0,
    GLX_CTRL_5570S  = 1,
    GLX_CTRL_5580S  = 2,
    GLX_CTRL_COMPACT = 3
} glx_controller_family_t;

/**
 * Safety partner role — dual-processor architecture.
 */
typedef enum {
    GLX_PARTNER_PRIMARY   = 0,
    GLX_PARTNER_SECONDARY = 1
} glx_safety_partner_role_t;

/**
 * Safety partner status.
 */
typedef enum {
    GLX_PARTNER_OK          = 0,
    GLX_PARTNER_FAULTED     = 1,
    GLX_PARTNER_MISMATCH    = 2,
    GLX_PARTNER_UNLOCKED    = 3,
    GLX_PARTNER_OFFLINE     = 4,
    GLX_PARTNER_DEGRADED    = 5
} glx_safety_partner_status_t;

/**
 * Safety lock/unlock state.
 */
typedef enum {
    GLX_LOCK_STATE_LOCKED      = 0,
    GLX_LOCK_STATE_UNLOCKING   = 1,
    GLX_LOCK_STATE_UNLOCKED    = 2
} glx_safety_lock_state_t;

/**
 * Safety task type enumeration.
 */
typedef enum {
    GLX_SAFETY_TASK_PERIODIC  = 0,
    GLX_SAFETY_TASK_EVENT     = 1,
    GLX_SAFETY_TASK_SIGNATURE = 2
} glx_safety_task_type_t;

/**
 * Safety tag class.
 */
typedef enum {
    GLX_TAG_CLASS_SAFETY = 0,
    GLX_TAG_CLASS_STANDARD = 1,
    GLX_TAG_CLASS_SAFETY_MAPPED = 2
} glx_tag_class_t;

/**
 * Safety I/O module types.
 */
typedef enum {
    GLX_IO_DISCRETE_IN   = 0,
    GLX_IO_DISCRETE_OUT  = 1,
    GLX_IO_ANALOG_IN     = 2,
    GLX_IO_RELAY_OUT     = 3,
    GLX_IO_SPEED_MONITOR = 4,
    GLX_IO_GENERIC       = 5
} glx_safety_io_type_t;

/**
 * GuardLogix operational state.
 */
typedef enum {
    GLX_STATE_POWER_UP       = 0,
    GLX_STATE_SAFETY_LOCKED  = 1,
    GLX_STATE_SAFETY_FAULT   = 2,
    GLX_STATE_SAFETY_TASK_OVERRUN = 3,
    GLX_STATE_FIRMWARE_UPDATE = 4,
    GLX_STATE_HARD_FAULT     = 5
} glx_controller_state_t;

/**
 * Safety signature structure — 64-bit CRC-based project signature.
 */
typedef struct {
    uint64_t signature;
    uint32_t generation_time;
    uint32_t safety_task_checksum;
    uint32_t safety_io_config_checksum;
    uint16_t safety_network_number;
    uint16_t partner_signature_matched;
} glx_safety_signature_t;

/**
 * Safety Network Number (SNN) — per CIP Safety Vol. 8.
 */
typedef struct {
    uint16_t snn_value;
    uint8_t  snn_format;
    uint8_t  snn_uniqueness_valid;
    uint32_t snn_generation_time;
} glx_safety_network_number_t;

/**
 * 1oo2D diagnostic status — cross-channel comparison.
 */
typedef struct {
    uint8_t  diagnostic_active;
    uint8_t  cross_check_passing;
    uint8_t  diverse_compiler_ok;
    uint8_t  cycle_counter_synced;
    uint32_t mismatch_count;
    uint32_t last_mismatch_cycle;
    uint16_t cross_check_latency_us;
} glx_1oo2d_diagnostics_t;

/**
 * Safety watchdog configuration.
 */
typedef struct {
    uint32_t watchdog_timeout_ms;
    uint32_t max_task_period_ms;
    uint8_t  watchdog_enabled;
    uint32_t last_reset_cycle;
    uint8_t  overrun_count;
} glx_safety_watchdog_t;

/**
 * GuardLogix safety controller object — complete safety PLC instance.
 */
typedef struct {
    uint16_t                controller_id;
    glx_controller_family_t family;
    glx_controller_state_t  state;
    glx_safety_lock_state_t lock_state;
    glx_sil_level_t         max_sil;
    glx_pl_level_t          max_pl;
    glx_safety_partner_role_t   my_role;
    glx_safety_partner_status_t partner_status;
    glx_safety_task_type_t  safety_task_type;
    uint32_t                safety_task_period_ms;
    uint32_t                safety_task_max_scan_us;
    glx_safety_watchdog_t   watchdog;
    glx_safety_signature_t  project_sig;
    glx_safety_signature_t  partner_sig;
    glx_safety_network_number_t snn;
    glx_1oo2d_diagnostics_t diag_1oo2d;
    uint32_t                proof_test_interval_hours;
    uint32_t                last_proof_test_time;
    uint32_t                proof_test_due_time;
    uint16_t                max_safety_connections;
    uint16_t                active_safety_connections;
    uint32_t                safety_connection_ids[16];
    uint16_t                safety_input_count;
    uint16_t                safety_output_count;
} glx_safety_controller_t;

int glx_safety_controller_init(glx_safety_controller_t *ctrl,
                                glx_controller_family_t family,
                                int is_primary,
                                uint32_t period_ms);

int glx_safety_state_transition(glx_safety_controller_t *ctrl,
                                 glx_controller_state_t new_state);

int glx_cross_check_update(glx_safety_controller_t *ctrl,
                           const uint8_t *my_data,
                           const uint8_t *partner_data,
                           size_t data_len);

int glx_verify_partner_signature(const glx_safety_controller_t *ctrl);

int glx_compute_proof_test_interval(glx_sil_level_t target_sil,
                                     double lambda_du,
                                     double dc,
                                     double beta,
                                     uint32_t *t_pt_out);

int glx_validate_snn(const glx_safety_network_number_t *snn);

uint32_t glx_compute_safety_reaction_time(const glx_safety_controller_t *ctrl,
                                          uint32_t input_delay_ms,
                                          uint32_t network_rpi_ms,
                                          uint32_t output_delay_ms);

#endif /* GUARDLOGIX_SAFETY_H */
