/**
 * @file plc_rockwell_config.h
 * @brief Rockwell Controller Configuration ? Model Management, Firmware, Redundancy
 *
 * Knowledge Coverage: L1 ? L2 ? L4 ? L7 (Industrial Applications)
 *
 * This header models the Rockwell ControlLogix / CompactLogix / GuardLogix
 * controller family configuration. It covers controller types, firmware
 * versions, redundancy configurations, and system-level parameters.
 *
 * Controller Families:
 *   - ControlLogix 5580 (1756-L8x): High-performance modular platform
 *   - CompactLogix 5380 (5069-L3xx): Mid-range integrated platform
 *   - CompactLogix 5480 (5069-L4xx): Windows + Logix on same hardware
 *   - GuardLogix 5580 (1756-L8xS): Safety-rated (SIL 2/3)
 *   - MicroLogix 1400: Legacy platform (MicroLogix, not RSLogix 5000)
 *
 * Reference: Rockwell Selection Guide 1756-SG001
 * Ref: IEC 61508 (Functional safety), IEC 62443 (Industrial cybersecurity)
 */

#ifndef PLC_ROCKWELL_CONFIG_H
#define PLC_ROCKWELL_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Controller Model Definitions
 * ============================================================================ */

/** Controller platform families */
typedef enum {
    PLATFORM_CONTROLLOGIX_5570 = 0,  /**< 1756-L7x series (v20-v32)        */
    PLATFORM_CONTROLLOGIX_5580 = 1,  /**< 1756-L8x series (v28+)           */
    PLATFORM_COMPACTLOGIX_5370 = 2,  /**< 1769-L3x series (v20-v32)        */
    PLATFORM_COMPACTLOGIX_5380 = 3,  /**< 5069-L3xx series (v28+)          */
    PLATFORM_COMPACTLOGIX_5480 = 4,  /**< 5069-L4xx series (v32+)          */
    PLATFORM_GUARDLOGIX_5570   = 5,  /**< 1756-L7xS (v20+)                 */
    PLATFORM_GUARDLOGIX_5580   = 6,  /**< 1756-L8xS (v31+)                 */
    PLATFORM_SOFTLOGIX_5800    = 7,  /**< SoftLogix 5800 (v21+)            */
    PLATFORM_MICRO800          = 8   /**< Micro800 (CCW-based)             */
} plc_platform_t;

/** Specific controller models with key specifications */
typedef struct {
    const char*     model_number;  /**< e.g., "1756-L85E"                    */
    plc_platform_t  platform;      /**< Platform family                      */
    uint16_t        max_tasks;     /**< Maximum number of tasks              */
    uint16_t        max_programs;  /**< Maximum programs per task            */
    uint32_t        max_tags;      /**< Maximum controller-scope tags        */
    uint32_t        user_memory_mb;/**< User-accessible memory (MB)          */
    uint32_t        io_memory_kb;  /**< I/O memory (KB)                      */
    bool            supports_safety;/**< Can run GuardLogix safety firmware  */
    bool            supports_motion;/**< Integrated motion (Kintex/Delta)    */
    bool            dual_ethernet; /**< Dual Ethernet/IP ports              */
    bool            supports_cip_security;/**< CIP Security (IEC 62443-4-2) */
    uint32_t        max_cip_connections;/**< Max CIP class 3 connections     */
    uint16_t        max_axes;      /**< Maximum motion axes                  */
    uint16_t        max_en2t_nodes;/**< Max EtherNet/IP adapter nodes        */
} plc_controller_spec_t;

/* ============================================================================
 * L2: Controller Configuration ? The .ACD project entity
 * ============================================================================
 * In Studio 5000, a project (.ACD file) contains the complete configuration
 * for a single controller. This includes:
 *   - Controller definition (model, firmware, slot)
 *   - Task and program organization
 *   - Tag database
 *   - I/O configuration tree
 *   - Communication settings
 *   - Safety configuration (if GuardLogix)
 *   - Motion group configuration
 *
 * This structure models the top-level project configuration.
 */

/** Redundancy mode ? ControlLogix high availability */
typedef enum {
    REDUNDANCY_NONE        = 0, /**< Simplex (single controller)             */
    REDUNDANCY_HOT_STANDBY = 1, /**< Primary + Secondary, bumpless switchover */
    REDUNDANCY_WARM_STANDBY= 2  /**< State synced periodically (deprecated)  */
} plc_redundancy_mode_t;

/** Controller operating mode */
typedef enum {
    CTRL_MODE_PROGRAM   = 0,  /**< Program mode: logic not executing         */
    CTRL_MODE_RUN       = 1,  /**< Run mode: logic executing normally        */
    CTRL_MODE_TEST      = 2,  /**< Test mode: step/breakpoint support        */
    CTRL_MODE_FAULTED   = 3   /**< Major fault: logic halted                */
} plc_controller_mode_t;

/** Time synchronization source */
typedef enum {
    TIME_SRC_NONE       = 0,   /**< No time sync (free-running)             */
    TIME_SRC_CST_NTP    = 1,   /**< Cisco NTP via backplane                 */
    TIME_SRC_1588_PTP   = 2,   /**< IEEE 1588 Precision Time Protocol       */
    TIME_SRC_CIP_SYNC   = 3,   /**< CIP Sync via Ethernet/IP                */
    TIME_SRC_GPS        = 4    /**< External GPS receiver (1756-TIME)       */
} plc_time_sync_source_t;

/** SD card configuration */
typedef enum {
    SD_MODE_DISABLED    = 0,   /**< No SD card operations                   */
    SD_MODE_LOAD_ON_CORRUPT=1,/**< Auto-restore from SD on corrupt firmware */
    SD_MODE_LOAD_ON_POWERUP=2,/**< Always load from SD on power-up          */
    SD_MODE_LOG_TO_SD   = 3    /**< Data logging destination                */
} plc_sd_card_mode_t;

/** Complete controller configuration */
typedef struct {
    char                    project_name[128];  /**< .ACD project name           */
    char                    controller_name[64]; /**< Logix controller name      */
    plc_platform_t          platform;           /**< Platform family             */
    char                    model_number[32];   /**< "1756-L85E"                 */
    uint32_t                firmware_major;     /**< e.g., 36                    */
    uint32_t                firmware_minor;     /**< e.g., 11                    */
    uint32_t                firmware_build;     /**< Build number                */
    uint8_t                 slot_number;        /**< Chassis slot (0-16)         */
    plc_redundancy_mode_t   redundancy_mode;    /**< Redundancy configuration    */
    plc_controller_mode_t   operating_mode;     /**< Program/Run/Test/Faulted    */
    bool                    online_edits_enabled;/**< Online editing allowed      */
    uint32_t                watchdog_ms;        /**< Task watchdog timeout (ms)   */
    uint32_t                system_overhead_pct;/**< % slice for communications  */
    /* Time sync */
    plc_time_sync_source_t  time_sync_source;
    bool                    is_time_master;     /**< Grandmaster clock           */
    double                  time_zone_offset;   /**< UTC offset hours            */
    /* Memory */
    uint32_t                user_memory_bytes;  /**< Total user memory allocated */
    uint32_t                safety_memory_bytes;/**< Safety partition memory     */
    /* Redundancy params */
    uint16_t                secondary_slot;     /**< Chassis slot of secondary   */
    uint32_t                max_scan_delta_us;  /**< Max discrepancy before fail */
    /* Security (IEC 62443) */
    bool                    cip_security_enabled;
    bool                    firmware_signing_required;
    bool                    logic_source_protection;
    char                    factorytalk_realm[64];/**< FactoryTalk Security realm */
    /* SD Card */
    plc_sd_card_mode_t      sd_card_mode;
    bool                    sd_card_present;
    /* Diagnostics */
    bool                    enable_minor_fault_logging;
    bool                    enable_trace_monitoring;
    uint32_t                trace_buffer_size;  /**< Trace log entries           */
} plc_controller_config_t;

/* ============================================================================
 * L2: Task Configuration ? Logix 5000 multitasking model
 * ============================================================================
 * Logix 5000 supports three task types:
 *   1. Continuous: Runs as background, never stops, lowest priority
 *   2. Periodic: Fixed interval, interrupts continuous task
 *   3. Event: Triggered by EVENT instruction, I/O module, or consumed tag
 *
 * Task priority 1 (highest) to 15 (lowest). Continuous task is always
 * priority 15. Periodic and event tasks are 1-14.
 *
 * This multi-task model is a key differentiator from traditional PLCs
 * that only have a single cyclic scan. It enables mixed time-critical
 * and non-critical logic on the same controller.
 */

/** Logix task type */
typedef enum {
    TASK_TYPE_CONTINUOUS = 0,   /**< Background, never stops                  */
    TASK_TYPE_PERIODIC   = 1,   /**< Fixed-interval, preemptive              */
    TASK_TYPE_EVENT      = 2    /**< Triggered by external event              */
} plc_task_type_t;

/** Task event trigger type */
typedef enum {
    EVENT_NONE          = 0,    /**< Not applicable                          */
    EVENT_INSTRUCTION   = 1,    /**< EVENT instruction in logic              */
    EVENT_MODULE_INPUT  = 2,    /**< Input module state change               */
    EVENT_CONSUMED_TAG  = 3,    /**< Consumed tag data arrival               */
    EVENT_MOTION_PHASE  = 4,    /**< Motion registration event               */
    EVENT_AXIS_STATUS   = 5     /**< Axis status change                      */
} plc_event_trigger_t;

/** Single task configuration */
typedef struct {
    char                name[64];            /**< Task name                     */
    plc_task_type_t     type;                /**< Continuous/Periodic/Event     */
    uint8_t             priority;            /**< 1(highest)-15(lowest)        */
    uint32_t            period_us;           /**< Period in microseconds       */
    uint32_t            watchdog_us;         /**< Task watchdog timeout        */
    bool                disable_outputs;     /**< Freeze outputs during task   */
    bool                inhibit_task;        /**< Suspend task execution       */
    bool                allow_event_overlap; /**< Multiple event instances?    */
    plc_event_trigger_t event_trigger;       /**< Trigger for EVENT type       */
    char                event_tag_name[64];  /**< Tag monitored for events     */
    uint16_t            program_count;       /**< Programs scheduled in task   */
    char                program_names[32][64];/**< Program name list            */
} plc_task_config_t;

/* ============================================================================
 * L2: Program Configuration ? IEC 61131-3 PROGRAM unit
 * ============================================================================
 * In Logix 5000, a "Program" is an IEC 61131-3 PROGRAM POU. It contains:
 *   - Program-scoped tags (invisible to other programs)
 *   - Routines (the actual logic, typically in Ladder, FBD, or ST)
 *   - An optional fault routine
 *
 * Programs are scheduled within tasks. A program can appear in only one task
 * but the same task can run multiple programs sequentially.
 */

/** Language selection for a routine (Logix 5000 v21+) */
typedef enum {
    RUNG_LANG_LADDER       = 0,  /**< Ladder Diagram (LD)                     */
    RUNG_LANG_FBD          = 1,  /**< Function Block Diagram (FBD)            */
    RUNG_LANG_STRUCTURED   = 2,  /**< Structured Text (ST)                    */
    RUNG_LANG_SEQUENTIAL   = 3   /**< Sequential Function Chart (SFC)         */
} plc_routine_language_t;

/** Program configuration */
typedef struct {
    char                    name[64];            /**< Program name              */
    char                    description[256];     /**< Program description       */
    uint16_t                routine_count;        /**< Number of routines        */
    char                    main_routine[64];     /**< Main routine name        */
    char                    fault_routine[64];    /**< Fault handler (optional) */
    bool                    is_safety_program;    /**< GuardLogix safety flag   */
    bool                    disable_program;      /**< Inhibit this program     */
    bool                    synchronize_redundancy;/**< Crossload to secondary   */
    plc_routine_language_t  default_language;     /**< Default new routine lang */
} plc_program_config_t;

/* ============================================================================
 * L4: Engineering Laws ? IEC 61508 Safety Configuration
 * ============================================================================
 * GuardLogix controllers implement IEC 61508 SIL 2/3 through a combination
 * of hardware redundancy and software safety mechanisms.
 *
 * Safety Task Properties:
 *   - Fixed 5-500 ms period
 *   - Safety Task Signature (STS) computed at download
 *   - Dual processor execution with result comparison
 *   - All safety tags have inverted-copy stored in safety partition
 *   - Safety logic executes in lockstep (no online edits)
 *
 * Ref: Rockwell SAFETY-AT020, IEC 61508-3 ?7.4
 */

/** Safety configuration for the controller */
typedef struct {
    bool            safety_enabled;          /**< Safety firmware active           */
    uint8_t         target_sil;              /**< Target SIL (2 or 3)              */
    char            safety_task_name[64];    /**< Name of the safety task          */
    uint32_t        safety_period_ms;        /**< Safety task period (5-500ms)     */
    uint32_t        safety_watchdog_ms;      /**< Safety task watchdog             */
    bool            safety_signature_enabled;/**< STS verified at start            */
    bool            safety_locked;           /**< Safety locked (no edits)         */
    bool            safety_partner_present;  /**< Redundant safety processor       */
    uint32_t        max_safety_connections;  /**< Max safety CIP connections       */
    uint16_t        safety_io_slot_start;    /**< First safety I/O chassis slot     */
    uint16_t        safety_io_slot_end;      /**< Last safety I/O chassis slot      */
    bool            safety_output_test_enabled;/**< Pulse test on safety outputs    */
    double          pfd_avg_target;           /**< Target PFDavg (SIL-based)        */
    uint32_t        diagnostic_coverage;     /**< Diagnostic Coverage percentage    */
    double          mttf_hours;              /**< Mean Time To Failure (hours)      */
    double          proof_test_interval_hours;/**< Proof test interval (hours)       */
} plc_safety_config_t;

/* ============================================================================
 * L3: I/O Configuration ? Logix Device Tree
 * ============================================================================
 * The I/O configuration tree defines all connections to physical I/O modules.
 * In Logix 5000, this includes:
 *   - Local chassis modules (1756 I/O)
 *   - Remote chassis via EtherNet/IP (CIP network modules)
 *   - Point I/O (1734) via Ethernet/IP adapters
 *   - Flex I/O (1794) adapters
 *   - Third-party CIP devices (PROFIBUS PA, HART bridges, etc.)
 */

/** I/O module types */
typedef enum {
    IO_TYPE_DIGITAL_IN  = 0,    /**< Digital input module (1756-IB16, etc.)  */
    IO_TYPE_DIGITAL_OUT = 1,    /**< Digital output (1756-OB16, etc.)        */
    IO_TYPE_ANALOG_IN   = 2,    /**< Analog input (1756-IF8, etc.)           */
    IO_TYPE_ANALOG_OUT  = 3,    /**< Analog output (1756-OF8, etc.)          */
    IO_TYPE_SPECIALTY   = 4,    /**< HSC, thermocouple, RTD modules          */
    IO_TYPE_COMM_ETHIP  = 5,    /**< EtherNet/IP communication module        */
    IO_TYPE_COMM_DNET   = 6,    /**< DeviceNet scanner module                 */
    IO_TYPE_COMM_CNET   = 7,    /**< ControlNet communication module          */
    IO_TYPE_SAFETY_IN   = 8,    /**< Safety digital input                     */
    IO_TYPE_SAFETY_OUT  = 9,    /**< Safety digital output                    */
    IO_TYPE_MOTION_AXIS = 10    /**< Motion axis (Kinetix 5500/5700)         */
} plc_io_module_type_t;

/** I/O connection type (RPI-based or COS-based) */
typedef enum {
    IO_CONN_RPI_ONLY    = 0,     /**< Requested Packet Interval only          */
    IO_CONN_COS_INPUT   = 1,     /**< Change-of-State on input               */
    IO_CONN_COS_ALL     = 2      /**< COS on input and output                */
} plc_io_connection_type_t;

/** Single I/O module entry in the device tree */
typedef struct {
    char                    name[64];        /**< Module name (default: rack:slot) */
    plc_io_module_type_t    type;            /**< Module type                     */
    char                    catalog[32];     /**< e.g., "1756-IB16D"              */
    uint16_t                parent_bus;      /**< Communication parent module     */
    uint8_t                 slot;            /**< Chassis slot number (0-16)      */
    bool                    electronic_keying;/**< Exact/Compatible/Disabled       */
    float                   rpi_ms;          /**< Requested Packet Interval (ms)  */
    plc_io_connection_type_t conn_type;      /**< RPI / COS                      */
    bool                    unicast;         /**< Unicast vs multicast I/O        */
    uint32_t                input_size_bytes;/**< Input image size                 */
    uint32_t                output_size_bytes;/**< Output image size               */
    /* Configuration */
    bool                    inhibit_module;  /**< Connection inhibited            */
    bool                    major_fault_on_error;/**< Fault on connection loss    */
    /* Safety (GuardLogix) */
    bool                    is_safety_module;/**< SIL-rated module                */
    uint8_t                 safety_slot_partner;/**< Partner module slot for SIL3 */
    /* Diagnostics */
    bool                    enable_diagnostic_latch; /**< Latch diag bits          */
    uint16_t                input_filter_us; /**< Input filter (digital only)     */
} plc_io_module_t;

/* ============================================================================
 * L3: Communication Configuration ? EtherNet/IP
 * ============================================================================
 * EtherNet/IP is Rockwell's primary industrial protocol, built on
 * standard Ethernet with CIP at the application layer.
 *
 * Key parameters:
 *   - IP Address: Standard IPv4 address
 *   - Subnet Mask: Network partition
 *   - Gateway: Default route for inter-network traffic
 *   - CIP Connections: Logical connections for I/O and messaging
 *
 * Ref: CIP Specification Vol 1 (ODVA), Vol 2 (EtherNet/IP)
 */

/** Ethernet port configuration */
typedef struct {
    char            port_name[16];      /**< "A1", "A2", "B1" (front/rear)    */
    bool            enabled;            /**< Port active                       */
    bool            autonegotiate;      /**< Auto speed/duplex                 */
    uint16_t        speed_mbps;         /**< 10/100/1000 Mbps                  */
    bool            full_duplex;        /**< full-duplex = true                */
    char            ip_address[16];     /**< IPv4 address "192.168.1.10"       */
    char            subnet_mask[16];    /**< Subnet mask "255.255.255.0"       */
    char            gateway[16];        /**< Default gateway                    */
    bool            dhcp_enabled;       /**< DHCP client                       */
    bool            bootp_enabled;      /**< BOOTP client (legacy)             */
    char            hostname[64];       /**< DNS hostname                      */
    char            domain_name[128];   /**< DNS domain                        */
} plc_ethernet_port_t;

/** CIP protocol configuration */
typedef struct {
    uint16_t        max_ucmm_connections;/**< Max unconnected messages           */
    uint16_t        max_class3_connections;/**< Max connected explicit msg       */
    uint16_t        max_class1_connections;/**< Max implicit (I/O) connections   */
    bool            enable_cip_sync;       /**< CIP Sync for motion (IEEE 1588) */
    bool            enable_dlr;            /**< Device Level Ring protocol      */
    uint8_t         dlr_ring_rank;         /**< Supervisor/Backup/Normal node   */
    bool            enable_qos;            /**< Quality of Service (802.1Q)     */
    uint8_t         qos_dscp_io;           /**< DSCP value for I/O traffic      */
    uint8_t         qos_dscp_explicit;     /**< DSCP for explicit messaging     */
} plc_cip_config_t;

/* ============================================================================
 * L7: Industrial Applications ? FactoryTalk Integration
 * ============================================================================
 * FactoryTalk is Rockwell's software platform for industrial information:
 *   - FactoryTalk View SE/ME: HMI/SCADA
 *   - FactoryTalk Historian: Data historian (PI-compatible)
 *   - FactoryTalk AssetCentre: Version control & disaster recovery
 *   - FactoryTalk Security: Role-based access control
 *   - FactoryTalk Linx: Communication driver (RSLinx successor)
 */

/** FactoryTalk service configuration */
typedef struct {
    char            services_platform[64];/**< "FactoryTalk Services Platform"  */
    char            directory_server[128];/**< FactoryTalk Directory server     */
    char            realm_name[64];      /**< Security realm                   */
    uint16_t        tcp_port;            /**< FTSP TCP port (default: 44818)   */
    bool            integrated_security; /**< Use Windows-linked security      */
    bool            audit_logging;       /**< Enable audit trail               */
    char            historial_server[128];/**< Historian SE server              */
    /* AssetCentre */
    bool            enable_assetcentre;  /**< Version control integration      */
    char            assetcentre_server[128];
    uint32_t        backup_interval_hours;/**< Auto-backup frequency            */
} plc_factorytalk_config_t;

/* ============================================================================
 * L8: Advanced ? Controller Performance Model
 * ============================================================================
 * This structure models the real-time performance characteristics of a
 * Logix 5000 controller. Understanding these is critical for:
 *   - Task scheduling and watchdog configuration
 *   - Predicting scan time impact of AOI/UDT additions
 *   - Capacity planning for future expansion
 *
 * The performance model is based on empirical measurements from published
 * Rockwell performance benchmarks (1756-RM087) and community data.
 */

/** Controller performance metrics */
typedef struct {
    uint32_t    cpu_type;              /**< 0=ARM, 1=Intel, 2=Kintex FPGA     */
    uint32_t    cpu_clock_mhz;         /**< Processor clock frequency          */
    uint32_t    cpu_cores;             /**< Total cores (1-4 for L8x)          */
    uint32_t    ram_total_mb;          /**< Total RAM installed                */
    uint32_t    ram_user_mb;           /**< User-accessible RAM                */
    uint32_t    nvram_kb;              /**< Non-volatile SRAM                  */
    /* Scan performance */
    double      dint_add_ns;           /**< Time for ADD DINT DINT DINT (ns)   */
    double      dint_mul_ns;           /**< Time for MUL DINT DINT DINT (ns)   */
    double      branch_ns;             /**< Time per ladder branch (ns)        */
    double      timer_ns;              /**< Time for TON instruction (ns)      */
    double      pid_ns;                /**< Time for PID instruction (ns)      */
    double      aoi_overhead_ns;       /**< AOI call/return overhead (ns)      */
    double      message_ns;            /**< MSG instruction overhead (ns)      */
    double      interrupt_latency_ns;  /**< Max interrupt latency (ns)         */
    /* Memory performance */
    double      tag_read_ns;           /**< Controller-scope tag read (ns)     */
    double      program_tag_read_ns;   /**< Program-scope tag read (ns)       */
    double      array_index_ns;        /**< Array element access (ns)          */
    double      udt_member_access_ns;  /**< UDT member access (ns)             */
    /* Throughput */
    uint32_t    max_packets_per_scan;  /**< Max CIP packets per scan           */
    uint32_t    max_io_connections;    /**< Max implicit I/O connections       */
    uint32_t    max_produced_tags;     /**< Max produced tags                  */
    uint32_t    max_consumed_tags;     /**< Max consumed tags                  */
} plc_performance_model_t;

/* ============================================================================
 * Core API: Controller Configuration
 * ============================================================================ */

plc_controller_config_t* plc_config_create(const char* project_name,
                                            plc_platform_t platform,
                                            const char* model);
void plc_config_free(plc_controller_config_t* config);

bool plc_config_set_firmware(plc_controller_config_t* config,
                              uint32_t major, uint32_t minor, uint32_t build);

bool plc_config_set_redundancy(plc_controller_config_t* config,
                                plc_redundancy_mode_t mode, uint16_t secondary_slot);

bool plc_config_validate(const plc_controller_config_t* config,
                          char* error_buffer, uint32_t buffer_size);

/* ============================================================================
 * Core API: Task Management
 * ============================================================================ */

plc_task_config_t* plc_task_create(const char* name, plc_task_type_t type,
                                    uint8_t priority, uint32_t period_us);
void plc_task_free(plc_task_config_t* task);

bool plc_task_add_program(plc_task_config_t* task, const char* program_name);
bool plc_task_remove_program(plc_task_config_t* task, const char* program_name);

uint32_t plc_task_estimate_scan_time(const plc_task_config_t* task,
                                      const plc_performance_model_t* perf,
                                      uint32_t rung_count_per_program);

/* ============================================================================
 * Core API: Program Management
 * ============================================================================ */

plc_program_config_t* plc_program_create(const char* name, const char* description);
void plc_program_free(plc_program_config_t* program);
bool plc_program_set_main_routine(plc_program_config_t* program, const char* routine_name);

/* ============================================================================
 * Core API: I/O Configuration
 * ============================================================================ */

plc_io_module_t* plc_io_module_create(const char* name, plc_io_module_type_t type,
                                       const char* catalog, uint8_t slot);
void plc_io_module_free(plc_io_module_t* module);

bool plc_io_calculate_image_size(const plc_io_module_t* module,
                                  uint32_t* input_size, uint32_t* output_size);

/* ============================================================================
 * Core API: EtherNet/IP Configuration
 * ============================================================================ */

plc_ethernet_port_t* plc_eth_port_create(const char* port_name);
void plc_eth_port_free(plc_ethernet_port_t* port);

bool plc_eth_port_set_ip(plc_ethernet_port_t* port,
                          const char* ip, const char* mask, const char* gateway);

bool plc_eth_port_validate_ip(const plc_ethernet_port_t* port);

/* ============================================================================
 * Core API: Safety Configuration
 * ============================================================================ */

bool plc_safety_config_validate(const plc_safety_config_t* safety,
                                 const plc_controller_config_t* config,
                                 char* error_buffer, uint32_t buffer_size);

double plc_safety_calculate_pfd(const plc_safety_config_t* safety,
                                 double mttf_hours, double proof_test_interval_hours);

/* ============================================================================
 * Core API: Performance Model
 * ============================================================================ */

const plc_performance_model_t* plc_performance_lookup(const char* model_number);

uint32_t plc_estimate_scan_time(const plc_performance_model_t* perf,
                                 uint32_t rung_count, uint32_t aoi_count,
                                 uint32_t pid_count, uint32_t msg_count);

#ifdef __cplusplus
}
#endif

#endif /* PLC_ROCKWELL_CONFIG_H */
