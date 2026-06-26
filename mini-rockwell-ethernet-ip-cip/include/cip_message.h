/**
 * cip_message.h - CIP Message Router & EPATH Encoding
 * Knowledge Domain: L3 Engineering Structures
 * Reference: ODVA CIP Vol 1, Chapter 2: Explicit Messaging
 *
 * Concepts: CIP Message Router PDU, EPATH logical segment encoding,
 *           General Status codes (complete enumeration), Unconnected Send
 * School: RWTH Aachen PLC/SCADA, CMU 24-677 Adv Ctrl Systems
 */
#ifndef CIP_MESSAGE_H
#define CIP_MESSAGE_H
#include <stdint.h>
#include <stddef.h>
#include "ethernet_ip_cip.h"

/* CIP Service Codes (ODVA Vol 1, Appendix A - Common Services) */
#define CIP_SERVICE_GET_ATTRIBUTE_ALL       0x01u
#define CIP_SERVICE_SET_ATTRIBUTE_ALL       0x02u
#define CIP_SERVICE_GET_ATTRIBUTE_LIST      0x03u
#define CIP_SERVICE_SET_ATTRIBUTE_LIST      0x04u
#define CIP_SERVICE_RESET                   0x05u
#define CIP_SERVICE_START                   0x06u
#define CIP_SERVICE_STOP                    0x07u
#define CIP_SERVICE_CREATE                  0x08u
#define CIP_SERVICE_DELETE                  0x09u
#define CIP_SERVICE_MULTIPLE_SERVICE_PACKET 0x0Au
#define CIP_SERVICE_APPLY_ATTRIBUTES        0x0Du
#define CIP_SERVICE_GET_ATTRIBUTE_SINGLE    0x0Eu
#define CIP_SERVICE_SET_ATTRIBUTE_SINGLE    0x10u
#define CIP_SERVICE_FIND_NEXT_OBJECT_INSTANCE 0x11u
#define CIP_SERVICE_RESTORE                 0x15u
#define CIP_SERVICE_SAVE                    0x16u
#define CIP_SERVICE_NO_OPERATION            0x17u
#define CIP_SERVICE_RESPONSE_BIT            0x80u
#define CIP_SERVICE_REQUEST_MASK            0x7Fu

/* CIP General Status Codes (ODVA Vol 1, Appendix B - complete) */
#define CIP_STATUS_SUCCESS                  0x00u
#define CIP_STATUS_CONNECTION_FAILURE       0x01u
#define CIP_STATUS_RESOURCE_UNAVAILABLE     0x02u
#define CIP_STATUS_INVALID_PARAMETER_VALUE  0x03u
#define CIP_STATUS_PATH_SEGMENT_ERROR       0x04u
#define CIP_STATUS_PATH_DESTINATION_UNKNOWN 0x05u
#define CIP_STATUS_PARTIAL_TRANSFER         0x06u
#define CIP_STATUS_CONNECTION_LOST          0x07u
#define CIP_STATUS_SERVICE_NOT_SUPPORTED    0x08u
#define CIP_STATUS_INVALID_ATTRIBUTE_VALUE  0x09u
#define CIP_STATUS_ATTRIBUTE_LIST_ERROR     0x0Au
#define CIP_STATUS_ALREADY_IN_REQUESTED_MODE 0x0Bu
#define CIP_STATUS_OBJECT_STATE_CONFLICT    0x0Cu
#define CIP_STATUS_OBJECT_ALREADY_EXISTS    0x0Du
#define CIP_STATUS_ATTRIBUTE_NOT_SETTABLE   0x0Eu
#define CIP_STATUS_PRIVILEGE_VIOLATION      0x0Fu
#define CIP_STATUS_DEVICE_STATE_CONFLICT    0x10u
#define CIP_STATUS_REPLY_DATA_TOO_LARGE     0x11u
#define CIP_STATUS_FRAGMENTATION_VALUE      0x12u
#define CIP_STATUS_NOT_ENOUGH_DATA          0x13u
#define CIP_STATUS_ATTRIBUTE_NOT_SUPPORTED  0x14u
#define CIP_STATUS_TOO_MUCH_DATA            0x15u
#define CIP_STATUS_OBJECT_DOES_NOT_EXIST    0x16u
#define CIP_STATUS_SERVICE_FRAG_SEQUENCE    0x17u
#define CIP_STATUS_NO_STORED_ATTRIBUTE_DATA 0x18u
#define CIP_STATUS_STORE_OPERATION_FAILURE  0x19u
#define CIP_STATUS_ROUTING_FAILURE          0x1Au
#define CIP_STATUS_ROUTING_RESP_TOO_LARGE   0x1Bu
#define CIP_STATUS_MISSING_ATTRIBUTE_LIST   0x1Cu
#define CIP_STATUS_INVALID_ATTRIBUTE_LIST   0x1Du
#define CIP_STATUS_EMBEDDED_SERVICE_ERROR   0x1Eu
#define CIP_STATUS_VENDOR_SPECIFIC          0x1Fu
#define CIP_STATUS_INVALID_PARAM            0x20u
#define CIP_STATUS_WRITE_ONCE_VALUE_SET     0x21u
#define CIP_STATUS_INVALID_REPLY_RECEIVED   0x22u
#define CIP_STATUS_BUFFER_OVERFLOW          0x23u
#define CIP_STATUS_MESSAGE_FORMAT_ERROR     0x24u
#define CIP_STATUS_KEY_FAILURE_IN_PATH      0x25u
#define CIP_STATUS_PATH_SIZE_INVALID        0x26u
#define CIP_STATUS_UNEXPECTED_ATTRIBUTE     0x27u

/* EPATH Logical Segment Types (ODVA Vol 1, Sec 2-9.1.2) */
#define EPATH_SEGMENT_TYPE_MASK             0xE0u
#define EPATH_SEGMENT_LOGICAL_CLASS         0x20u
#define EPATH_SEGMENT_LOGICAL_INSTANCE      0x24u
#define EPATH_SEGMENT_CLASS_16              0x21u
#define EPATH_SEGMENT_INSTANCE_16           0x25u
#define EPATH_SEGMENT_ATTRIBUTE             0x30u
#define EPATH_SEGMENT_ATTRIBUTE_16          0x31u
#define EPATH_SEGMENT_LOGICAL_SYMBOLIC      0x2Cu
#define EPATH_SEGMENT_8BIT_MASK             0x1Fu
#define EPATH_PAD_BYTE                      0x00u
#define CIP_MAX_EPATH_DEPTH                 10u
#define CIP_MAX_EPATH_BYTES                 64u

/* Message Router Request */
typedef struct {
    uint8_t  service;
    uint8_t  request_path[64];
    uint8_t  request_path_size;
    uint8_t  request_data[2000];
    uint16_t request_data_size;
} cip_mr_request_t;

/* Message Router Response */
typedef struct {
    uint8_t  service;
    uint8_t  reserved;
    uint8_t  general_status;
    uint8_t  size_of_additional_status;
    uint16_t additional_status[4];
    uint8_t  response_data[2000];
    uint16_t response_data_size;
} cip_mr_response_t;

/* High-level Class/Instance/Attribute service request */
typedef struct {
    uint8_t  service;
    uint8_t  class_id;
    uint8_t  instance_id;
    uint8_t  attribute_id;
    uint8_t  data[256];
    uint16_t data_size;
} cip_service_req_t;

/* Decoded human-readable CIP path */
typedef struct {
    uint16_t class_id;
    uint32_t instance_id;
    uint16_t attribute_id;
    char     symbolic_name[64];
    uint8_t  path_depth;
} cip_path_t;

/* Function declarations */
void cip_mr_request_init(cip_mr_request_t *req);
void cip_mr_response_init(cip_mr_response_t *resp);
int cip_epath_encode_class(uint8_t epath[], uint16_t class_id);
int cip_epath_encode_instance(uint8_t epath[], uint32_t instance_id);
int cip_epath_encode_attribute(uint8_t epath[], uint16_t attribute_id);
int cip_epath_build(uint8_t epath[], size_t max_bytes, uint16_t class_id, uint32_t instance_id, uint16_t attribute_id);
int cip_epath_decode(const uint8_t epath[], uint8_t epath_size, cip_path_t *path);
int cip_validate_request(const cip_mr_request_t *req);
int cip_validate_response(const cip_mr_response_t *resp);
const char *cip_status_string(uint8_t general_status);
#endif /* CIP_MESSAGE_H */
