/**
 * @file plantpax_architecture.h
 * @brief PlantPAx DCS System Architecture ? Topology, Equipment Hierarchy, Network Layers
 *
 * Module: mini-rockwell-plantpax-dcs
 * Knowledge Coverage: L1 Definitions, L3 Engineering Structures, L4 ISA-88/ISA-95
 *
 * PlantPAx is Rockwell Automation's modern Distributed Control System (DCS),
 * built on the Logix control platform with integrated safety, motion, and
 * process control capabilities. Unlike traditional DCS systems with proprietary
 * controllers, PlantPAx uses the same ControlLogix hardware for both PLC and
 * DCS applications, unified under a common automation platform.
 *
 * Reference Architecture (ISA-95 Functional Hierarchy):
 *   Level 4: Enterprise (ERP, MES via FactoryTalk)
 *   Level 3: Operations Management (Historian, AssetCentre)
 *   Level 2: Supervisory Control (FactoryTalk View SE HMI)
 *   Level 1: Control (ControlLogix L8x with PlantPAx Process Objects)
 *   Level 0: I/O and Instrumentation (1756 I/O, FLEX I/O, POINT I/O)
 *   Field:  Sensors, actuators, transmitters
 *
 * Reference:
 *   ISA-95 Enterprise-Control System Integration
 *   ISA-88 Batch Control Standards
 *   Rockwell Automation PROCES-RM001 PlantPAx System Reference Manual
 * Curriculum: RWTH Aachen Industrial Control, Purdue ME 575, MIT 2.171
 */

#ifndef PLANTPAX_ARCHITECTURE_H
#define PLANTPAX_ARCHITECTURE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1: PlantPAx System Architecture - Core Type Definitions
 * ------------------------------------------------------------------------- */

/** Automation pyramid levels (ISA-95 functional hierarchy) */
typedef enum {
    PAX_LEVEL_FIELD = 0,
    PAX_LEVEL_IO = 1,
    PAX_LEVEL_CONTROL = 2,
    PAX_LEVEL_SUPERVISORY = 3,
    PAX_LEVEL_OPERATIONS = 4,
    PAX_LEVEL_ENTERPRISE = 5
} pax_level_t;

/** System service states */
typedef enum {
    PAX_SERVICE_RUNNING = 0,
    PAX_SERVICE_STANDBY = 1,
    PAX_SERVICE_MAINTENANCE = 2,
    PAX_SERVICE_OFFLINE = 3,
    PAX_SERVICE_DEGRADED = 4,
    PAX_SERVICE_COMMISSIONING = 5,
    PAX_SERVICE_SIMULATION = 6
} pax_service_state_t;

/** System health classification (derived from NAMUR NE107) */
typedef enum {
    PAX_HEALTH_OK = 0,
    PAX_HEALTH_MAINTENANCE_REQUIRED = 1,
    PAX_HEALTH_OUT_OF_SPEC = 2,
    PAX_HEALTH_CHECK_FUNCTION = 3,
    PAX_HEALTH_FAILURE = 4
} pax_health_t;

/** Network topology types in PlantPAx architecture */
typedef enum {
    PAX_TOPOLOGY_STAR = 0,
    PAX_TOPOLOGY_RING = 1,
    PAX_TOPOLOGY_LINEAR = 2,
    PAX_TOPOLOGY_REDUNDANT_STAR = 3,
    PAX_TOPOLOGY_TREE = 4
} pax_topology_t;

/** Redundancy modes for controllers and networks */
typedef enum {
    PAX_REDUNDANCY_NONE = 0,
    PAX_REDUNDANCY_HOT_STANDBY = 1,
    PAX_REDUNDANCY_WARM_STANDBY = 2,
    PAX_REDUNDANCY_COLD_STANDBY = 3,
    PAX_REDUNDANCY_N_OF_M = 4,
    PAX_REDUNDANCY_PARALLEL = 5
} pax_redundancy_mode_t;

/* ---------------------------------------------------------------------------
 * L1/L3: ISA-88 / ISA-95 Equipment Hierarchy
 * ------------------------------------------------------------------------- */

/** ISA-88 physical model hierarchy level */
typedef enum {
    PAX_ENT_ENTERPRISE = 0,
    PAX_ENT_SITE = 1,
    PAX_ENT_AREA = 2,
    PAX_ENT_PROCESS_CELL = 3,
    PAX_ENT_UNIT = 4,
    PAX_ENT_EQUIPMENT_MODULE = 5,
    PAX_ENT_CONTROL_MODULE = 6
} pax_entity_level_t;

/** Entity capability: what this entity can do */
typedef enum {
    PAX_CAP_PROCEDURAL = 0,
    PAX_CAP_EQUIPMENT = 1,
    PAX_CAP_CONTROL = 2,
    PAX_CAP_MONITOR = 3,
    PAX_CAP_SAFETY = 4
} pax_entity_capability_t;

/** State machine for ISA-88 procedural elements */
typedef enum {
    PAX_STATE_IDLE = 0,
    PAX_STATE_RUNNING = 1,
    PAX_STATE_PAUSING = 2,
    PAX_STATE_PAUSED = 3,
    PAX_STATE_HOLDING = 4,
    PAX_STATE_HELD = 5,
    PAX_STATE_RESTARTING = 6,
    PAX_STATE_STOPPING = 7,
    PAX_STATE_STOPPED = 8,
    PAX_STATE_ABORTING = 9,
    PAX_STATE_ABORTED = 10,
    PAX_STATE_COMPLETE = 11,
    PAX_STATE_RESETTING = 12
} pax_procedural_state_t;

/* ---------------------------------------------------------------------------
 * L1: System Component Categories
 * ------------------------------------------------------------------------- */

/** Controller families in PlantPAx ecosystem */
typedef enum {
    PAX_CTRL_CONTROLLOGIX_L8 = 0,
    PAX_CTRL_CONTROLLOGIX_L7 = 1,
    PAX_CTRL_COMPACTLOGIX_5069 = 2,
    PAX_CTRL_COMPACTLOGIX_1769 = 3,
    PAX_CTRL_FLEXLOGIX = 4,
    PAX_CTRL_COMPACT_GUARDLOGIX = 5,
    PAX_CTRL_GUARDLOGIX = 6
} pax_controller_family_t;

/** I/O platform families */
typedef enum {
    PAX_IO_1756_CHASSIS = 0,
    PAX_IO_5069_COMPACT = 1,
    PAX_IO_FLEX_5000 = 2,
    PAX_IO_FLEX = 3,
    PAX_IO_POINT = 4,
    PAX_IO_ARMOR = 5,
    PAX_IO_ARMOR_POWERFLEX = 6
} pax_io_family_t;

/** Network protocol layers in PlantPAx */
typedef enum {
    PAX_NET_ETHERNET_IP = 0,
    PAX_NET_DEVICE_LEVEL_RING = 1,
    PAX_NET_CONTROLNET = 2,
    PAX_NET_DEVICENET = 3,
    PAX_NET_HART_IP = 4,
    PAX_NET_MODBUS_TCP = 5,
    PAX_NET_OPC_UA = 6,
    PAX_NET_PROFIBUS_PA = 7
} pax_network_protocol_t;

/* ---------------------------------------------------------------------------
 * L1: Core Data Structures for System Architecture
 * ------------------------------------------------------------------------- */

/** Physical network segment definition */
typedef struct {
    uint32_t segment_id;
    pax_topology_t topology;
    pax_network_protocol_t protocol;
    double bandwidth_mbps;
    uint16_t max_nodes;
    uint16_t active_nodes;
    double rpi_ms;
    pax_redundancy_mode_t redundancy;
    bool dhcp_enabled;
    char subnet_mask[16];
    char gateway_address[16];
    uint32_t vlan_id;
} pax_network_segment_t;

/** Equipment entity in the ISA-88/95 hierarchy */
typedef struct {
    uint32_t entity_id;
    char tag_name[64];
    pax_entity_level_t level;
    pax_entity_capability_t capability;
    uint32_t parent_entity_id;
    pax_procedural_state_t state;
    pax_health_t health;
    uint32_t controller_id;
    double operational_hours;
    uint32_t num_children;
    uint32_t child_ids[32];
    char description[256];
} pax_entity_t;

/** System node descriptor (any device on the network) */
typedef struct {
    uint32_t node_id;
    char node_name[64];
    char ip_address[16];
    uint32_t network_segment_id;
    pax_level_t level;
    uint16_t rack_number;
    uint16_t slot_number;
    uint32_t firmware_revision;
    uint32_t serial_number;
    pax_health_t health;
    double uptime_hours;
    uint64_t bytes_tx;
    uint64_t bytes_rx;
    uint32_t comm_errors;
} pax_system_node_t;

/** System-wide configuration descriptor */
typedef struct {
    char project_name[128];
    char plant_name[128];
    uint32_t project_revision;
    uint32_t num_entities;
    uint32_t num_nodes;
    uint32_t num_controllers;
    uint32_t num_network_segments;
    pax_health_t overall_health;
    double system_availability_pct;
    double target_availability_pct;
    uint64_t total_io_points;
    uint64_t scan_count;
} pax_system_config_t;

/* ---------------------------------------------------------------------------
 * L1: Constants for PlantPAx Architecture
 * ------------------------------------------------------------------------- */

#define PAX_MAX_ENTITIES_PER_SYSTEM     1024
#define PAX_MAX_NODES_PER_SEGMENT       256
#define PAX_MAX_SEGMENTS_PER_SYSTEM     64
#define PAX_MAX_CONTROLLERS_PER_SYSTEM  32
#define PAX_DEFAULT_RPI_MS              20.0
#define PAX_ETHERNET_IP_MTU             1500
#define PAX_DLR_BEACON_INTERVAL_MS      3.0
#define PAX_DLR_RING_RECOVERY_MS        3.0
#define PAX_PLANT_CHASSIS_MAX_SLOTS     17

#define PAX_AVAILABILITY_STANDARD       99.90
#define PAX_AVAILABILITY_HIGH           99.95
#define PAX_AVAILABILITY_CRITICAL       99.99
#define PAX_AVAILABILITY_SAFETY         99.999

/* ---------------------------------------------------------------------------
 * L2/L3: System Architecture API
 * ------------------------------------------------------------------------- */

void pax_system_config_init(pax_system_config_t *config,
                             const char *project_name,
                             const char *plant_name);

int pax_register_network_segment(pax_network_segment_t *segment,
                                  pax_topology_t topology,
                                  pax_network_protocol_t protocol,
                                  double bandwidth_mbps,
                                  uint16_t max_nodes);

int pax_register_entity(pax_entity_t *entity,
                         pax_entity_level_t level,
                         const char *tag_name,
                         uint32_t parent_entity_id);

double pax_compute_system_availability(const double *component_availabilities,
                                        uint32_t num_components,
                                        uint32_t redundancy_pairs);

int pax_validate_system_config(const pax_system_config_t *config);

double pax_compute_network_load(const pax_network_segment_t *segment);

const char *pax_procedural_state_name(pax_procedural_state_t state);

const char *pax_entity_level_name(pax_entity_level_t level);

pax_health_t pax_assess_system_health(const pax_system_node_t *nodes,
                                       uint32_t num_nodes);

void pax_dump_architecture(const pax_system_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PLANTPAX_ARCHITECTURE_H */
