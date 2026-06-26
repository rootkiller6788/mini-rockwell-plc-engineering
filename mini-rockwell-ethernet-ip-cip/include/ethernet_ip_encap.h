/**
 * ethernet_ip_encap.h - EtherNet/IP Encapsulation Layer
 * Knowledge Domain: L3 Engineering Structures
 * Reference: ODVA Vol 2, Chapter 2: Encapsulation Protocol
 *
 * Concepts: Encapsulation header (24-byte fixed), Session management,
 *           CPF (Common Packet Format), ListIdentity, Register/UnregisterSession,
 *           SendRRData (unconnected), SendUnitData (connected)
 * School: RWTH Aachen Industrial Control, Purdue ME 575
 */
#ifndef ETHERNET_IP_ENCAP_H
#define ETHERNET_IP_ENCAP_H
#include <stdint.h>
#include <stddef.h>
#include "ethernet_ip_cip.h"

/* Encapsulation Commands (ODVA Vol 2, Sec 2-3.3) */
#define EIP_ENCAP_COMMAND_NOP              0x0000u
#define EIP_ENCAP_COMMAND_LIST_SERVICES    0x0004u
#define EIP_ENCAP_COMMAND_LIST_IDENTITY    0x0063u
#define EIP_ENCAP_COMMAND_LIST_INTERFACES  0x0064u
#define EIP_ENCAP_COMMAND_REGISTER_SESSION 0x0065u
#define EIP_ENCAP_COMMAND_UNREGISTER_SESSION 0x0066u
#define EIP_ENCAP_COMMAND_SEND_RR_DATA     0x006Fu
#define EIP_ENCAP_COMMAND_SEND_UNIT_DATA   0x0070u
#define EIP_ENCAP_COMMAND_INDICATE_STATUS  0x0072u
#define EIP_ENCAP_COMMAND_CANCEL           0x0073u

/* Encapsulation Status Codes */
#define EIP_ENCAP_STATUS_SUCCESS           0x0000u
#define EIP_ENCAP_STATUS_INVALID_COMMAND   0x0001u
#define EIP_ENCAP_STATUS_INSUFFICIENT_MEMORY 0x0002u
#define EIP_ENCAP_STATUS_INCORRECT_DATA    0x0003u
#define EIP_ENCAP_STATUS_INVALID_SESSION   0x0064u
#define EIP_ENCAP_STATUS_INVALID_LENGTH    0x0065u
#define EIP_ENCAP_STATUS_UNSUPPORTED_PROTOCOL 0x0069u

#define EIP_ENCAP_HEADER_SIZE              24u
#define EIP_ENCAP_MAX_SESSION_COUNT        64u

/* 24-byte encapsulation header (fixed format per ODVA Vol 2, Sec 2-3) */
#pragma pack(push, 1)
typedef struct {
    uint16_t command;
    uint16_t length;
    uint32_t session_handle;
    uint32_t status;
    uint8_t  sender_context[8];
    uint32_t options;
} eip_encap_header_t;
#pragma pack(pop)

typedef struct {
    uint32_t session_handle;
    uint32_t ip_address;
    uint16_t tcp_port;
    uint8_t  sender_context[8];
    int      is_active;
    uint32_t last_activity_ms;
} eip_session_t;

/* CPF (Common Packet Format) Item (ODVA Vol 2, Sec 2-6.3) */
typedef struct {
    uint16_t item_type_code;
    uint16_t item_length;
    uint8_t  data[1436];
} eip_cpf_item_t;

typedef struct {
    uint32_t item_count;
    uint16_t interface_handle;
    uint16_t timeout;
    eip_cpf_item_t items[8];
} eip_cpf_packet_t;

/* ListIdentity response item (ODVA Vol 2, Sec 2-4.3.2) */
typedef struct {
    uint16_t protocol_version;
    uint16_t option_flags;
    uint8_t  socket_addr_sin_family[2];
    uint16_t socket_addr_sin_port;
    uint8_t  socket_addr_sin_addr[4];
    uint8_t  socket_addr_sin_zero[8];
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint8_t  major_revision;
    uint8_t  minor_revision;
    uint16_t status;
    uint32_t serial_number;
    uint8_t  product_name_len;
    char     product_name[32];
    uint8_t  state;
} eip_identity_item_t;

void eip_encap_init_header(eip_encap_header_t *hdr, uint16_t command, uint16_t length, uint32_t session, uint32_t status);
int eip_encap_register_session(eip_session_t *session, uint32_t ip_addr, uint16_t port);
int eip_encap_unregister_session(eip_session_t *session);
int eip_encap_send_rr_data(eip_session_t *session, const uint8_t *cip_data, uint16_t cip_len, uint32_t timeout_ms);
int eip_encap_send_unit_data(eip_session_t *session, const uint8_t *cip_data, uint16_t cip_len);
int eip_encap_parse_header(const uint8_t *raw_data, uint16_t raw_len, eip_encap_header_t *hdr, uint16_t *data_offset);
int eip_encap_list_identity(const eip_identity_item_t *id, uint8_t *response, uint16_t *resp_len, uint16_t max_len);
int eip_encap_cpf_build(eip_cpf_packet_t *cpf, uint16_t iface_handle, uint16_t timeout);
int eip_encap_cpf_add_item(eip_cpf_packet_t *cpf, uint16_t type_code, const uint8_t *data, uint16_t data_len);
int eip_encap_validate_session(const eip_session_t *session);
uint32_t eip_encap_calculate_timeout(uint32_t rpi_us, uint8_t multiplier);
const char *eip_encap_command_string(uint16_t command);
const char *eip_encap_status_string(uint32_t status);
#endif /* ETHERNET_IP_ENCAP_H */
