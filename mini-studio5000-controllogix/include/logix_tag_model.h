/**
 * @file    logix_tag_model.h
 * @brief   L1-L3: ControlLogix Tag Data Model
 *
 * Knowledge Mapping:
 *   L1 Definitions:  Atomic types (BOOL/SINT/INT/DINT/REAL), structure types,
 *                    array types, tag scope (Controller vs Program),
 *                    alias tags, produced/consumed tags
 *   L2 Concepts:     Tag database, symbol resolution, data access rules,
 *                    tag scope and visibility, external access security
 *   L3 Structures:   Memory layout, data alignment, array indexing,
 *                    string types (STRING vs STRING[N]), UDT memory
 *   L7 Applications: Rockwell ControlLogix tag architecture
 *
 * Reference: Rockwell Automation Publication 1756-PM004
 *   "Logix5000 Controllers I/O and Tag Data"
 *
 * Course Alignment:
 *   MIT 2.171 - Digital Control: data types for real-time systems
 *   CMU 18-771 - Linear Systems: typed data structures
 *   ISA/IEC 61131-3: standard data types
 */

#ifndef LOGIX_TAG_MODEL_H
#define LOGIX_TAG_MODEL_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================
 * L1: Atomic Data Types (IEC 61131-3 + Logix Extensions)
 * ======================================================================== */

typedef enum {
    LOGIX_TYPE_BOOL      = 0xC0,
    LOGIX_TYPE_SINT      = 0xC1,
    LOGIX_TYPE_INT       = 0xC2,
    LOGIX_TYPE_DINT      = 0xC3,
    LOGIX_TYPE_LINT      = 0xC4,
    LOGIX_TYPE_REAL      = 0xC5,
    LOGIX_TYPE_LREAL     = 0xC6,
    LOGIX_TYPE_USINT     = 0xC7,
    LOGIX_TYPE_UINT      = 0xC8,
    LOGIX_TYPE_UDINT     = 0xC9,
    LOGIX_TYPE_ULINT     = 0xCA,
    LOGIX_TYPE_STRING    = 0xDA,
    LOGIX_TYPE_STRING_N  = 0xDB,
    LOGIX_TYPE_ARRAY     = 0xE0,
    LOGIX_TYPE_STRUCT    = 0xE1,
    LOGIX_TYPE_ALIAS     = 0xF0,
    LOGIX_TYPE_MODULE    = 0xF1,
} logix_atomic_type_t;

typedef enum {
    TAG_SCOPE_CONTROLLER = 0,
    TAG_SCOPE_PROGRAM    = 1
} logix_tag_scope_t;

typedef enum {
    ACCESS_READ_WRITE       = 0,
    ACCESS_READ_ONLY        = 1,
    ACCESS_EXTERNAL_READ    = 2,
    ACCESS_EXTERNAL_RW      = 3,
    ACCESS_NONE             = 4
} logix_tag_access_t;

typedef struct {
    uint32_t dim_size[3];
    uint32_t dim_count;
    uint32_t element_size;
    uint32_t total_elements;
} logix_array_desc_t;

typedef struct {
    char               name[40];
    logix_atomic_type_t type;
    uint32_t           offset_bytes;
    uint32_t           size_bytes;
    uint8_t            bit_position;
    logix_array_desc_t array_info;
    char               description[128];
    bool               is_required;
    bool               is_visible;
} logix_udt_member_t;

typedef struct {
    char               name[40];
    char               description[256];
    uint32_t           member_count;
    logix_udt_member_t members[200];
    uint32_t           total_size_bytes;
    bool               is_safety;
    bool               is_encrypted;
    uint32_t           nested_depth;
} logix_udt_t;

typedef struct {
    char               name[40];
    logix_tag_scope_t  scope;
    logix_atomic_type_t type;
    logix_tag_access_t access;
    union {
        bool     v_bool;
        int8_t   v_sint;
        int16_t  v_int;
        int32_t  v_dint;
        int64_t  v_lint;
        float    v_real;
        double   v_lreal;
        uint8_t  v_usint;
        uint16_t v_uint;
        uint32_t v_udint;
        uint64_t v_ulint;
    } value;
    logix_array_desc_t array_desc;
    uint32_t           udt_type_index;
    char               engineering_units[16];
    char               description[128];
    bool               is_constant;
    bool               is_produced;
    bool               is_consumed;
    uint32_t           produced_connection_id;
    uint32_t           consumed_connection_id;
    char               alias_target[40];
    uint8_t            slot;
} logix_tag_t;

typedef struct {
    uint32_t    producer_controller_id;
    uint32_t    connection_id;
    double      rpi_ms;
    uint32_t    data_size_bytes;
    uint32_t    consumer_count;
    uint32_t    consumer_ids[256];
    bool        use_unicast;
    bool        connection_active;
    uint32_t    packet_count;
    uint32_t    timeout_count;
} logix_produced_connection_t;

typedef struct {
    uint32_t    consumer_controller_id;
    uint32_t    connection_id;
    uint32_t    producer_controller_id;
    double      rpi_ms;
    uint32_t    data_size_bytes;
    bool        connection_active;
    uint32_t    packet_count;
    uint32_t    timeout_count;
    uint32_t    data_invalid_count;
} logix_consumed_connection_t;

typedef struct {
    uint32_t    total_memory_bytes;
    uint32_t    logic_memory_used;
    uint32_t    data_memory_used;
    uint32_t    io_memory_used;
    uint32_t    safety_memory_used;
    uint32_t    comm_buffer_used;
    uint32_t    tag_count_controller_scope;
    uint32_t    tag_count_program_scope;
    uint32_t    max_tag_name_length;
} logix_memory_stats_t;

/* API */
bool logix_tag_create_controller_scope(logix_tag_t *tag,
                                         const char *name,
                                         logix_atomic_type_t type);
bool logix_tag_create_program_scope(logix_tag_t *tag,
                                      const char *name,
                                      logix_atomic_type_t type);
bool logix_tag_set_alias(logix_tag_t *tag, const char *base_tag_name);
bool logix_tag_set_produced(logix_tag_t *tag, double rpi_ms);
bool logix_tag_set_consumed(logix_tag_t *tag, double rpi_ms);
bool logix_tag_write(logix_tag_t *tag, const void *source);
uint32_t logix_tag_read(const logix_tag_t *tag, void *dest);
uint32_t logix_udt_calculate_size(const logix_udt_t *udt);
bool logix_tag_validate_name(const char *name);
uint32_t logix_type_size(logix_atomic_type_t type);
const char *logix_type_name(logix_atomic_type_t type);
bool logix_tag_is_alias(const logix_tag_t *tag);
void logix_memory_update_stats(const logix_tag_t *tags,
                                uint32_t tag_count,
                                logix_memory_stats_t *stats);
int32_t logix_array_offset(const logix_array_desc_t *desc,
                             uint32_t i1, uint32_t i2, uint32_t i3);

#endif /* LOGIX_TAG_MODEL_H */
