/**
 * @file    logix_redundancy.c
 * @brief   ControlLogix Redundancy System Implementation
 * L8: Controller redundancy, synchronization, availability
 * Reference: 1756-UM535
 */

#include <string.h>
#include <math.h>
#include "logix_redundancy.h"

void logix_redundancy_init(redundancy_system_t *sys,
                            uint32_t partner_slot)
{
    if (!sys) return;

    memset(sys, 0, sizeof(*sys));
    sys->partner_slot = partner_slot;
    sys->role = REDUN_ROLE_STANDALONE;
    sys->state = REDUN_STATE_INITIALIZING;
    sys->auto_sync_enabled = true;
}

bool logix_redundancy_qualify(const studio5000_project_t *primary,
                                const studio5000_project_t *secondary,
                                const logix_chassis_t *primary_chassis,
                                const logix_chassis_t *secondary_chassis,
                                redundancy_qualification_t *qual)
{
    if (!primary || !secondary || !qual) return false;

    memset(qual, 0, sizeof(*qual));
    bool all_pass = true;

    /* Firmware match */
    qual->firmware_match = (primary->firmware_compat_major ==
                            secondary->firmware_compat_major &&
                            primary->firmware_compat_minor ==
                            secondary->firmware_compat_minor);
    if (!qual->firmware_match) {
        all_pass = false;
        strncpy(qual->failure_reason, "Firmware mismatch", 255);
    }

    /* Program checksum */
    qual->checksum_primary = logix_redundancy_compute_checksum(primary);
    qual->checksum_secondary = logix_redundancy_compute_checksum(secondary);
    qual->program_checksum_match = (qual->checksum_primary ==
                                    qual->checksum_secondary);
    if (!qual->program_checksum_match) {
        all_pass = false;
        if (qual->failure_reason[0] == '\0') {
            strncpy(qual->failure_reason, "Program checksum mismatch", 255);
        }
    }

    /* I/O configuration */
    if (primary_chassis && secondary_chassis) {
        qual->io_config_match = (primary_chassis->size ==
                                 secondary_chassis->size);
        if (!qual->io_config_match) {
            all_pass = false;
            if (qual->failure_reason[0] == '\0') {
                strncpy(qual->failure_reason, "I/O configuration mismatch", 255);
            }
        }
    }

    /* Safety match */
    qual->safety_config_match = (primary->safety_signature_exists ==
                                 secondary->safety_signature_exists);
    if (!qual->safety_config_match) all_pass = false;

    return all_pass;
}

uint32_t logix_redundancy_compute_checksum(const studio5000_project_t *project)
{
    if (!project) return 0;

    /* CRC-32 checksum over project configuration */
    uint32_t crc = 0xFFFFFFFFu;

    /* Hash controller name */
    for (const char *p = project->controller_name; *p; p++) {
        crc ^= (uint8_t)*p;
        for (int i = 0; i < 8; i++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320u;
            else
                crc >>= 1;
        }
    }

    /* Hash task/program counts */
    crc ^= project->task_count;
    crc ^= project->program_count;

    /* Hash firmware version */
    crc ^= project->firmware_compat_major;
    crc ^= project->firmware_compat_minor;

    return ~crc;
}

double logix_redundancy_simulate_switchover(redundancy_system_t *sys,
                                              switchover_reason_t reason)
{
    if (!sys) return 0.0;

    sys->last_switchover_reason = reason;
    sys->switchover_count++;

    /* Base switchover time: 20ms (typical for L8x with RM2)
     * Add factors for:
     *   - Data cross-load size (each MB adds ~2ms)
     *   - Network path switching (~1ms per connection)
     *   - Motion axis re-homing delay (per axis)
     */
    double switchover_time_ms = 20.0;  /* Base 20ms */

    /* Data cross-load: ~2ms per MB */
    double data_mb = sys->data_crossload_bytes / (1024.0 * 1024.0);
    switchover_time_ms += data_mb * 2.0;

    /* Synchronization state penalty */
    if (sys->state != REDUN_STATE_SYNCHRONIZED) {
        switchover_time_ms += 100.0;  /* Not synchronized: cold start */
    }

    /* Motion axis re-home: ~500ms per axis */
    /* (simplified - not tracking exact axis count here) */

    sys->last_switchover_time_ms = switchover_time_ms;
    if (switchover_time_ms > sys->max_switchover_time_ms) {
        sys->max_switchover_time_ms = switchover_time_ms;
    }

    /* Update roles */
    if (sys->role == REDUN_ROLE_PRIMARY) {
        sys->role = REDUN_ROLE_SECONDARY;
    } else if (sys->role == REDUN_ROLE_SECONDARY) {
        sys->role = REDUN_ROLE_PRIMARY;
    }

    return switchover_time_ms;
}

uint32_t logix_redundancy_crossload_estimate(const logix_tag_t *tags,
                                               uint32_t tag_count,
                                               const studio5000_project_t *project)
{
    if (!tags) return 0;

    uint32_t total_bytes = 0;

    /* Tag data */
    for (uint32_t i = 0; i < tag_count; i++) {
        uint32_t size = logix_type_size(tags[i].type);
        if (tags[i].array_desc.dim_count > 0) {
            size *= tags[i].array_desc.total_elements;
        }
        total_bytes += size;
    }

    /* Program state overhead: ~1KB per program */
    if (project) {
        total_bytes += project->program_count * 1024;
    }

    return total_bytes;
}

double logix_redundancy_estimate_sync_time(uint32_t crossload_bytes)
{
    /* RM2 link: 100 Mbps = 12.5 MB/s
     * Sync_time = data / bandwidth + overhead
     * Overhead: ~500ms for qualification + handshake */
    double data_mb = crossload_bytes / (1024.0 * 1024.0);
    double sync_time_sec = data_mb / 12.5 + 0.5;

    return sync_time_sec;
}

bool logix_redundancy_is_bumpless(const redundancy_system_t *sys)
{
    if (!sys) return false;

    /* Bumpless switchover requires:
     *   - Synchronized state
     *   - Compatibility established
     *   - No disqualification */
    return (sys->state == REDUN_STATE_SYNCHRONIZED &&
            sys->compatibility_established &&
            sys->role != REDUN_ROLE_DISQUALIFIED);
}

void logix_net_redundancy_init(network_redundancy_t *net,
                                 network_redundancy_type_t type)
{
    if (!net) return;

    memset(net, 0, sizeof(*net));
    net->type = type;

    if (type == NET_REDUN_DLR) {
        net->beacon_interval_ms = 400;
        net->beacon_timeout_ms = 1960;
        net->ring_recovery_time_ms = 3;  /* Typical 3ms */
    }
}

double logix_net_dlr_recovery_time(double beacon_interval_us,
                                     uint32_t hop_count)
{
    /* DLR recovery = beacon_timeout + frame_delay * hop_count
     * Frame delay per hop: ~0.5us (cut-through switching)
     * Beacon timeout: default 1960us */
    double beacon_timeout_us = beacon_interval_us * 4.9;  /* Ratio ~4.9x */
    double frame_delay_per_hop_us = 0.5;

    return beacon_timeout_us + frame_delay_per_hop_us * (double)hop_count;
}

double logix_redundancy_availability(double mtbf_hours, double mttr_hours)
{
    if (mtbf_hours <= 0.0 || mttr_hours <= 0.0) return 0.0;

    /* Single controller availability */
    double a_single = mtbf_hours / (mtbf_hours + mttr_hours);

    /* Dual redundant: A = 1 - (1 - A_single)^2 */
    double a_dual = 1.0 - (1.0 - a_single) * (1.0 - a_single);

    /* Clamp to [0, 1] */
    if (a_dual < 0.0) a_dual = 0.0;
    if (a_dual > 1.0) a_dual = 1.0;

    return a_dual;
}