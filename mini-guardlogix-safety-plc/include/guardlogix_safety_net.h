/**
 * guardlogix_safety_net.h -- CIP Safety Network Communication
 *
 * Covers L1: CIP Safety protocol basics, safety connection types,
 * safety network number, time coordination, safety data production
 * and consumption models.
 *
 * Covers L2: Safety connection parameters (RPI, timeout multiplier,
 * network delay multiplier), safety open/close procedures.
 *
 * Covers L3: CIP Safety data frame structure, safety connection
 * reaction time limit calculation, timestamp-based coordination.
 *
 * CIP Safety is the safety protocol layer on top of EtherNet/IP
 * (CIP). It provides SIL 3 certified data transport between safety
 * devices using the "black channel" principle.
 *
 * Reference: CIP Safety Specification Vol. 8
 *            ODVA CIP Safety Architecture
 */

#ifndef GUARDLOGIX_SAFETY_NET_H
#define GUARDLOGIX_SAFETY_NET_H

#include <stdint.h>
#include "guardlogix_safety.h"

#define GLX_SAFETY_MAX_CONNECTION_ID_LEN 8
#define GLX_SAFETY_MAX_ORIGINATOR_SN_LEN 10
#define GLX_SAFETY_DEFAULT_RPI_MS        20
#define GLX_SAFETY_DEFAULT_TIMEOUT_MULT   4
#define GLX_SAFETY_DEFAULT_NET_DELAY_MULT 3
#define GLX_SAFETY_MIN_CRTL_MS           10
#define GLX_SAFETY_MAX_PAYLOAD_BYTES      256

typedef enum {
    GLX_SAFETY_CONN_ORIGINATOR = 0,
    GLX_SAFETY_CONN_TARGET     = 1
} glx_safety_connection_role_t;

typedef enum {
    GLX_SAFETY_CONN_UNICAST   = 0,
    GLX_SAFETY_CONN_MULTICAST = 1
} glx_safety_connection_type_t;

typedef enum {
    GLX_SAFETY_CONN_CLOSED       = 0,
    GLX_SAFETY_CONN_OPENING      = 1,
    GLX_SAFETY_CONN_ESTABLISHED  = 2,
    GLX_SAFETY_CONN_TIMED_OUT    = 3,
    GLX_SAFETY_CONN_FAULTED      = 4
} glx_safety_connection_state_t;

typedef enum {
    GLX_SAFETY_DATA_PRODUCTION = 0,
    GLX_SAFETY_DATA_CONSUMPTION = 1
} glx_safety_data_direction_t;

typedef struct {
    uint8_t  id[GLX_SAFETY_MAX_CONNECTION_ID_LEN];
    uint16_t id_length;
    uint16_t originating_snn;
    uint16_t target_snn;
} glx_safety_connection_id_t;

typedef struct {
    uint8_t  originator_sn[GLX_SAFETY_MAX_ORIGINATOR_SN_LEN];
    uint8_t  sn_length;
    uint16_t safety_network_number;
    uint32_t timestamp;
    uint32_t unique_id;
} glx_safety_originator_t;

typedef struct {
    uint32_t requested_packet_interval_us;
    uint16_t timeout_multiplier;
    uint16_t network_delay_multiplier;
    uint32_t timeout_value_us;
    uint32_t min_crtl_us;
    uint32_t max_crtl_us;
} glx_safety_connection_params_t;

typedef struct {
    glx_safety_connection_id_t     connection_id;
    glx_safety_connection_role_t   role;
    glx_safety_connection_type_t   conn_type;
    glx_safety_connection_state_t  state;
    glx_safety_connection_params_t params;
    glx_safety_data_direction_t    direction;
    uint16_t produced_data_size;
    uint16_t consumed_data_size;
    uint32_t last_rx_timestamp_us;
    uint32_t last_tx_timestamp_us;
    uint32_t rx_packet_count;
    uint32_t tx_packet_count;
    uint32_t timeout_count;
    uint32_t crc_error_count;
    uint32_t snn_mismatch_count;
    uint32_t timestamp_mismatch_count;
    uint8_t  safety_validator_active;
    uint32_t safety_validator_crc;
} glx_safety_connection_t;

typedef struct {
    uint16_t source_snn;
    uint16_t dest_snn;
    uint32_t timestamp;
    uint32_t safety_crc;
    uint16_t data_length;
    uint8_t  data[GLX_SAFETY_MAX_PAYLOAD_BYTES];
    uint8_t  crc_valid;
    uint8_t  timestamp_valid;
} glx_safety_data_packet_t;

typedef struct {
    uint8_t  bridge_active;
    uint16_t bridge_snn;
    uint32_t bridge_connections;
    uint8_t  bridge_health_ok;
} glx_safety_network_bridge_t;

int glx_safety_connection_init(glx_safety_connection_t *conn,
                                glx_safety_connection_role_t role,
                                glx_safety_connection_type_t type,
                                uint32_t rpi_us,
                                uint16_t timeout_mult);
int glx_safety_connection_open(glx_safety_connection_t *conn,
                                const glx_safety_connection_id_t *conn_id,
                                uint16_t local_snn,
                                uint16_t remote_snn);
int glx_safety_connection_close(glx_safety_connection_t *conn);
int glx_safety_connection_check_timeout(glx_safety_connection_t *conn,
                                         uint32_t current_time_us);
int glx_safety_data_packet_create(glx_safety_data_packet_t *packet,
                                   uint16_t source_snn,
                                   uint16_t dest_snn,
                                   uint32_t timestamp,
                                   const uint8_t *data,
                                   uint16_t data_len);
int glx_safety_data_packet_validate(const glx_safety_data_packet_t *packet,
                                     uint16_t expected_snn,
                                     uint32_t max_timestamp_delta_us);
uint32_t glx_safety_compute_crtl(uint32_t rpi_us,
                                  uint16_t timeout_multiplier,
                                  uint16_t network_delay_multiplier);
int glx_safety_validate_connection_params(
    const glx_safety_connection_params_t *params,
    uint32_t process_safety_time_us);
uint32_t glx_safety_originator_sn_generate(
    glx_safety_originator_t *originator,
    uint16_t snn);
int glx_safety_network_bridge_init(glx_safety_network_bridge_t *bridge,
                                    uint16_t bridge_snn);
int glx_safety_check_snn_match(uint16_t snn_a, uint16_t snn_b);
void glx_safety_connection_reset_stats(glx_safety_connection_t *conn);
int glx_safety_connection_is_healthy(const glx_safety_connection_t *conn);

#endif /* GUARDLOGIX_SAFETY_NET_H */
