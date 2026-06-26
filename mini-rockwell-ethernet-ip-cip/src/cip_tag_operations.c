/**
 * cip_tag_operations.c - ControlLogix Tag Read/Write Operations
 * Knowledge Domain: L5 Algorithms, L7 Industrial Applications
 * Reference: Rockwell Automation Knowledgebase, ODVA CIP Vol 1
 *
 * Each function implements one independent knowledge point:
 *   1. cip_tag_build_read_request()     - Tag Read service request
 *   2. cip_tag_build_write_request()    - Tag Write service request
 *   3. cip_tag_encode_symbol()          - ANCI Extended Symbolic EPATH
 *   4. cip_tag_decode_value()           - Tag value parsing from response
 *   5. cip_tag_get_type_code()          - CIP data type to type code
 *   6. cip_tag_calculate_segments()     - Tag EPATH segment count
 *   7. cip_tag_validate_name()          - Tag name syntax validation
 *   8. cip_tag_build_multi_request()    - Multiple tag read in one message
 *
 * School: RWTH Aachen, Rockwell Studio 5000 tag programming,
 *         Purdue ME 575 - Industrial data access patterns
 */
#include <string.h>
#include "../include/cip_message.h"
#include "../include/cip_object.h"

#define CIP_TAG_SERVICE_READ         0x4Cu    /* Read Tag Service */
#define CIP_TAG_SERVICE_WRITE        0x4Du    /* Write Tag Service */
#define CIP_TAG_SERVICE_READ_MODIFY_WRITE 0x4Eu
#define CIP_TAG_SERVICE_READ_FRAGMENTED 0x52u
#define CIP_TAG_SERVICE_WRITE_FRAGMENTED 0x53u
#define CIP_TAG_MAX_NAME_LEN         128u
#define CIP_TAG_MAX_SEGMENTS         16u
#define CIP_TAG_SYMBOL_CLASS         0x6Bu    /* Symbol Object Class */

/* Tag information structure */
typedef struct {
    uint32_t instance_id;
    uint16_t data_type;
    uint16_t element_count;
    uint16_t symbol_type;
    char     tag_name[CIP_TAG_MAX_NAME_LEN];
} cip_tag_info_t;

/* -------------------------------------------------------------------------
 * cip_tag_build_read_request - Build a Tag Read service request
 *
 * Knowledge: ControlLogix/CompactLogix Tag Read.
 * Uses the Symbol Object (Class 0x6B) to read controller-scoped tags
 * by name. The tag name is encoded as an ANCI Extended Symbolic EPATH
 * segment. Tag Read is the primary data access mechanism for HMI/SCADA.
 *
 * Service: Read Tag Service (0x4C)
 * Path: Symbol Object (0x6B), Instance 1
 * Data: number of elements to read
 *
 * Returns: total request size in bytes. Complexity: O(n) where n = name_len.
 * ------------------------------------------------------------------------- */
int cip_tag_build_read_request(const char *tag_name, uint16_t element_count,
                               uint8_t *req_buffer, uint16_t buffer_size)
{
    if (!tag_name || !req_buffer) return 0;

    size_t name_len = strlen(tag_name);
    if (name_len == 0 || name_len > CIP_TAG_MAX_NAME_LEN) return 0;

    /* Calculate total request size */
    uint16_t total = (uint16_t)(6 + 2 + name_len + 1 + 2);
    if (total > buffer_size) return 0;

    uint16_t offset = 0;
    /* Service code: Read Tag (0x4C) */
    req_buffer[offset++] = CIP_TAG_SERVICE_READ;

    /* Request path size: 2 bytes per segment (class + instance + symbolic) */
    uint8_t path_bytes = (uint8_t)(2 + 2 + 2 + name_len);
    req_buffer[offset++] = path_bytes;

    /* Path: Class Symbol (0x6B, 2-byte logical class) */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_CLASS;
    req_buffer[offset++] = (uint8_t)(CIP_TAG_SYMBOL_CLASS & 0xFFu);

    /* Path: Instance 1 (2-byte logical instance) */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_INSTANCE;
    req_buffer[offset++] = 1;

    /* Path: ANCI Extended Symbolic segment */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_SYMBOLIC;
    req_buffer[offset++] = (uint8_t)(name_len & 0xFFu);
    memcpy(&req_buffer[offset], tag_name, name_len);
    offset += (uint16_t)name_len;

    /* Number of elements */
    req_buffer[offset++] = (uint8_t)(element_count & 0xFF);
    req_buffer[offset++] = (uint8_t)((element_count >> 8) & 0xFF);

    return (int)offset;
}

/* -------------------------------------------------------------------------
 * cip_tag_build_write_request - Build a Tag Write service request
 *
 * Knowledge: ControlLogix/CompactLogix Tag Write.
 * Uses the Write Tag Service (0x4D) to write new values to controller
 * tags. The data format must match the tag's CIP data type.
 *
 * Returns: total request size in bytes. Complexity: O(n).
 * ------------------------------------------------------------------------- */
int cip_tag_build_write_request(const char *tag_name, uint16_t type_code,
                                uint16_t element_count,
                                const uint8_t *write_data, uint16_t data_len,
                                uint8_t *req_buffer, uint16_t buffer_size)
{
    if (!tag_name || !write_data || !req_buffer) return 0;

    size_t name_len = strlen(tag_name);
    if (name_len == 0 || name_len > CIP_TAG_MAX_NAME_LEN) return 0;

    uint16_t total = (uint16_t)(6 + 2 + 2 + 2 + name_len + 2 + 2 + data_len);
    if (total > buffer_size) return 0;

    uint16_t offset = 0;
    req_buffer[offset++] = CIP_TAG_SERVICE_WRITE;

    uint8_t path_bytes = (uint8_t)(2 + 2 + 2 + name_len);
    req_buffer[offset++] = path_bytes;

    /* Path: Symbol Class (0x6B) */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_CLASS;
    req_buffer[offset++] = (uint8_t)(CIP_TAG_SYMBOL_CLASS & 0xFFu);

    /* Path: Instance 1 */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_INSTANCE;
    req_buffer[offset++] = 1;

    /* ANCI Extended Symbolic */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_SYMBOLIC;
    req_buffer[offset++] = (uint8_t)(name_len & 0xFFu);
    memcpy(&req_buffer[offset], tag_name, name_len);
    offset += (uint16_t)name_len;

    /* Data type code */
    req_buffer[offset++] = (uint8_t)(type_code & 0xFF);
    req_buffer[offset++] = (uint8_t)((type_code >> 8) & 0xFF);

    /* Element count */
    req_buffer[offset++] = (uint8_t)(element_count & 0xFF);
    req_buffer[offset++] = (uint8_t)((element_count >> 8) & 0xFF);

    /* Write data */
    memcpy(&req_buffer[offset], write_data, data_len);
    offset += data_len;

    return (int)offset;
}

/* -------------------------------------------------------------------------
 * cip_tag_encode_symbol - Encode tag name as ANCI Extended Symbolic EPATH
 *
 * Knowledge: CIP symbolic addressing for tag-based controllers.
 * ControlLogix and CompactLogix use symbolic (tag-based) addressing rather
 * than fixed register addresses. The tag name is encoded as an ANCI
 * (American National Standards Institute) Extended Symbolic segment in
 * the EPATH, allowing direct tag access by name.
 *
 * The segment format: [segment_byte] [name_length] [name_bytes...]
 *
 * Returns: bytes written. Complexity: O(n) where n = name_len.
 * ------------------------------------------------------------------------- */
int cip_tag_encode_symbol(const char *tag_name, uint8_t *epath_buf, uint16_t buf_size)
{
    if (!tag_name || !epath_buf) return 0;

    size_t len = strlen(tag_name);
    if (len == 0 || len > 255) return 0;

    uint16_t needed = (uint16_t)(1 + len);
    if (needed > buf_size) return 0;

    /* Segment byte: Logical Symbolic + length in low 5 bits for short names */
    if (len <= 0x1F) {
        epath_buf[0] = EPATH_SEGMENT_LOGICAL_SYMBOLIC | (uint8_t)(len & 0x1Fu);
        memcpy(&epath_buf[1], tag_name, len);
        return (int)(1 + len);
    } else {
        /* Extended length: Pad byte + length byte */
        epath_buf[0] = EPATH_SEGMENT_LOGICAL_SYMBOLIC;
        epath_buf[1] = (uint8_t)(len & 0xFF);
        memcpy(&epath_buf[2], tag_name, len);
        return (int)(2 + len);
    }
}

/* -------------------------------------------------------------------------
 * cip_tag_decode_value - Decode a typed CIP value from response bytes
 *
 * Knowledge: CIP data decoding from tag read responses.
 * Parses the CIP typed value from the response according to the data type
 * code. Supports DINT, REAL, SINT, INT, BOOL.
 *
 * Returns: 0 = success, -1 = unknown type. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_tag_decode_value(const uint8_t *data, uint16_t data_len,
                         uint16_t type_code, void *value_out, uint16_t *value_size)
{
    if (!data || !value_out || !value_size) return -1;

    switch (type_code) {
    case 0x00C4: /* DINT - 4 bytes signed */
        if (data_len < 4) return -1;
        {
            int32_t val = (int32_t)data[0]
                        | ((int32_t)data[1] << 8)
                        | ((int32_t)data[2] << 16)
                        | ((int32_t)data[3] << 24);
            *(int32_t *)value_out = val;
        }
        *value_size = 4;
        return 0;

    case 0x00CA: /* REAL - 4 bytes IEEE 754 single-precision */
        if (data_len < 4) return -1;
        memcpy(value_out, data, 4);
        *value_size = 4;
        return 0;

    case 0x00C2: /* SINT - 1 byte signed */
        *(int8_t *)value_out = (int8_t)data[0];
        *value_size = 1;
        return 0;

    case 0x00C3: /* INT - 2 bytes signed */
        {
            int16_t val = (int16_t)data[0] | ((int16_t)data[1] << 8);
            *(int16_t *)value_out = val;
        }
        *value_size = 2;
        return 0;

    case 0x00C1: /* BOOL - 1 byte */
        *(uint8_t *)value_out = data[0];
        *value_size = 1;
        return 0;

    default:
        *value_size = 0;
        return -1;
    }
}

/* -------------------------------------------------------------------------
 * cip_tag_get_type_code - Map CIP data type to type code
 *
 * Knowledge: CIP data type code mapping.
 * Maps the common CIP data type names to their numeric codes used in
 * tag read/write operations.
 *
 * Returns: type code, or 0 if unknown. Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint16_t cip_tag_get_type_code(const char *type_name)
{
    if (!type_name) return 0;
    if (strcmp(type_name, "DINT") == 0) return 0x00C4u;
    if (strcmp(type_name, "REAL") == 0) return 0x00CAu;
    if (strcmp(type_name, "SINT") == 0) return 0x00C2u;
    if (strcmp(type_name, "INT") == 0)  return 0x00C3u;
    if (strcmp(type_name, "BOOL") == 0) return 0x00C1u;
    if (strcmp(type_name, "UDINT") == 0)return 0x00C8u;
    if (strcmp(type_name, "STRING") == 0) return 0x00D0u;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_tag_calculate_segments - Calculate number of EPATH segments for a tag
 *
 * Knowledge: Tag path decomposition for multi-level tag names.
 * ControlLogix supports program-scoped and controller-scoped tags.
 * A program-scoped tag like "Program:MainProgram.Counter" requires
 * additional EPATH segments for the program scope.
 *
 * Returns: segment count (always >= 2: class + symbolic).
 * Complexity: O(n) where n = number of '.' separators in name.
 * ------------------------------------------------------------------------- */
int cip_tag_calculate_segments(const char *tag_name)
{
    if (!tag_name) return 0;

    int segments = 2; /* class segment + symbolic segment minimum */
    const char *p = tag_name;
    while (*p) {
        if (*p == '.') segments++;
        if (*p == ':') segments++;
        p++;
    }
    return segments;
}

/* -------------------------------------------------------------------------
 * cip_tag_validate_name - Check tag name syntax
 *
 * Knowledge: ControlLogix tag naming rules.
 * Valid tag names:
 *   - Start with letter or underscore
 *   - Contain only [A-Za-z0-9_]
 *   - Max 40 characters (controller-scoped)
 *   - Case-insensitive
 *
 * Returns: 1 = valid, 0 = invalid. Complexity: O(n).
 * ------------------------------------------------------------------------- */
int cip_tag_validate_name(const char *tag_name)
{
    if (!tag_name) return 0;

    size_t len = strlen(tag_name);
    if (len == 0 || len > 40) return 0;

    /* First character: letter or underscore */
    char c = tag_name[0];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) {
        return 0;
    }

    /* Remaining characters: alphanumeric or underscore */
    for (size_t i = 1; i < len; i++) {
        c = tag_name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            /* Allow dots and colons for program-scoped tags */
            if (c != '.' && c != ':') return 0;
        }
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * cip_tag_build_multi_request - Build Multiple Service Packet for tags
 *
 * Knowledge: CIP Multiple Service Packet for batch tag operations.
 * The MSP (0x0A) allows sending multiple read/write requests in a single
 * message, reducing round-trips for HMI displays reading many tags.
 *
 * Returns: total request size, 0 if buffer too small.
 * Complexity: O(n*m) where n = tag_count, m = avg tag name length.
 * ------------------------------------------------------------------------- */
int cip_tag_build_multi_request(const char *tag_names[], uint16_t tag_count,
                                uint8_t *req_buffer, uint16_t buffer_size)
{
    if (!tag_names || !req_buffer || tag_count == 0) return 0;

    uint16_t offset = 0;

    /* MSP service code */
    req_buffer[offset++] = CIP_SERVICE_MULTIPLE_SERVICE_PACKET;

    /* Request path: Symbol Object (2-byte format per segment) */
    req_buffer[offset++] = 4; /* path size: class(2) + instance(2) */
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_CLASS;
    req_buffer[offset++] = (uint8_t)(CIP_TAG_SYMBOL_CLASS & 0xFFu);
    req_buffer[offset++] = EPATH_SEGMENT_LOGICAL_INSTANCE;
    req_buffer[offset++] = 1;

    /* Number of services in packet */
    req_buffer[offset++] = (uint8_t)(tag_count & 0xFF);
    req_buffer[offset++] = (uint8_t)((tag_count >> 8) & 0xFF);

    /* Offset array: 2 bytes per service offset entry */
    uint16_t offset_array_start = offset;
    offset += tag_count * 2;

    /* Build each service request */
    for (uint16_t i = 0; i < tag_count; i++) {
        /* Write offset in offset array */
        req_buffer[offset_array_start + i * 2] = (uint8_t)((offset - offset_array_start) & 0xFF);
        req_buffer[offset_array_start + i * 2 + 1] = (uint8_t)(((offset - offset_array_start) >> 8) & 0xFF);

        /* Build individual read request */
        int written = cip_tag_build_read_request(tag_names[i], 1,
                                                  &req_buffer[offset],
                                                  buffer_size - offset);
        if (written <= 0) return 0;
        offset += (uint16_t)written;
    }

    return (int)offset;
}
