/**
 * cip_object.h - CIP Object Model & Required Objects
 * Knowledge Domain: L1 Definitions, L3 Engineering Structures
 * Reference: ODVA CIP Vol 1, Chapter 5: Object Library
 *
 * Concepts: CIP Object model (Class/Instance/Attribute), Identity Object,
 *           Assembly Object, TCP/IP Interface, Ethernet Link,
 *           Connection Manager Object, CIP data type codes
 * School: RWTH Aachen, Purdue ME 575, ISA/IEC
 */
#ifndef CIP_OBJECT_H
#define CIP_OBJECT_H
#include <stdint.h>
#include <stddef.h>
#include "ethernet_ip_cip.h"

/* CIP Data Type Codes (ODVA Vol 1, Appendix D) */
typedef enum {
    CIP_OBJ_ATTR_TYPE_BOOL   = 0xC1,
    CIP_OBJ_ATTR_TYPE_SINT   = 0xC2,
    CIP_OBJ_ATTR_TYPE_INT    = 0xC3,
    CIP_OBJ_ATTR_TYPE_DINT   = 0xC4,
    CIP_OBJ_ATTR_TYPE_LINT   = 0xC5,
    CIP_OBJ_ATTR_TYPE_USINT  = 0xC6,
    CIP_OBJ_ATTR_TYPE_UINT   = 0xC7,
    CIP_OBJ_ATTR_TYPE_UDINT  = 0xC8,
    CIP_OBJ_ATTR_TYPE_ULINT  = 0xC9,
    CIP_OBJ_ATTR_TYPE_REAL   = 0xCA,
    CIP_OBJ_ATTR_TYPE_LREAL  = 0xCB,
    CIP_OBJ_ATTR_TYPE_STRING = 0xD0,
    CIP_OBJ_ATTR_TYPE_BYTE   = 0xD1,
    CIP_OBJ_ATTR_TYPE_WORD   = 0xD2,
    CIP_OBJ_ATTR_TYPE_DWORD  = 0xD3,
    CIP_OBJ_ATTR_TYPE_LWORD  = 0xD4,
    CIP_OBJ_ATTR_TYPE_STRUCT = 0xA0,
    CIP_OBJ_ATTR_TYPE_ARRAY  = 0xA3
} cip_attr_data_type_t;

typedef struct {
    uint16_t vendor_id;
    uint16_t device_type;
    uint16_t product_code;
    uint8_t  major_revision;
    uint8_t  minor_revision;
    uint16_t status;
    uint32_t serial_number;
    char     product_name[32];
    uint8_t  product_name_len;
} cip_identity_object_t;

typedef struct {
    uint32_t instance_id;
    uint8_t  data[512];
    uint16_t data_size;
    int      is_dynamic;
} cip_assembly_object_t;

typedef struct {
    uint32_t ip_address;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t primary_dns;
    uint32_t secondary_dns;
    char     hostname[64];
    uint8_t  ip_config_method;
    uint8_t  dhcp_enabled;
} cip_tcpip_object_t;

typedef struct {
    uint8_t  mac_address[6];
    uint16_t interface_speed_mbps;
    uint8_t  duplex_mode;
    uint8_t  autonegotiate;
    uint32_t in_octets;
    uint32_t in_discards;
    uint32_t in_errors;
    uint32_t out_octets;
    uint32_t out_discards;
    uint32_t out_errors;
} cip_ethernet_link_object_t;

typedef struct {
    uint16_t class_id;
    uint32_t instance_id;
    uint8_t  num_connections;
    uint32_t open_requests;
    uint32_t open_format_rejects;
    uint32_t open_resource_rejects;
    uint32_t open_other_rejects;
    uint32_t close_requests;
    uint32_t close_other_requests;
    uint32_t connection_timeouts;
} cip_connection_manager_object_t;

void cip_obj_identity_init(cip_identity_object_t *id, uint16_t vendor_id, uint16_t device_type, uint16_t product_code, uint8_t major_rev, uint8_t minor_rev, uint32_t serial, const char *product_name);
void cip_obj_assembly_init(cip_assembly_object_t *asmb, uint32_t instance_id, int is_dynamic);
int cip_obj_assembly_set_data(cip_assembly_object_t *asmb, const uint8_t *data, uint16_t size);
int cip_obj_assembly_get_data(const cip_assembly_object_t *asmb, uint8_t *buffer, uint16_t buffer_size, uint16_t *out_size);
void cip_obj_tcpip_init(cip_tcpip_object_t *tcpip, uint32_t ip, uint32_t mask, uint32_t gw, const char *hostname);
void cip_obj_ethernet_link_init(cip_ethernet_link_object_t *eth, const uint8_t mac[6], uint16_t speed_mbps, uint8_t duplex);
void cip_obj_connection_manager_init(cip_connection_manager_object_t *cm);
int cip_obj_validate_class_id(uint16_t class_id);
int cip_obj_validate_instance_id(uint16_t class_id, uint32_t instance_id);
const char *cip_obj_class_name(uint16_t class_id);
int cip_obj_get_attribute_size(uint16_t class_id, uint8_t attribute_id, uint16_t *size_out);
const char *cip_obj_attr_type_name(cip_attr_data_type_t type);

/* Data Assembly Template (from cip_data_assembly.c) */
#define CIP_DA_MAX_MEMBERS 64

typedef struct {
    uint16_t member_id;
    uint8_t  data_type;
    uint16_t byte_offset;
    uint16_t byte_size;
    uint8_t  bit_offset;
    uint8_t  bit_size;
    char     member_name[32];
} cip_da_member_t;

typedef struct {
    cip_da_member_t members[64];
    uint8_t  member_count;
    uint16_t total_size;
    uint32_t assembly_instance;
} cip_da_template_t;

void cip_da_init_template(cip_da_template_t *tmpl, uint32_t instance);
int cip_da_add_member(cip_da_template_t *tmpl, uint16_t member_id, uint8_t data_type, uint16_t byte_size, uint8_t bit_offset, uint8_t bit_size, const char *name);
uint16_t cip_da_calculate_size(const cip_da_template_t *tmpl);
int cip_da_pack_data(const cip_da_template_t *tmpl, const uint8_t **member_data, const uint16_t *member_data_sizes, uint8_t *assembly_buffer, uint16_t buffer_size);
int cip_da_unpack_data(const cip_da_template_t *tmpl, const uint8_t *assembly_buffer, uint16_t buffer_size, uint8_t **member_buffers, uint16_t *member_buffer_sizes);
uint16_t cip_da_get_member_offset(const cip_da_template_t *tmpl, uint8_t idx);
uint16_t cip_da_byte_swap_16(uint16_t value);
uint32_t cip_da_byte_swap_32(uint32_t value);
#endif /* CIP_OBJECT_H */
