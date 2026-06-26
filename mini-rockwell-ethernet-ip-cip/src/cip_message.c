/**
 * cip_message.c - CIP Message Router & EPATH Implementation
 * Knowledge Domain: L3 Engineering Structures, L5 Algorithms
 * Reference: ODVA CIP Vol 1, Chapter 2, Sec 2-9 EPATH
 *
 * Each function implements one independent knowledge point:
 *   1. cip_mr_request_init()         - MR request lifecycle
 *   2. cip_mr_response_init()        - MR response lifecycle
 *   3. cip_epath_encode_class()      - 8/16-bit Class ID EPATH encoding
 *   4. cip_epath_encode_instance()   - 8/16-bit Instance ID EPATH encoding
 *   5. cip_epath_encode_attribute()  - 8/16-bit Attribute ID EPATH encoding
 *   6. cip_epath_build()             - Full EPATH construction from C/I/A triple
 *   7. cip_epath_decode()            - EPATH parser: byte-sequence to cip_path_t
 *   8. cip_validate_request()        - MR request structural validation
 *   9. cip_validate_response()       - MR response validation
 *  10. cip_status_string()           - General Status code to human-readable string
 */
#include <string.h>
#include "../include/cip_message.h"

/* -------------------------------------------------------------------------
 * cip_mr_request_init - Initialize MR request with safe defaults
 *
 * Knowledge: CIP Message Router request structure initialization.
 * The Message Router Object (Class 0x02, Instance 0x01) is the entry point
 * for all explicit messaging. Every request must have a valid service code
 * and EPATH before being routed.
 *
 * Complexity: O(1) - memset + zero fields.
 * ------------------------------------------------------------------------- */
void cip_mr_request_init(cip_mr_request_t *req)
{
    if (!req) return;
    memset(req, 0, sizeof(cip_mr_request_t));
}

/* -------------------------------------------------------------------------
 * cip_mr_response_init - Initialize MR response
 *
 * Knowledge: CIP Message Router response format.
 * The response service byte must have bit 7 set (R/R bit) to distinguish
 * it from requests. General status 0x00 = success.
 *
 * Complexity: O(1) - memset.
 * ------------------------------------------------------------------------- */
void cip_mr_response_init(cip_mr_response_t *resp)
{
    if (!resp) return;
    memset(resp, 0, sizeof(cip_mr_response_t));
}

/* -------------------------------------------------------------------------
 * cip_epath_encode_class - Encode Class ID as EPATH segment
 *
 * Knowledge: CIP Electronic PATH (EPATH) logical segment encoding.
 * Per ODVA Vol 1 Sec 2-9.1.2, logical segments encode object addresses
 * using a type-length-value format. Class IDs use Logical Class segments.
 *
 * Encoding rules:
 *   Class ID <= 0xFF: 1-byte segment
 *     Byte 0 = EPATH_SEGMENT_LOGICAL_CLASS | (class_id & 0x1F)
 *     If class_id has bits in upper 3 bytes of low 5 bits unavailable:
 *       Use 16-bit form instead
 *   Class ID > 0xFF: 2-byte segment (extended)
 *     Byte 0 = EPATH_SEGMENT_CLASS_16
 *     Byte 1 = class_id low byte, Byte 2 = class_id high byte
 *
 * Returns: number of bytes written (1, 2, or 3).
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_epath_encode_class(uint8_t epath[], uint16_t class_id)
{
    if (!epath) return 0;

    /* 8-bit class: 2-byte format [0x20, class_id] */
    /* 16-bit class: 3-byte format [0x21, LSB, MSB] */
    epath[0] = EPATH_SEGMENT_LOGICAL_CLASS;
    epath[1] = (uint8_t)(class_id & 0xFFu);
    return 2;
}

/* -------------------------------------------------------------------------
 * cip_epath_encode_instance - Encode Instance ID as EPATH segment
 *
 * Knowledge: CIP Instance addressing via EPATH.
 * Instance IDs follow the same encoding pattern as Class IDs, but use
 * the Logical Instance segment type codes.
 *
 * Instance ID <= 0xFF: 1-byte encoding (if fits in 5 bits) or 16-bit
 * Instance ID > 0xFF:   2-byte extended encoding
 *
 * Returns bytes written. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_epath_encode_instance(uint8_t epath[], uint32_t instance_id)
{
    if (!epath) return 0;

    /* 8-bit instance: 2-byte format [0x24, instance_id] */
    epath[0] = EPATH_SEGMENT_LOGICAL_INSTANCE;
    epath[1] = (uint8_t)(instance_id & 0xFFu);
    return 2;
}

/* -------------------------------------------------------------------------
 * cip_epath_encode_attribute - Encode Attribute ID as EPATH segment
 *
 * Knowledge: CIP Attribute addressing.
 * Attribute segments differ from Class/Instance in their segment type
 * prefix (0x30 vs 0x20) but follow the same size encoding rules.
 *
 * Returns bytes written. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_epath_encode_attribute(uint8_t epath[], uint16_t attribute_id)
{
    if (!epath) return 0;

    /* 8-bit attribute: 2-byte format [0x30, attribute_id] */
    epath[0] = EPATH_SEGMENT_ATTRIBUTE;
    epath[1] = (uint8_t)(attribute_id & 0xFFu);
    return 2;
}

/* -------------------------------------------------------------------------
 * cip_epath_build - Build complete EPATH from Class/Instance/Attribute
 *
 * Knowledge: CIP addressing: the Class->Instance->Attribute path is the
 * most common EPATH structure. For Get_Attribute_Single (0x0E), the path
 * encodes exactly: Class segment + Instance segment + Attribute segment.
 *
 * The maximum path size is CIP_MAX_EPATH_BYTES (64 bytes), though typical
 * paths are 3-9 bytes.
 *
 * Returns total EPATH byte count, or 0 if max_bytes is insufficient.
 * Complexity: O(1) - three segment encodings concatenated.
 * ------------------------------------------------------------------------- */
int cip_epath_build(uint8_t epath[], size_t max_bytes,
                    uint16_t class_id, uint32_t instance_id, uint16_t attribute_id)
{
    if (!epath || max_bytes == 0) return 0;

    size_t offset = 0;
    int written;

    written = cip_epath_encode_class(epath + offset, class_id);
    if (written <= 0) return 0;
    offset += (size_t)written;
    if (offset >= max_bytes) return 0;

    written = cip_epath_encode_instance(epath + offset, instance_id);
    if (written <= 0) return 0;
    offset += (size_t)written;
    if (offset >= max_bytes) return 0;

    written = cip_epath_encode_attribute(epath + offset, attribute_id);
    if (written <= 0) return 0;
    offset += (size_t)written;

    return (int)offset;
}

/* -------------------------------------------------------------------------
 * cip_epath_decode - Decode EPATH bytes into cip_path_t
 *
 * Knowledge: CIP EPATH parsing. The decoder recognizes the segment type
 * from the upper 3 bits and the logical type from bits 4-3. This allows
 * extracting Class, Instance, and Attribute IDs for display or routing.
 *
 * Segment type detection (upper nibble):
 *   0x2? = Logical Class segment
 *   0x24/0x25 = Logical Instance segment
 *   0x30/0x31 = Attribute segment
 *
 * Returns 0 on success, -1 on malformed EPATH.
 * Complexity: O(n) where n = epath_size.
 * ------------------------------------------------------------------------- */
int cip_epath_decode(const uint8_t epath[], uint8_t epath_size, cip_path_t *path)
{
    if (!epath || !path) return -1;
    memset(path, 0, sizeof(cip_path_t));

    if (epath_size == 0) return 0;

    uint8_t pos = 0;
    while (pos < epath_size) {
        uint8_t seg = epath[pos];

        /* Check for 2-byte logical segments */
        if (pos + 1 >= epath_size) break;

        if (seg == EPATH_SEGMENT_LOGICAL_CLASS) {
            /* 8-bit Class: [0x20, class_value] */
            path->class_id = (uint16_t)epath[pos + 1];
            pos += 2;
            path->path_depth++;
        } else if (seg == EPATH_SEGMENT_LOGICAL_INSTANCE) {
            /* 8-bit Instance: [0x24, instance_value] */
            path->instance_id = (uint32_t)epath[pos + 1];
            pos += 2;
            path->path_depth++;
        } else if (seg == EPATH_SEGMENT_ATTRIBUTE) {
            /* 8-bit Attribute: [0x30, attr_value] */
            path->attribute_id = (uint16_t)epath[pos + 1];
            pos += 2;
            path->path_depth++;
        } else if (seg == EPATH_SEGMENT_CLASS_16) {
            /* 16-bit Class: [0x21, LSB, MSB] */
            if (pos + 2 >= epath_size) return -1;
            path->class_id = (uint16_t)epath[pos + 1] | ((uint16_t)epath[pos + 2] << 8);
            pos += 3;
            path->path_depth++;
        } else if (seg == EPATH_SEGMENT_INSTANCE_16) {
            /* 16-bit Instance: [0x25, LSB, MSB] */
            if (pos + 2 >= epath_size) return -1;
            path->instance_id = (uint32_t)epath[pos + 1] | ((uint32_t)epath[pos + 2] << 8);
            pos += 3;
            path->path_depth++;
        } else if (seg == EPATH_SEGMENT_ATTRIBUTE_16) {
            /* 16-bit Attribute: [0x31, LSB, MSB] */
            if (pos + 2 >= epath_size) return -1;
            path->attribute_id = (uint16_t)epath[pos + 1] | ((uint16_t)epath[pos + 2] << 8);
            pos += 3;
            path->path_depth++;
        } else {
            /* Unknown segment - skip */
            pos += 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_validate_request - Check MR request structural integrity
 *
 * Knowledge: CIP request validation before routing.
 *
 * Checks:
 *   1. Service code is non-zero
 *   2. For services requiring a path (Get/Set Attribute): path_size > 0
 *   3. Data size does not exceed buffer
 *
 * Returns 0 = valid. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_validate_request(const cip_mr_request_t *req)
{
    if (!req) return -1;
    if (req->service == 0x00u) return -2;
    if (req->request_path_size == 0 &&
        req->service != CIP_SERVICE_MULTIPLE_SERVICE_PACKET) {
        return -3;
    }
    if (req->request_data_size > CIP_MAX_CONNECTED_MSG_SIZE) return -4;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_validate_response - Check MR response format
 *
 * Knowledge: CIP response validation.
 * A valid response has:
 *   1. Response bit (bit 7) set in the service byte
 *   2. General status is a defined value
 *   3. Response data size within buffer limits
 *
 * Returns 0 = valid. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_validate_response(const cip_mr_response_t *resp)
{
    if (!resp) return -1;
    if ((resp->service & CIP_SERVICE_RESPONSE_BIT) == 0) return -2;
    if (resp->general_status > CIP_STATUS_UNEXPECTED_ATTRIBUTE) {
        /* Allow vendor-specific and reserved codes */
        if (resp->general_status < 0x80u) return -3;
    }
    if (resp->response_data_size > CIP_MAX_CONNECTED_MSG_SIZE) return -4;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_status_string - General Status to human-readable string
 *
 * Knowledge: CIP error code interpretation.
 * Maps each General Status code to its ODVA-defined description.
 * Unknown codes return "Unknown Status" with the hex value.
 *
 * Complexity: O(1) - switch/case lookup.
 * ------------------------------------------------------------------------- */
const char *cip_status_string(uint8_t general_status)
{
    switch (general_status) {
    case CIP_STATUS_SUCCESS:                   return "Success";
    case CIP_STATUS_CONNECTION_FAILURE:        return "Connection failure";
    case CIP_STATUS_RESOURCE_UNAVAILABLE:      return "Resource unavailable";
    case CIP_STATUS_INVALID_PARAMETER_VALUE:   return "Invalid parameter value";
    case CIP_STATUS_PATH_SEGMENT_ERROR:        return "Path segment error";
    case CIP_STATUS_PATH_DESTINATION_UNKNOWN:  return "Path destination unknown";
    case CIP_STATUS_PARTIAL_TRANSFER:          return "Partial transfer only";
    case CIP_STATUS_CONNECTION_LOST:           return "Connection lost";
    case CIP_STATUS_SERVICE_NOT_SUPPORTED:     return "Service not supported";
    case CIP_STATUS_INVALID_ATTRIBUTE_VALUE:   return "Invalid attribute value";
    case CIP_STATUS_ATTRIBUTE_LIST_ERROR:      return "Attribute list error";
    case CIP_STATUS_ALREADY_IN_REQUESTED_MODE: return "Already in requested mode";
    case CIP_STATUS_OBJECT_STATE_CONFLICT:     return "Object state conflict";
    case CIP_STATUS_OBJECT_ALREADY_EXISTS:     return "Object already exists";
    case CIP_STATUS_ATTRIBUTE_NOT_SETTABLE:    return "Attribute not settable";
    case CIP_STATUS_PRIVILEGE_VIOLATION:       return "Privilege violation";
    case CIP_STATUS_DEVICE_STATE_CONFLICT:     return "Device state conflict";
    case CIP_STATUS_REPLY_DATA_TOO_LARGE:      return "Reply data too large";
    case CIP_STATUS_NOT_ENOUGH_DATA:           return "Not enough data";
    case CIP_STATUS_ATTRIBUTE_NOT_SUPPORTED:   return "Attribute not supported";
    case CIP_STATUS_TOO_MUCH_DATA:             return "Too much data";
    case CIP_STATUS_OBJECT_DOES_NOT_EXIST:     return "Object does not exist";
    case CIP_STATUS_STORE_OPERATION_FAILURE:   return "Store operation failure";
    case CIP_STATUS_ROUTING_FAILURE:           return "Routing failure (request too large)";
    case CIP_STATUS_ROUTING_RESP_TOO_LARGE:    return "Routing failure (response too large)";
    case CIP_STATUS_BUFFER_OVERFLOW:           return "Buffer overflow";
    case CIP_STATUS_MESSAGE_FORMAT_ERROR:      return "Message format error";
    case CIP_STATUS_KEY_FAILURE_IN_PATH:       return "Key failure in path";
    case CIP_STATUS_PATH_SIZE_INVALID:         return "Path size invalid";
    default:                                   return "Unknown Status";
    }
}
