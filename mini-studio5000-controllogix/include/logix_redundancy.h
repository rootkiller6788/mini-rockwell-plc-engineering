/**
 * @file    logix_redundancy.h
 * @brief   L8: ControlLogix Redundancy System (CRS)
 *
 * Knowledge Mapping:
 *   L8 Advanced Topics: Controller redundancy, bumpless switchover,
 *                       synchronization qualification, network path
 *                       validation, redundancy diagnostics
 *   L2 Concepts:        Hot standby, data cross-loading, program
 *                       synchronization, forced switchover
 *   L7 Applications:    Rockwell 1756-RM2 Redundancy Module,
 *                       ControlLogix Enhanced Redundancy (L7x)
 *
 * Reference: Rockwell Automation Publication 1756-UM535
 *   "ControlLogix Enhanced Redundancy System User Manual"
 */

#ifndef LOGIX_REDUNDANCY_H
#define LOGIX_REDUNDANCY_H

#include <stdint.h>
#include <stdbool.h>
#include "control_logix_platform.h"
#include "studio5000_project.h"
#include "logix_tag_model.h"

typedef enum {
    REDUN_ROLE_PRIMARY    = 0,
    REDUN_ROLE_SECONDARY  = 1,
    REDUN_ROLE_DISQUALIFIED = 2,
    REDUN_ROLE_STANDALONE  = 3
} redundancy_role_t;

typedef enum {
    REDUN_STATE_INITIALIZING    = 0,
    REDUN_STATE_SYNCING         = 1,
    REDUN_STATE_SYNCHRONIZED    = 2,
    REDUN_STATE_SWITCHOVER      = 3,
    REDUN_STATE_DISQUALIFIED    = 4,
    REDUN_STATE_PAUSED          = 5
} redundancy_state_t;

typedef struct {
    bool     firmware_match;
    bool     program_checksum_match;
    bool     io_config_match;
    bool     network_config_match;
    bool     safety_config_match;
    bool     rack_optimization_match;
    bool     motion_config_match;
    uint32_t checksum_primary;
    uint32_t checksum_secondary;
    char     failure_reason[256];
} redundancy_qualification_t;

typedef enum {
    SWITCHOVER_NONE            = 0,
    SWITCHOVER_USER_COMMAND    = 1,
    SWITCHOVER_MAJOR_FAULT     = 2,
    SWITCHOVER_POWER_LOSS      = 3,
    SWITCHOVER_NETWORK_LOSS    = 4,
    SWITCHOVER_CHASSIS_REMOVAL = 5,
    SWITCHOVER_AUTO_TEST       = 6
} switchover_reason_t;

typedef struct {
    redundancy_qualification_t qualification;
    redundancy_role_t         role;
    redundancy_state_t        state;
    uint32_t                  partner_slot;
    bool                      auto_sync_enabled;
    bool                      compatibility_established;
    uint64_t                  last_sync_timestamp;
    uint64_t                  last_switchover_timestamp;
    switchover_reason_t       last_switchover_reason;
    uint32_t                  switchover_count;
    uint32_t                  failed_switchover_count;
    double                    last_switchover_time_ms;
    double                    max_switchover_time_ms;
    uint32_t                  sync_attempt_count;
    uint32_t                  sync_failure_count;
    uint32_t                  data_crossload_bytes;
    bool                      motion_redundancy_supported;
    bool                      safety_redundancy_supported;
} redundancy_system_t;

typedef enum {
    NET_REDUN_NONE   = 0,
    NET_REDUN_DLR    = 1,
    NET_REDUN_PRP    = 2,
    NET_REDUN_LINEAR_STAR = 3
} network_redundancy_type_t;

typedef struct {
    network_redundancy_type_t type;
    bool     ring_supervisor;
    bool     ring_active;
    bool     ring_faulted;
    uint32_t beacon_interval_ms;
    uint32_t beacon_timeout_ms;
    uint32_t ring_recovery_time_ms;
    uint32_t ring_participant_count;
    bool     prp_lan_a_active;
    bool     prp_lan_b_active;
} network_redundancy_t;

void logix_redundancy_init(redundancy_system_t *sys, uint32_t partner_slot);
bool logix_redundancy_qualify(const studio5000_project_t *primary,
                                const studio5000_project_t *secondary,
                                const logix_chassis_t *primary_chassis,
                                const logix_chassis_t *secondary_chassis,
                                redundancy_qualification_t *qual);
uint32_t logix_redundancy_compute_checksum(const studio5000_project_t *project);
double logix_redundancy_simulate_switchover(redundancy_system_t *sys,
                                              switchover_reason_t reason);
uint32_t logix_redundancy_crossload_estimate(const logix_tag_t *tags,
                                               uint32_t tag_count,
                                               const studio5000_project_t *project);
double logix_redundancy_estimate_sync_time(uint32_t crossload_bytes);
bool logix_redundancy_is_bumpless(const redundancy_system_t *sys);
void logix_net_redundancy_init(network_redundancy_t *net,
                                 network_redundancy_type_t type);
double logix_net_dlr_recovery_time(double beacon_interval_us, uint32_t hop_count);
double logix_redundancy_availability(double mtbf_hours, double mttr_hours);

#endif /* LOGIX_REDUNDANCY_H */