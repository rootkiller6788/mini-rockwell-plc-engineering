/**
 * @file    control_logix_platform.h
 * @brief   L1-L3: ControlLogix 1756 Platform Architecture
 *
 * Knowledge Mapping:
 *   L1 Definitions:  Chassis types, slot addressing, module classifications,
 *                    ControlLogix vs CompactLogix, redundancy architectures
 *   L3 Structures:   Backplane topology, slot numbering, module adjacency,
 *                    power supply budget calculation
 *   L7 Applications: Rockwell 1756 ControlLogix platform
 *
 * Reference: Rockwell Automation Publication 1756-UM001
 *   "ControlLogix System User Manual"
 *
 * Course Alignment:
 *   RWTH Aachen - Industrial Control Systems: PLC hardware architectures
 *   Purdue ECE 602 - Lumped Systems: electrical bus modeling
 *   ISA/IEC - IEC 61131-2: PLC hardware requirements
 */

#ifndef CONTROL_LOGIX_PLATFORM_H
#define CONTROL_LOGIX_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Chassis Types and Sizes (ControlLogix 1756 Series)
 * ======================================================================== */

typedef enum {
    CHASSIS_4_SLOT  = 4,   /* 1756-A4  */
    CHASSIS_7_SLOT  = 7,   /* 1756-A7  */
    CHASSIS_10_SLOT = 10,  /* 1756-A10 */
    CHASSIS_13_SLOT = 13,  /* 1756-A13 */
    CHASSIS_17_SLOT = 17   /* 1756-A17 */
} logix_chassis_size_t;

typedef enum {
    CHASSIS_TYPE_STANDARD,      /* Standard passive backplane */
    CHASSIS_TYPE_EXTENDED_TEMP, /* -25 to +70 deg C operating range */
    CHASSIS_TYPE_CONFORMAL,     /* Conformal coating for harsh environments */
    CHASSIS_TYPE_REDUNDANT      /* Redundancy-capable chassis (1756-AxxR) */
} logix_chassis_type_t;

/**
 * @brief L1: Slot addressing and electrical position
 *
 * ControlLogix uses a physical slot numbering scheme where slot 0 is always
 * the leftmost slot. Controllers typically occupy slot 0 for single-controller
 * systems, though any slot can hold a controller in multi-controller chassis.
 *
 * The backplane operates at 3.3V/5V with specific current limits per slot:
 *   - Max 5V current per module: 1.5 A (high-power slots), 0.32 A (standard)
 *   - Max 3.3V current per module: 2.0 A (high-power slots)
 *   - Total chassis power budget: depends on power supply (PA2/PA4/PA75 etc.)
 */
typedef struct {
    uint8_t  slot_number;       /* 0 .. chassis_size-1              */
    bool     is_high_power;     /* High-power slot capability       */
    bool     is_occupied;       /* Module present in this slot      */
    uint32_t module_product_code; /* 16-bit Rockwell product code   */
    uint8_t  module_series;     /* HW revision letter (A=1, B=2..) */
    uint8_t  module_revision_major;
    uint8_t  module_revision_minor;
    double   current_draw_5v;   /* Amperes at 5V                    */
    double   current_draw_3v3;  /* Amperes at 3.3V                 */
    double   current_draw_24v;  /* Amperes at 24V (if applicable)   */
} logix_slot_t;

/**
 * @brief L3: Power supply definition
 *
 * ControlLogix power supplies provide 1.2V, 3.3V, 5V, and 24V DC outputs
 * to the backplane. The system must verify that total current draw across
 * all modules does not exceed the power supply rating.
 *
 * Common supplies:
 *   1756-PA72: 85-265VAC input, 5V@10A, 3.3V@4A, 24V@2.8A
 *   1756-PA75: 85-265VAC input, 5V@13A, 3.3V@4A, 24V@2.8A
 *   1756-PB75: 24VDC input,     5V@13A, 3.3V@4A, 24V@2.8A
 */
typedef struct {
    uint32_t model_number;      /* e.g., 1756-PA72 encoded as product code */
    double   rated_5v_amps;     /* Maximum current at 5V                   */
    double   rated_3v3_amps;    /* Maximum current at 3.3V                 */
    double   rated_24v_amps;    /* Maximum current at 24V                  */
    double   total_power_watts; /* Combined power rating                   */
    double   used_5v_amps;      /* Current consumption from all modules    */
    double   used_3v3_amps;
    double   used_24v_amps;
} logix_power_supply_t;

/* ========================================================================
 * L1: Controller Family Definitions
 * ======================================================================== */

typedef enum {
    CTLR_1756_L61  = 0x0036,  /* ControlLogix 5561                      */
    CTLR_1756_L62  = 0x0037,  /* ControlLogix 5562 (with SIL2 safety)   */
    CTLR_1756_L63  = 0x0038,  /* ControlLogix 5563                      */
    CTLR_1756_L64  = 0x0039,  /* ControlLogix 5564                      */
    CTLR_1756_L71  = 0x0041,  /* ControlLogix 5571 (dual-core)          */
    CTLR_1756_L72  = 0x0042,  /* ControlLogix 5572                      */
    CTLR_1756_L73  = 0x0043,  /* ControlLogix 5573                      */
    CTLR_1756_L74  = 0x0044,  /* ControlLogix 5574                      */
    CTLR_1756_L75  = 0x0045,  /* ControlLogix 5575 (SIL2)               */
    CTLR_1756_L81E = 0x0046,  /* ControlLogix 5580 with 1G EtherNet/IP  */
    CTLR_1756_L82E = 0x0047,  /* ControlLogix 5580 with DLR             */
    CTLR_1756_L83E = 0x0048,  /* ControlLogix 5580                      */
    CTLR_1756_L84E = 0x0049,  /* ControlLogix 5580 with SIL2            */
    CTLR_1756_L85E = 0x004A,  /* ControlLogix 5580 with SIL3            */
    CTLR_1769_L16ER= 0x0050,  /* CompactLogix 5370 L1 (16 DI, 16 DO)   */
    CTLR_1769_L18ER= 0x0051,  /* CompactLogix 5370 L1                  */
    CTLR_1769_L24ER= 0x0052,  /* CompactLogix 5370 L2                  */
    CTLR_1769_L27ERM=0x0053,  /* CompactLogix 5370 L2 with motion      */
    CTLR_1769_L30ER= 0x0054,  /* CompactLogix 5370 L3                  */
    CTLR_1769_L33ER= 0x0055,  /* CompactLogix 5370 L3 with motion      */
    CTLR_1769_L36ERM=0x0056,  /* CompactLogix 5370 L3 max              */
    CTLR_5069_L306ER=0x0060,  /* CompactLogix 5380 series              */
    CTLR_5069_L310ER=0x0061,
    CTLR_5069_L320ER=0x0062,
    CTLR_5069_L330ER=0x0063,
    CTLR_5069_L340ER=0x0064,
} logix_controller_type_t;

/**
 * @brief L1: Controller status and diagnostics
 *
 * ControlLogix controllers maintain a status word accessible via GSV
 * (Get System Value) instruction. Key status bits indicate:
 *   - Run/Program/Test/Fault mode
 *   - Battery low condition
 *   - Memory usage
 *   - I/O force status
 */
typedef struct {
    logix_controller_type_t type;
    uint32_t firmware_major;
    uint32_t firmware_minor;
    uint32_t serial_number;
    char     product_name[32];    /* e.g., "1756-L81E" */
    uint32_t total_memory_kb;
    uint32_t used_memory_kb;
    uint32_t free_memory_kb;
    bool     battery_low;
    bool     battery_present;
    bool     forces_enabled;
    bool     forces_installed;
    bool     io_ok;
    bool     redundancy_enabled;
    bool     safety_enabled;       /* SIL rated controller */
    uint32_t safety_sil_level;    /* 2 or 3 */
    uint32_t ethernet_ports;
    bool     dlr_supported;        /* Device Level Ring */
} logix_controller_t;

/* ========================================================================
 * L3: Chassis and Backplane Management
 * ======================================================================== */

/**
 * @brief Complete chassis representation
 *
 * A ControlLogix chassis consists of a passive backplane with slots
 * for modules, one or more power supplies, and optional redundancy.
 *
 * The backplane uses a high-speed serial bus (ControlBus) operating at
 * 100 Mbps for 1756-L7x/L8x series, carrying:
 *   - Produced/consumed tag data between controllers
 *   - I/O module data updates at RPI (Requested Packet Interval)
 *   - Module configuration and status
 *   - Motion data for CIP Motion
 */
typedef struct {
    logix_chassis_size_t size;
    logix_chassis_type_t type;
    logix_slot_t         slots[17];      /* Max 17 slots */
    logix_power_supply_t power_supply;
    bool                 redundancy_capable;
    uint8_t              controller_slot; /* Which slot has the CPU */
    uint8_t              partner_slot;    /* Redundant partner slot */
    double               backplane_speed_mbps; /* 100 for L7x/L8x */
} logix_chassis_t;

/**
 * @brief L3: I/O module classifications
 *
 * ControlLogix I/O modules are categorized by function:
 *   - Digital Input  (1756-IBxx, 1756-IVxx, 1756-IAxx)
 *   - Digital Output (1756-OBxx, 1756-OVxx, 1756-OAxx)
 *   - Analog Input   (1756-IFxx, 1756-IRxx, 1756-ITxx)
 *   - Analog Output  (1756-OFxx)
 *   - Specialty      (1756-HSC, 1756-HYD, 1756-M02AE, etc.)
 *
 * Each I/O module defines its own connection type and RPI constraints:
 *   - Module RPI range: typically 0.2ms to 750ms
 *   - COS (Change of State): digital inputs can trigger on change
 *   - Multicast vs Unicast connection types
 *   - Module compatibility requires matching firmware minor revision
 */
typedef enum {
    IO_MODULE_DIGITAL_INPUT   = 0x01,
    IO_MODULE_DIGITAL_OUTPUT  = 0x02,
    IO_MODULE_ANALOG_INPUT    = 0x03,
    IO_MODULE_ANALOG_OUTPUT   = 0x04,
    IO_MODULE_COMMUNICATION   = 0x05,  /* EN2T, EN3TR, CN2R, etc. */
    IO_MODULE_MOTION          = 0x06,  /* 1756-M02AE, M08SE, etc. */
    IO_MODULE_SPECIALTY       = 0x07,  /* HSC, flow meter, weigh scale */
    IO_MODULE_SAFETY          = 0x08,  /* 1756-IB8S, OB8S, etc. */
} logix_io_module_class_t;

/* ========================================================================
 * API: Platform Management
 * ======================================================================== */

/**
 * @brief Initialize a chassis with specified size and type
 * @param chassis      Pointer to chassis structure
 * @param size         Number of slots (4,7,10,13,17)
 * @param type         Chassis type (standard, extended, etc.)
 * @return true on success, false if size exceeds maximum
 *
 * Complexity: O(n) where n = chassis size (slot initialization)
 */
bool logix_chassis_init(logix_chassis_t *chassis,
                         logix_chassis_size_t size,
                         logix_chassis_type_t type);

/**
 * @brief Calculate total power consumption and verify against budget
 *
 * Iterates all occupied slots and sums current draw at each voltage rail.
 * Compares against power supply ratings.
 *
 * @param chassis  Initialized chassis structure
 * @return true if all voltage rails within power supply budget
 *
 * Complexity: O(n) where n = chassis size
 * Theorem: Sum of module currents <= rated current for each rail
 * Reference: 1756-TD003 Power Supplies Technical Data
 */
bool logix_chassis_verify_power_budget(const logix_chassis_t *chassis);

/**
 * @brief Populate a slot with a module definition
 * @param chassis          Target chassis
 * @param slot_number      Slot index (0-based)
 * @param product_code     Rockwell product code
 * @param draw_5v          Current draw at 5V (amperes)
 * @param draw_3v3         Current draw at 3.3V (amperes)
 * @param draw_24v         Current draw at 24V (amperes)
 * @return true on success
 *
 * Complexity: O(1)
 * Reference: 1756-SG001 Selection Guide
 */
bool logix_chassis_install_module(logix_chassis_t *chassis,
                                   uint8_t slot_number,
                                   uint32_t product_code,
                                   double draw_5v,
                                   double draw_3v3,
                                   double draw_24v);

/**
 * @brief Remove a module from a slot
 * Complexity: O(1)
 */
bool logix_chassis_remove_module(logix_chassis_t *chassis, uint8_t slot_number);

/**
 * @brief Get the controller slot number (typically slot 0)
 * Complexity: O(1)
 */
uint8_t logix_chassis_get_controller_slot(const logix_chassis_t *chassis);

/**
 * @brief Calculate total slot count minus empty slots
 * Complexity: O(n)
 */
uint8_t logix_chassis_occupied_slot_count(const logix_chassis_t *chassis);

/**
 * @brief Check if chassis supports redundancy (redundancy-capable chassis + L7x/L8x controller)
 * @return true if redundant controller pair can be established
 *
 * Reference: 1756-UM535 ControlLogix Redundancy User Manual
 *   - Requires 1756-A7R, A10R, or A17R chassis
 *   - Requires identical firmware revision between partners
 *   - Requires 1756-RM2 redundancy module
 */
bool logix_chassis_is_redundancy_capable(const logix_chassis_t *chassis);

/**
 * @brief Compute remaining power budget for each rail
 * @param chassis           Initialized chassis
 * @param rem_5v[out]       Remaining 5V capacity (amperes)
 * @param rem_3v3[out]      Remaining 3.3V capacity (amperes)
 * @param rem_24v[out]      Remaining 24V capacity (amperes)
 *
 * Complexity: O(n)
 * Theorem: remaining[i] = rated[i] - Σ consumed[i][j] for all occupied slots j
 */
void logix_chassis_power_remaining(const logix_chassis_t *chassis,
                                    double *rem_5v,
                                    double *rem_3v3,
                                    double *rem_24v);

/**
 * @brief Map a controller type enum to its human-readable name
 * Complexity: O(1)
 */
const char *logix_controller_type_name(logix_controller_type_t type);

/**
 * @brief Get firmware compatibility range for a controller model
 * @param type         Controller type
 * @param min_major[out] Minimum supported firmware major version
 * @param min_minor[out] Minimum supported firmware minor version
 * @param max_major[out] Maximum supported firmware major version
 * @param max_minor[out] Maximum supported firmware minor version
 * Complexity: O(1)
 */
void logix_controller_firmware_range(logix_controller_type_t type,
                                      uint32_t *min_major, uint32_t *min_minor,
                                      uint32_t *max_major, uint32_t *max_minor);

#endif /* CONTROL_LOGIX_PLATFORM_H */
