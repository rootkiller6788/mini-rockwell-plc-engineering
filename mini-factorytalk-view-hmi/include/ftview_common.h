/**
 * @file ftview_common.h
 * @brief FactoryTalk View HMI -- Common Definitions (L1: Definitions / L4: ISA-101)
 *
 * Implements knowledge points:
 *   L1.1 -- HMI data types (analog, digital, string) with engineering units
 *   L1.2 -- ISA-101 HMI hierarchy levels (1-4): Level 1 Process Area,
 *          Level 2 Process Unit, Level 3 Equipment Module, Level 4 Control Module
 *   L1.3 -- HMI quality-of-service flags (Good, Bad, Uncertain per OPC UA Part 8)
 *   L1.4 -- HMI object role types per ISA-101 Annex A
 *   L1.5 -- Display resolution classes and aspect ratio constants
 *   L2.1 -- Update rate and data freshness thresholds
 *   L4.1 -- ISA-101 high-performance HMI color palette (LOPA greyscale + alarm colours)
 *
 * Reference: ISA-101.01-2015 Human Machine Interfaces for Process Automation Systems
 *            OPC UA Part 8: Data Access
 */

#ifndef FTVIEW_COMMON_H
#define FTVIEW_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* =====================================================================
 * L1.1 -- HMI Tag Data Types
 *
 * Per ISA-101, HMI data elements fall into three primitive classes.
 * Each carries engineering unit metadata for display transformation.
 * ===================================================================== */

/** Tag value quality -- maps OPC UA StatusCode high byte */
typedef enum {
    FTVIEW_QUALITY_GOOD                = 0xC0,
    FTVIEW_QUALITY_GOOD_LOCAL_OVERRIDE = 0xD8,
    FTVIEW_QUALITY_UNCERTAIN           = 0x40,
    FTVIEW_QUALITY_UNCERTAIN_SUBST     = 0x50,
    FTVIEW_QUALITY_BAD                 = 0x00,
    FTVIEW_QUALITY_BAD_COMM_FAILURE    = 0x18,
    FTVIEW_QUALITY_BAD_CONFIG_ERROR    = 0x04,
    FTVIEW_QUALITY_NA                  = 0xFF
} ftview_quality_t;

/** Scalar value types for HMI tags */
typedef enum {
    FTVIEW_TYPE_ANALOG    = 0,
    FTVIEW_TYPE_DIGITAL   = 1,
    FTVIEW_TYPE_INTEGER   = 2,
    FTVIEW_TYPE_STRING    = 3,
    FTVIEW_TYPE_TIMESTAMP = 4
} ftview_type_t;

/** Engineering unit descriptor */
typedef struct {
    char     label[12];
    double   range_lo;
    double   range_hi;
    double   clamp_lo;
    double   clamp_hi;
    uint8_t  precision;
} ftview_eu_t;

/** Scalar tag value union with quality and timestamp */
typedef struct {
    ftview_type_t    type;
    ftview_quality_t quality;
    int64_t          timestamp_ms;
    union {
        double   analog;
        bool     digital;
        int32_t  integer;
        char     string[256];
    } data;
} ftview_value_t;

/* =====================================================================
 * L1.2 -- ISA-101 Hierarchy Levels
 *
 * ISA-101 defines a 4-level hierarchy for
 * situational-awareness-driven display design.
 * ===================================================================== */

typedef enum {
    ISA101_L1_PROCESS_AREA     = 1,
    ISA101_L2_PROCESS_UNIT     = 2,
    ISA101_L3_EQUIPMENT_MODULE = 3,
    ISA101_L4_CONTROL_MODULE   = 4
} isa101_level_t;

/** ISA-101 display archetype */
typedef enum {
    ISA101_DISPLAY_OVERVIEW       = 0,
    ISA101_DISPLAY_TREND          = 1,
    ISA101_DISPLAY_PID_FACEPLATE  = 2,
    ISA101_DISPLAY_ALARM_LIST     = 3,
    ISA101_DISPLAY_MOTOR_CONTROL  = 4,
    ISA101_DISPLAY_DIAGNOSTIC     = 5,
    ISA101_DISPLAY_RECIPE         = 6,
    ISA101_DISPLAY_SEQUENCE       = 7
} isa101_display_t;

/* =====================================================================
 * L1.5 -- Display resolution classes
 * ===================================================================== */

typedef enum {
    FTVIEW_RES_SVGA    = 0,
    FTVIEW_RES_XGA     = 1,
    FTVIEW_RES_SXGA    = 2,
    FTVIEW_RES_FHD     = 3,
    FTVIEW_RES_WUXGA   = 4
} ftview_resolution_t;

typedef struct {
    ftview_resolution_t res;
    uint16_t            width;
    uint16_t            height;
    double              aspect_ratio;
} ftview_display_geometry_t;

/* =====================================================================
 * L4.1 -- ISA-101 High-Performance HMI Color Palette
 *
 * ISA-101.01 S5.6: Use greyscale for process
 * values, colour ONLY for abnormal conditions.
 * ===================================================================== */

typedef struct { uint8_t r, g, b; } ftview_color_t;

#define ISA101_COLOR_BG_DARK      ((ftview_color_t){ 30,  30,  30 })
#define ISA101_COLOR_BG_MEDIUM    ((ftview_color_t){ 70,  70,  70 })
#define ISA101_COLOR_BG_LIGHT     ((ftview_color_t){100, 100, 100 })
#define ISA101_COLOR_STATIC_TEXT  ((ftview_color_t){200, 200, 200 })
#define ISA101_COLOR_PROCESS_LO   ((ftview_color_t){120, 180, 255 })
#define ISA101_COLOR_PROCESS_MID  ((ftview_color_t){200, 200, 200 })
#define ISA101_COLOR_PROCESS_HI   ((ftview_color_t){120, 180, 255 })
#define ISA101_COLOR_ALARM_HI     ((ftview_color_t){255,  60,  60 })
#define ISA101_COLOR_ALARM_HIHI   ((ftview_color_t){255,  30,  30 })
#define ISA101_COLOR_ALARM_LO     ((ftview_color_t){255, 255,  50 })
#define ISA101_COLOR_ALARM_LOLO   ((ftview_color_t){255, 215,   0 })
#define ISA101_COLOR_DEVIATION    ((ftview_color_t){255, 140,   0 })
#define ISA101_COLOR_CONFIRMED    ((ftview_color_t){  0, 200, 100 })

/* =====================================================================
 * L1.4 -- HMI Object Role Types (ISA-101 Annex A)
 * ===================================================================== */

typedef enum {
    FTVIEW_ROLE_PV           = 0,
    FTVIEW_ROLE_SP           = 1,
    FTVIEW_ROLE_OUT          = 2,
    FTVIEW_ROLE_ALARM_STATE  = 3,
    FTVIEW_ROLE_MODE         = 4,
    FTVIEW_ROLE_CMD          = 5,
    FTVIEW_ROLE_NAV          = 6,
    FTVIEW_ROLE_TREND_PEN    = 7,
    FTVIEW_ROLE_LABEL        = 8
} ftview_object_role_t;

/* =====================================================================
 * Common error codes
 * ===================================================================== */

typedef enum {
    FTVIEW_OK                  =  0,
    FTVIEW_ERR_NULL_PTR        = -1,
    FTVIEW_ERR_OUT_OF_MEMORY   = -2,
    FTVIEW_ERR_TAG_NOT_FOUND   = -3,
    FTVIEW_ERR_DUPLICATE_TAG   = -4,
    FTVIEW_ERR_INVALID_PARAM   = -5,
    FTVIEW_ERR_BUFFER_FULL     = -6,
    FTVIEW_ERR_COMM_TIMEOUT    = -7,
    FTVIEW_ERR_AUTH_FAILED     = -8,
    FTVIEW_ERR_DB_CORRUPT      = -9,
    FTVIEW_ERR_NOT_IMPLEMENTED = -10
} ftview_err_t;

const char *ftview_strerror(ftview_err_t err);

/** L2.1 -- Update rate constants */
#define FTVIEW_DEFAULT_UPDATE_RATE_MS   1000
#define FTVIEW_FAST_UPDATE_RATE_MS       250
#define FTVIEW_SLOW_UPDATE_RATE_MS      5000
#define FTVIEW_DATA_STALE_THRESHOLD_MS  5000

#endif /* FTVIEW_COMMON_H */
