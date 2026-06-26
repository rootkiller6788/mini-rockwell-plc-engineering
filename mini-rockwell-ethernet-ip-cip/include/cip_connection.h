/**
 * cip_connection.h - CIP Connection Management System
 * Knowledge Domain: L2 Core Concepts
 * Reference: ODVA CIP Vol 1, Chapter 3: Connection Management
 *
 * Concepts: CIP Connection State Machine (7 states, ODVA Fig 3-3.1),
 *           RPI/Timeout calculation, O->T/T->O connection points,
 *           Transport Class/Trigger, Connection priority, Ownership models
 * School: RWTH Aachen Industrial Control Systems, Purdue ME 575
 */
#ifndef CIP_CONNECTION_H
#define CIP_CONNECTION_H
#include <stdint.h>
#include <stddef.h>
#include "ethernet_ip_cip.h"

/* Connection State Machine (ODVA Vol 1, Sec 3-3.3, Figure 3-3.1) */
typedef enum {
    CIP_CONN_STATE_NONEXISTENT     = 0,
    CIP_CONN_STATE_CONFIGURING     = 1,
    CIP_CONN_STATE_WAITING_FOR_ID  = 2,
    CIP_CONN_STATE_ESTABLISHED     = 3,
    CIP_CONN_STATE_TIMING_OUT      = 4,
    CIP_CONN_STATE_DEFERRED_DELETE = 5,
    CIP_CONN_STATE_CLOSING         = 6
} cip_connection_state_t;

typedef enum {
    CIP_CONN_PRIORITY_LOW           = 0,
    CIP_CONN_PRIORITY_HIGH          = 1,
    CIP_CONN_PRIORITY_SCHEDULED     = 2,
    CIP_CONN_PRIORITY_URGENT        = 3
} cip_connection_priority_t;

typedef enum {
    CIP_CONN_SIZE_FIXED             = 0,
    CIP_CONN_SIZE_VARIABLE          = 1
} cip_connection_size_type_t;

#define CIP_MAX_CONNECTIONS                 256u
#define CIP_DEFAULT_TIMEOUT_MULTIPLIER      4u
#define CIP_CONN_ID_OT_BASE                 0x04000000u
#define CIP_CONN_ID_TO_BASE                 0x80000000u
#define CIP_CONN_POINT_OUTPUT_ASSEMBLY      0x64u
#define CIP_CONN_POINT_INPUT_ASSEMBLY       0x65u
#define CIP_CONN_POINT_CONFIG_ASSEMBLY      0x66u
#define CIP_CONN_POINT_HEARTBEAT            0x67u
#define CIP_MIN_RPI_US                      1000u
#define CIP_DEFAULT_RPI_US                  10000u

/* CIP Connection Record */
typedef struct {
    uint32_t o_to_t_connection_id;
    uint32_t t_to_o_connection_id;
    uint16_t connection_serial_number;
    uint16_t originator_vendor_id;
    uint32_t originator_serial_number;
    uint8_t  connection_type;
    uint8_t  connection_state;
    uint8_t  transport_class;
    uint8_t  production_trigger;
    uint8_t  connection_priority;
    uint8_t  connection_size_type;
    uint32_t rpi_us;
    uint8_t  timeout_multiplier;
    uint16_t o_to_t_size;
    uint16_t t_to_o_size;
    uint16_t o_to_t_connection_point;
    uint16_t t_to_o_connection_point;
    uint16_t config_connection_point;
    uint8_t  config_data[512];
    uint16_t config_data_size;
    uint8_t  redundant_owner;
    uint8_t  connection_path[32];
    uint8_t  connection_path_size;
    uint32_t connection_timeout_us;
    int      is_active;
} cip_connection_t;

void cip_conn_init(cip_connection_t *conn);
int cip_conn_configure(cip_connection_t *conn, uint16_t o_to_t_size, uint16_t t_to_o_size, uint16_t ot_point, uint16_t to_point, uint32_t rpi_us);
int cip_conn_transition(cip_connection_t *conn, cip_connection_state_t new_state);
uint32_t cip_conn_calculate_timeout(cip_connection_t *conn);
int cip_conn_set_transport(cip_connection_t *conn, uint8_t transport_class, uint8_t trigger);
int cip_conn_set_type(cip_connection_t *conn, uint8_t conn_type);
int cip_conn_validate(const cip_connection_t *conn);
const char *cip_conn_state_string(cip_connection_state_t state);
void cip_conn_close(cip_connection_t *conn);
uint16_t cip_conn_next_serial(void);
uint32_t cip_conn_calculate_api(uint32_t rpi_us, uint32_t tick_us);
uint32_t cip_conn_ticks_per_cycle(uint32_t rpi_us, uint32_t tick_us);

/* Forward Open types (from cip_forward_open.c) */
typedef struct {
    uint8_t  priority_timetick;
    uint8_t  timeout_ticks;
    uint32_t o_to_t_connection_id;
    uint32_t t_to_o_connection_id;
    uint16_t connection_serial_number;
    uint16_t originator_vendor_id;
    uint32_t originator_serial_number;
    uint8_t  timeout_multiplier;
    uint8_t  reserved[3];
    uint32_t o_to_t_rpi;
    uint16_t o_to_t_network_params;
    uint32_t t_to_o_rpi;
    uint16_t t_to_o_network_params;
    uint8_t  transport_trigger;
    uint8_t  connection_path[32];
    uint8_t  connection_path_size;
} cip_forward_open_request_t;

typedef struct {
    uint32_t o_to_t_connection_id;
    uint32_t t_to_o_connection_id;
    uint16_t connection_serial_number;
    uint16_t originator_vendor_id;
    uint32_t originator_serial_number;
    uint32_t o_to_t_api;
    uint32_t t_to_o_api;
    uint8_t  application_reply_size;
    uint8_t  reserved;
    uint8_t  application_reply[256];
} cip_forward_open_response_t;

void cip_fo_init_request(cip_forward_open_request_t *fo);
int cip_fo_set_connection_params(cip_forward_open_request_t *fo, uint32_t o_to_t_rpi, uint16_t o_to_t_size, uint32_t t_to_o_rpi, uint16_t t_to_o_size, uint8_t conn_type);
int cip_fo_set_transport_params(cip_forward_open_request_t *fo, uint8_t transport_class, uint8_t trigger);
int cip_fo_set_network_params(cip_forward_open_request_t *fo, const uint8_t *path, uint8_t path_size, uint16_t vlan_id);
int cip_fo_encode_request(const cip_forward_open_request_t *fo, uint8_t *buffer, uint16_t buffer_size);
int cip_fo_parse_response(const uint8_t *data, uint16_t data_len, cip_forward_open_response_t *resp);
void cip_fo_extract_connection_ids(const cip_forward_open_response_t *resp, uint32_t *ot_id, uint32_t *to_id);
void cip_fo_init_close_request(uint16_t connection_serial, uint16_t originator_vendor, uint32_t originator_serial, uint8_t *buffer, uint16_t *buf_size);
int cip_fo_validate_connection_size(uint16_t o_to_t_size, uint16_t t_to_o_size);
uint32_t cip_fo_calculate_api(uint32_t rpi_us, uint32_t tick_us);

/* Tag operations (from cip_tag_operations.c) */
int cip_tag_build_read_request(const char *tag_name, uint16_t element_count, uint8_t *req_buffer, uint16_t buffer_size);
int cip_tag_build_write_request(const char *tag_name, uint16_t type_code, uint16_t element_count, const uint8_t *write_data, uint16_t data_len, uint8_t *req_buffer, uint16_t buffer_size);
int cip_tag_encode_symbol(const char *tag_name, uint8_t *epath_buf, uint16_t buf_size);
int cip_tag_decode_value(const uint8_t *data, uint16_t data_len, uint16_t type_code, void *value_out, uint16_t *value_size);
uint16_t cip_tag_get_type_code(const char *type_name);
int cip_tag_calculate_segments(const char *tag_name);
int cip_tag_validate_name(const char *tag_name);
int cip_tag_build_multi_request(const char *tag_names[], uint16_t tag_count, uint8_t *req_buffer, uint16_t buffer_size);
#endif /* CIP_CONNECTION_H */
