/**
 * @file pointio_types.h
 * @brief Core type definitions for Rockwell CompactLogix Point I/O (1734 series).
 *
 * Level: L1 Definitions
 * Reference:
 *   - Rockwell Automation, "1734 POINT I/O Installation Instructions" (1734-IN001)
 *   - Rockwell Automation, "CompactLogix 5370 Controllers User Manual" (1769-UM021)
 *   - ODVA, "The Common Industrial Protocol (CIP) Specification" Vol.1, Ch.2
 *
 * This header defines all data types, enumerations, and constants used across
 * the CompactLogix Point I/O module implementation.
 */

#ifndef POINTIO_TYPES_H
#define POINTIO_TYPES_H

#include <stdint.h>
#include <stddef.h>

/*===========================================================================
 * L1: Point I/O Module Family Definitions
 *===========================================================================*/

/** Maximum number of I/O modules per PointBus backplane */
#define POINTIO_MAX_MODULES         63

/** Maximum data size per module (bytes) */
#define POINTIO_MAX_DATA_SIZE       128

/** Maximum channels per module */
#define POINTIO_MAX_CHANNELS        32

/** Point I/O module physical dimensions (mm) */
#define POINTIO_MODULE_WIDTH_MM     12.0
#define POINTIO_MODULE_HEIGHT_MM    65.0
#define POINTIO_MODULE_DEPTH_MM     75.0

/** PointBus backplane voltage (VDC) */
#define POINTIO_BACKPLANE_VOLTAGE   5.0

/** Maximum PointBus backplane current (mA) */
#define POINTIO_BACKPLANE_MAX_MA    1000

/** Default RPI (Requested Packet Interval) in milliseconds */
#define POINTIO_DEFAULT_RPI_MS      20

/** Minimum RPI allowed */
#define POINTIO_MIN_RPI_MS          1

/** Maximum RPI allowed */
#define POINTIO_MAX_RPI_MS          750

/** Connection timeout multiplier (RPI * multiplier) */
#define POINTIO_TIMEOUT_MULTIPLIER  4

/*===========================================================================
 * L1: Module Catalog Numbers
 *===========================================================================*/

/** Point I/O adapter catalog number */
#define POINTIO_CAT_AENT        "1734-AENT"
#define POINTIO_CAT_AENTR       "1734-AENTR"

/** Digital input modules */
#define POINTIO_CAT_IB2         "1734-IB2"
#define POINTIO_CAT_IB4         "1734-IB4"
#define POINTIO_CAT_IB8         "1734-IB8"
#define POINTIO_CAT_IB8S        "1734-IB8S"
#define POINTIO_CAT_IB16        "1734-IB16"

/** Digital output modules */
#define POINTIO_CAT_OB2         "1734-OB2"
#define POINTIO_CAT_OB4         "1734-OB4"
#define POINTIO_CAT_OB8         "1734-OB8"
#define POINTIO_CAT_OB8E        "1734-OB8E"
#define POINTIO_CAT_OB16        "1734-OB16"

/** Analog input modules */
#define POINTIO_CAT_IE2C        "1734-IE2C"
#define POINTIO_CAT_IE4C        "1734-IE4C"
#define POINTIO_CAT_IE8C        "1734-IE8C"
#define POINTIO_CAT_IE2V        "1734-IE2V"
#define POINTIO_CAT_IRT2        "1734-IR2"

/** Analog output modules */
#define POINTIO_CAT_OE2C        "1734-OE2C"
#define POINTIO_CAT_OE4C        "1734-OE4C"
#define POINTIO_CAT_OE2V        "1734-OE2V"

/** Specialty modules */
#define POINTIO_CAT_VHSC24      "1734-VHSC24"
#define POINTIO_CAT_VHSC5       "1734-VHSC5"
#define POINTIO_CAT_IJ2         "1734-IJ2"
#define POINTIO_CAT_IK2         "1734-IK2"
#define POINTIO_CAT_232ASC      "1734-232ASC"

/** Safety I/O modules */
#define POINTIO_CAT_IB8S_SAFETY "1734-IB8SK"
#define POINTIO_CAT_OB8S_SAFETY "1734-OB8SK"

/*===========================================================================
 * L1: Module Type Classification
 *===========================================================================*/

/**
 * @brief Point I/O module type classification.
 *
 * Each 1734 module falls into one of these functional categories.
 * Reference: 1734-SG001, Point I/O Selection Guide
 */
typedef enum {
    POINTIO_TYPE_UNKNOWN            = 0,
    POINTIO_TYPE_ADAPTER            = 1,   /* 1734-AENT, -AENTR */
    POINTIO_TYPE_DIGITAL_INPUT      = 2,   /* 1734-IBx */
    POINTIO_TYPE_DIGITAL_OUTPUT     = 3,   /* 1734-OBx */
    POINTIO_TYPE_DIGITAL_COMBO      = 4,   /* 1734-OBx/IBx combo */
    POINTIO_TYPE_ANALOG_INPUT       = 5,   /* 1734-IEx, IRx */
    POINTIO_TYPE_ANALOG_OUTPUT      = 6,   /* 1734-OEx */
    POINTIO_TYPE_SPECIALTY          = 7,   /* VHSC, IJ, IK, 232ASC */
    POINTIO_TYPE_SAFETY_INPUT       = 8,   /* 1734-IBxS */
    POINTIO_TYPE_SAFETY_OUTPUT      = 9,   /* 1734-OBxS */
    POINTIO_TYPE_TERMINAL_BASE      = 10   /* 1734-TB, -TBS */
} pointio_module_type_t;

/**
 * @brief Digital input filter selection.
 */
typedef enum {
    POINTIO_FILTER_OFF              = 0,
    POINTIO_FILTER_0_25MS           = 1,
    POINTIO_FILTER_0_5MS            = 2,
    POINTIO_FILTER_1MS              = 3,
    POINTIO_FILTER_2MS              = 4,
    POINTIO_FILTER_4MS              = 5,
    POINTIO_FILTER_8MS              = 6,
    POINTIO_FILTER_16MS             = 7,
    POINTIO_FILTER_32MS             = 8
} pointio_input_filter_t;

/**
 * @brief Analog input range selection.
 */
typedef enum {
    POINTIO_AI_RANGE_0_20MA         = 0,
    POINTIO_AI_RANGE_4_20MA         = 1,
    POINTIO_AI_RANGE_0_10V          = 2,
    POINTIO_AI_RANGE_N10_10V        = 3,
    POINTIO_AI_RANGE_0_5V           = 4,
    POINTIO_AI_RANGE_N5_5V          = 5,
    POINTIO_AI_RANGE_THERMOCOUPLE_J = 6,
    POINTIO_AI_RANGE_THERMOCOUPLE_K = 7,
    POINTIO_AI_RANGE_RTD_100_PT385  = 8,
    POINTIO_AI_RANGE_RTD_100_PT392  = 9
} pointio_analog_range_t;

/**
 * @brief Analog output range selection.
 */
typedef enum {
    POINTIO_AO_RANGE_0_20MA         = 0,
    POINTIO_AO_RANGE_4_20MA         = 1,
    POINTIO_AO_RANGE_0_10V          = 2,
    POINTIO_AO_RANGE_N10_10V        = 3
} pointio_analog_output_range_t;

/**
 * @brief Analog data format for engineering units.
 */
typedef enum {
    POINTIO_DATA_FORMAT_RAW         = 0,
    POINTIO_DATA_FORMAT_PERCENT     = 1,
    POINTIO_DATA_FORMAT_EU          = 2,
    POINTIO_DATA_FORMAT_SCALED_PID  = 3,
    POINTIO_DATA_FORMAT_FLOAT_ENG   = 4
} pointio_data_format_t;

/*===========================================================================
 * L1: Connection Types (CIP)
 *===========================================================================*/

/**
 * @brief CIP I/O connection type.
 * Reference: ODVA CIP Specification Vol.1, Section 3-5
 */
typedef enum {
    POINTIO_CONN_NONE               = 0,
    POINTIO_CONN_EXCLUSIVE_OWNER    = 1,
    POINTIO_CONN_INPUT_ONLY         = 2,
    POINTIO_CONN_LISTEN_ONLY        = 3,
    POINTIO_CONN_RACK_OPTIMIZED     = 4
} pointio_connection_type_t;

/**
 * @brief CIP connection state machine.
 */
typedef enum {
    POINTIO_CONN_STATE_IDLE         = 0,
    POINTIO_CONN_STATE_CONNECTING   = 1,
    POINTIO_CONN_STATE_RUNNING      = 2,
    POINTIO_CONN_STATE_TIMED_OUT    = 3,
    POINTIO_CONN_STATE_FAULTED      = 4,
    POINTIO_CONN_STATE_SHUTDOWN     = 5
} pointio_connection_state_t;

/**
 * @brief CIP transport class trigger.
 */
typedef enum {
    POINTIO_TRIGGER_CYCLIC          = 0,
    POINTIO_TRIGGER_COS             = 1,
    POINTIO_TRIGGER_APPLICATION     = 2
} pointio_trigger_t;

/*===========================================================================
 * L1: Module Status & Fault Codes
 *===========================================================================*/

/**
 * @brief Module operating state.
 */
typedef enum {
    POINTIO_STATUS_UNKNOWN          = -1,
    POINTIO_STATUS_OK               = 0,
    POINTIO_STATUS_CONNECTING       = 1,
    POINTIO_STATUS_FAULTED          = 2,
    POINTIO_STATUS_MAJOR_FAULT      = 3,
    POINTIO_STATUS_INHIBITED        = 4,
    POINTIO_STATUS_STANDBY          = 5,
    POINTIO_STATUS_NOT_PRESENT      = 6,
    POINTIO_STATUS_WRONG_MODULE     = 7
} pointio_module_status_t;

/**
 * @brief Point I/O fault codes (from 1734-UM001).
 */
typedef enum {
    POINTIO_FAULT_NONE                      = 0x0000,
    POINTIO_FAULT_CONN_TIMEOUT              = 0x0001,
    POINTIO_FAULT_MODULE_MISSING            = 0x0002,
    POINTIO_FAULT_CONFIG_MISMATCH           = 0x0003,
    POINTIO_FAULT_COMM_ERROR                = 0x0004,
    POINTIO_FAULT_FIELD_POWER_LOST          = 0x0005,
    POINTIO_FAULT_BACKPLANE_ERROR           = 0x0006,
    POINTIO_FAULT_OVERTEMP                  = 0x0007,
    POINTIO_FAULT_OVERVOLTAGE               = 0x0008,
    POINTIO_FAULT_UNDERVOLTAGE              = 0x0009,
    POINTIO_FAULT_SHORT_CIRCUIT             = 0x000A,
    POINTIO_FAULT_OPEN_WIRE                 = 0x000B,
    POINTIO_FAULT_OUT_OF_RANGE              = 0x000C,
    POINTIO_FAULT_CALIBRATION_LOST          = 0x000D,
    POINTIO_FAULT_FIRMWARE_UPDATE_REQUIRED  = 0x000E,
    POINTIO_FAULT_INTERNAL_ERROR            = 0x000F,
    POINTIO_FAULT_WATCHDOG_TIMEOUT          = 0x0010
} pointio_fault_code_t;

/*===========================================================================
 * L1: Time Synchronization
 *===========================================================================*/

typedef enum {
    POINTIO_TIME_SRC_NONE           = 0,
    POINTIO_TIME_SRC_1588_PTP       = 1,
    POINTIO_TIME_SRC_NTP            = 2,
    POINTIO_TIME_SRC_CIP_SYNC       = 3,
    POINTIO_TIME_SRC_CONTROLLER     = 4
} pointio_time_source_t;

/*===========================================================================
 * L1: Output Behavior on Fault/Idle
 *===========================================================================*/

typedef enum {
    POINTIO_FAULT_MODE_ZERO         = 0,
    POINTIO_FAULT_MODE_HOLD_LAST    = 1,
    POINTIO_FAULT_MODE_PREDEFINED   = 2
} pointio_fault_mode_t;

typedef enum {
    POINTIO_PROG_MODE_ZERO          = 0,
    POINTIO_PROG_MODE_HOLD_LAST     = 1,
    POINTIO_PROG_MODE_PREDEFINED    = 2
} pointio_prog_mode_t;

/*===========================================================================
 * L3: Module Identity & Position
 *===========================================================================*/

typedef struct {
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint16_t major_revision;
    uint16_t minor_revision;
    uint16_t serial_number;
    char     product_name[33];
    char     catalog_number[16];
    uint8_t  mac_address[6];
} pointio_identity_t;

typedef struct {
    uint8_t  slot;
    uint8_t  rack;
    uint8_t  group;
    uint8_t  parent_adapter;
} pointio_slot_t;

typedef enum {
    POINTIO_KEYING_EXACT_MATCH       = 0,
    POINTIO_KEYING_COMPATIBLE        = 1,
    POINTIO_KEYING_DISABLE           = 2,
    POINTIO_KEYING_CUSTOM            = 3
} pointio_keying_type_t;

typedef struct {
    pointio_keying_type_t keying_type;
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint16_t major_revision;
    uint16_t minor_revision_min;
    uint16_t minor_revision_max;
} pointio_keying_t;

/*===========================================================================
 * L3: Analog Scaling Configuration
 *===========================================================================*/

typedef struct {
    double  eu_low;
    double  eu_high;
    double  raw_low;
    double  raw_high;
    pointio_data_format_t format;
    pointio_analog_range_t range;
    char    eu_label[16];
} pointio_analog_scaling_t;

/*===========================================================================
 * L3: Power Budget
 *===========================================================================*/

typedef struct {
    double backplane_5v_ma;
    double backplane_24v_ma;
    double field_power_24v_ma;
    double total_power_w;
} pointio_power_consumption_t;

typedef struct {
    double backplane_5v_available_ma;
    double backplane_5v_used_ma;
    double backplane_24v_available_ma;
    double backplane_24v_used_ma;
    double field_power_available_ma;
    double field_power_used_ma;
    double total_power_available_w;
    double total_power_used_w;
    int    overloaded;
} pointio_power_budget_t;

/*===========================================================================
 * L3: Connection Error Statistics
 *===========================================================================*/

typedef struct {
    uint32_t total_packets_sent;
    uint32_t total_packets_received;
    uint32_t packets_lost;
    uint32_t connection_timeouts;
    uint32_t connection_opens;
    uint32_t connection_rejects;
    uint32_t cst_errors;
    double   avg_rtt_ms;
    double   max_rtt_ms;
    double   packet_loss_rate;
} pointio_conn_stats_t;

/*===========================================================================
 * L4: Safety Integrity Parameters
 *===========================================================================*/

typedef struct {
    int     is_safety_module;
    int     sil_level;
    int     performance_level;
    double  pfd_avg;
    double  pfh;
    int     hardware_fault_tolerance;
    double  proof_test_interval_h;
    int     diagnostic_coverage_percent;
} pointio_safety_params_t;

/*===========================================================================
 * L5: Sequence-of-Events Data
 *===========================================================================*/

typedef struct {
    uint64_t timestamp_us;
    uint8_t  slot;
    uint8_t  channel;
    uint16_t value;
    uint8_t  event_type;
    uint8_t  quality;
} pointio_soe_entry_t;

/*===========================================================================
 * L7: CompactLogix CPU Models
 *===========================================================================*/

typedef enum {
    CPU_MODEL_UNKNOWN       = 0,
    CPU_1769_L16ER_BB1B     = 1,
    CPU_1769_L18ER_BB1B     = 2,
    CPU_1769_L18ERM_BB1B    = 3,
    CPU_1769_L24ER_QB1B     = 4,
    CPU_1769_L27ERM_QBFC1B  = 5,
    CPU_1769_L30ER          = 6,
    CPU_1769_L33ER          = 7,
    CPU_1769_L36ERM         = 8
} compactlogix_cpu_model_t;

typedef struct {
    compactlogix_cpu_model_t model;
    uint32_t user_memory_kb;
    uint32_t i_o_memory_kb;
    uint32_t max_local_modules;
    uint32_t max_ethernet_nodes;
    uint32_t max_motion_axes;
    int      has_integrated_motion;
    int      has_dual_ethernet;
    int      has_integrated_io;
    int      integrated_di_count;
    int      integrated_do_count;
    int      supports_safety;
    char     firmware_revision[16];
} compactlogix_cpu_t;

#endif /* POINTIO_TYPES_H */
