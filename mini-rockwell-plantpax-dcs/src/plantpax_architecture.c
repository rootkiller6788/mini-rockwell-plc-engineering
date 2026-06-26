/**
 * @file plantpax_architecture.c
 * @brief PlantPAx DCS System Architecture Implementation
 *
 * Implements system configuration, ISA-88 equipment hierarchy management,
 * availability calculations, network load analysis, and health assessment.
 */
#include "plantpax_architecture.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---------------------------------------------------------------------------
 * L2: System Configuration Initialization
 * ------------------------------------------------------------------------- */

void pax_system_config_init(pax_system_config_t *config,
                             const char *project_name,
                             const char *plant_name)
{
    if (config == NULL) return;

    memset(config, 0, sizeof(pax_system_config_t));

    if (project_name != NULL) {
        strncpy(config->project_name, project_name,
                sizeof(config->project_name) - 1);
    }
    if (plant_name != NULL) {
        strncpy(config->plant_name, plant_name,
                sizeof(config->plant_name) - 1);
    }

    config->project_revision = 1;
    config->num_entities = 0;
    config->num_nodes = 0;
    config->num_controllers = 0;
    config->num_network_segments = 0;
    config->overall_health = PAX_HEALTH_OK;
    config->system_availability_pct = 100.0;
    config->target_availability_pct = PAX_AVAILABILITY_STANDARD;
    config->total_io_points = 0;
    config->scan_count = 0;
}

/* ---------------------------------------------------------------------------
 * L3: Network Segment Registration
 * ------------------------------------------------------------------------- */

int pax_register_network_segment(pax_network_segment_t *segment,
                                  pax_topology_t topology,
                                  pax_network_protocol_t protocol,
                                  double bandwidth_mbps,
                                  uint16_t max_nodes)
{
    if (segment == NULL) return -1;
    if (max_nodes < 1 || max_nodes > PAX_MAX_NODES_PER_SEGMENT) return -2;
    if (bandwidth_mbps <= 0.0) return -3;

    memset(segment, 0, sizeof(pax_network_segment_t));
    segment->segment_id = 0;  /* caller must assign */
    segment->topology = topology;
    segment->protocol = protocol;
    segment->bandwidth_mbps = bandwidth_mbps;
    segment->max_nodes = max_nodes;
    segment->active_nodes = 0;
    segment->rpi_ms = PAX_DEFAULT_RPI_MS;
    segment->redundancy = PAX_REDUNDANCY_NONE;
    segment->dhcp_enabled = false;
    strncpy(segment->subnet_mask, "255.255.255.0",
            sizeof(segment->subnet_mask) - 1);
    strncpy(segment->gateway_address, "192.168.1.1",
            sizeof(segment->gateway_address) - 1);
    segment->vlan_id = 0;

    return 0;
}

/* ---------------------------------------------------------------------------
 * L3: ISA-88 Equipment Entity Registration
 * ------------------------------------------------------------------------- */

int pax_register_entity(pax_entity_t *entity,
                         pax_entity_level_t level,
                         const char *tag_name,
                         uint32_t parent_entity_id)
{
    if (entity == NULL) return -1;
    if (tag_name == NULL) return -2;

    memset(entity, 0, sizeof(pax_entity_t));

    /* entity_id must be assigned by caller */
    entity->level = level;
    strncpy(entity->tag_name, tag_name, sizeof(entity->tag_name) - 1);
    entity->parent_entity_id = parent_entity_id;
    entity->state = PAX_STATE_IDLE;
    entity->health = PAX_HEALTH_OK;
    entity->capability = PAX_CAP_MONITOR;  /* default: monitor only */
    entity->operational_hours = 0.0;
    entity->num_children = 0;
    memset(entity->child_ids, 0, sizeof(entity->child_ids));

    return 0;
}

/* ---------------------------------------------------------------------------
 * L4: System Availability Calculation (ISA-95)
 *
 * For a system composed of n components in series with r redundant pairs:
 *
 * Series availability:     A_i (per component)
 * Redundant pair:           A_redundant = 1 - (1 - A_a)(1 - A_b)
 * System availability:      A_sys = product(A_component_j) for all j
 *
 * This models real PlantPAx architectures where:
 *   - Power supplies are often redundant (1:1)
 *   - Controllers can be redundant (1756-RM2)
 *   - Networks can have DLR ring redundancy
 *   - I/O can be non-redundant (series)
 * ------------------------------------------------------------------------- */

double pax_compute_system_availability(const double *component_availabilities,
                                        uint32_t num_components,
                                        uint32_t redundancy_pairs)
{
    if (component_availabilities == NULL || num_components == 0) {
        return 0.0;
    }

    if (redundancy_pairs > num_components / 2) {
        redundancy_pairs = num_components / 2;
    }

    uint32_t i;
    double system_availability = 1.0;

    /* Process components: first r pairs are redundant, rest are series */
    for (i = 0; i < redundancy_pairs; i++) {
        /* Convert percentage to fraction */
        double a1 = component_availabilities[2 * i] / 100.0;
        double a2 = component_availabilities[2 * i + 1] / 100.0;

        /* Redundant pair formula: 1 - (1-A1)(1-A2) */
        double a_pair = 1.0 - (1.0 - a1) * (1.0 - a2);

        /* Clamp to valid range */
        if (a_pair > 1.0) a_pair = 1.0;
        if (a_pair < 0.0) a_pair = 0.0;

        system_availability *= a_pair;
    }

    /* Remaining components are in series */
    for (i = 2 * redundancy_pairs; i < num_components; i++) {
        double a_i = component_availabilities[i] / 100.0;
        if (a_i > 1.0) a_i = 1.0;
        if (a_i < 0.0) a_i = 0.0;
        system_availability *= a_i;
    }

    return system_availability * 100.0;  /* Return as percentage */
}

/* ---------------------------------------------------------------------------
 * L3: System Configuration Validation
 * ------------------------------------------------------------------------- */

int pax_validate_system_config(const pax_system_config_t *config)
{
    if (config == NULL) return -1;

    /* Check for zero entities */
    if (config->num_entities == 0) {
        return -10;  /* WARNING: empty system */
    }

    /* Check capacity limits */
    if (config->num_entities > PAX_MAX_ENTITIES_PER_SYSTEM) {
        return -11;
    }
    if (config->num_controllers > PAX_MAX_CONTROLLERS_PER_SYSTEM) {
        return -12;
    }
    if (config->num_network_segments > PAX_MAX_SEGMENTS_PER_SYSTEM) {
        return -13;
    }

    /* Target availability must be in valid range */
    if (config->target_availability_pct < 0.0 ||
        config->target_availability_pct > 100.0) {
        return -14;
    }

    /* System availability must be consistent */
    if (config->system_availability_pct < 0.0 ||
        config->system_availability_pct > 100.0) {
        return -15;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * L3: Network Load Calculation
 *
 * Network load estimation for EtherNet/IP:
 *
 * Load = sum over all nodes of (packet_size * 8 / bandwidth) / RPI
 *
 * Where:
 *   - packet_size includes CIP header + data (typical 200-500 bytes)
 *   - bandwidth is in Mbps
 *   - RPI is the Requested Packet Interval
 *
 * For DLR ring topology, each packet travels the full ring
 * (N-1 hops), so actual load = single-segment load * (N-1).
 * ------------------------------------------------------------------------- */

double pax_compute_network_load(const pax_network_segment_t *segment)
{
    if (segment == NULL) return -1.0;
    if (segment->active_nodes == 0) return 0.0;

    /* Typical EtherNet/IP implicit messaging packet size */
    double avg_packet_size_bytes = 400.0;
    double bits_per_packet = avg_packet_size_bytes * 8.0;
    double bandwidth_bps = segment->bandwidth_mbps * 1e6;

    /* Packets per second per node = 1 / (RPI in seconds) */
    double rpi_seconds = segment->rpi_ms / 1000.0;
    if (rpi_seconds <= 0.0) rpi_seconds = 0.001;  /* 1 ms minimum */

    double packets_per_second = 1.0 / rpi_seconds;
    double bits_per_second_per_node = bits_per_packet * packets_per_second;

    /* Total bits per second for all nodes */
    double total_bps = bits_per_second_per_node * segment->active_nodes;

    /* DLR ring: each packet goes around the ring */
    if (segment->topology == PAX_TOPOLOGY_RING) {
        total_bps *= (double)(segment->active_nodes - 1);
        if (segment->active_nodes <= 1) total_bps = bits_per_second_per_node;
    }

    double load_pct = (total_bps / bandwidth_bps) * 100.0;

    /* Clamp to valid range */
    if (load_pct > 100.0) load_pct = 100.0;
    if (load_pct < 0.0) load_pct = 0.0;

    return load_pct;
}

/* ---------------------------------------------------------------------------
 * L2: Procedural State Name Conversion
 * ------------------------------------------------------------------------- */

const char *pax_procedural_state_name(pax_procedural_state_t state)
{
    static const char *names[] = {
        "IDLE", "RUNNING", "PAUSING", "PAUSED",
        "HOLDING", "HELD", "RESTARTING", "STOPPING",
        "STOPPED", "ABORTING", "ABORTED", "COMPLETE", "RESETTING"
    };

    if (state >= 0 && state <= PAX_STATE_RESETTING) {
        return names[state];
    }
    return "UNKNOWN";
}

/* ---------------------------------------------------------------------------
 * L2: Entity Level Name Conversion
 * ------------------------------------------------------------------------- */

const char *pax_entity_level_name(pax_entity_level_t level)
{
    static const char *names[] = {
        "Enterprise", "Site", "Area",
        "Process Cell", "Unit", "Equipment Module", "Control Module"
    };

    if (level >= 0 && level <= PAX_ENT_CONTROL_MODULE) {
        return names[level];
    }
    return "UNKNOWN";
}

/* ---------------------------------------------------------------------------
 * L2: System Health Assessment
 *
 * Propagation rule:
 *   - Any FAILURE -> system FAILURE
 *   - Any CHECK_FUNCTION -> system CHECK_FUNCTION
 *   - Any OUT_OF_SPEC w/o higher -> system OUT_OF_SPEC
 *   - Any MAINTENANCE_REQUIRED w/o higher -> system MAINTENANCE_REQUIRED
 *   - All OK -> system OK
 * ------------------------------------------------------------------------- */

pax_health_t pax_assess_system_health(const pax_system_node_t *nodes,
                                       uint32_t num_nodes)
{
    if (nodes == NULL || num_nodes == 0) {
        return PAX_HEALTH_FAILURE;
    }

    pax_health_t worst = PAX_HEALTH_OK;
    uint32_t i;

    for (i = 0; i < num_nodes; i++) {
        if (nodes[i].health > worst) {
            worst = nodes[i].health;
        }
        /* Early return on worst case */
        if (worst == PAX_HEALTH_FAILURE) break;
    }

    return worst;
}

/* ---------------------------------------------------------------------------
 * L3: Architecture Dump (Diagnostic)
 * ------------------------------------------------------------------------- */

void pax_dump_architecture(const pax_system_config_t *config)
{
    if (config == NULL) {
        printf("PlantPAx Architecture: (null)\n");
        return;
    }

    printf("===========================================\n");
    printf("  PlantPAx DCS Architecture Summary\n");
    printf("===========================================\n");
    printf("  Project:       %s\n", config->project_name);
    printf("  Plant:         %s\n", config->plant_name);
    printf("  Revision:      %u\n", config->project_revision);
    printf("  Entities:      %u\n", config->num_entities);
    printf("  Controllers:   %u\n", config->num_controllers);
    printf("  Network Segs:  %u\n", config->num_network_segments);
    printf("  Total Nodes:   %u\n", config->num_nodes);
    printf("  I/O Points:    %llu\n",
           (unsigned long long)config->total_io_points);
    printf("  Availability:  %.4f%% (target: %.4f%%)\n",
           config->system_availability_pct,
           config->target_availability_pct);
    printf("  Overall Health: ");
    switch (config->overall_health) {
        case PAX_HEALTH_OK:
            printf("OK\n");
            break;
        case PAX_HEALTH_MAINTENANCE_REQUIRED:
            printf("MAINTENANCE REQUIRED\n");
            break;
        case PAX_HEALTH_OUT_OF_SPEC:
            printf("OUT OF SPEC\n");
            break;
        case PAX_HEALTH_CHECK_FUNCTION:
            printf("CHECK FUNCTION\n");
            break;
        case PAX_HEALTH_FAILURE:
            printf("FAILURE\n");
            break;
        default:
            printf("UNKNOWN\n");
    }
    printf("  Scan Count:    %llu\n",
           (unsigned long long)config->scan_count);
    printf("===========================================\n");
}
