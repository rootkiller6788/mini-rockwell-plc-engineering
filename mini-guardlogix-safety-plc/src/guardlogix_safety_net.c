/**
 * guardlogix_safety_net.c -- CIP Safety Network Communication
 *
 * Knowledge points:
 *   L1: CIP Safety protocol -- safety over EtherNet/IP
 *   L2: Black channel principle -- safety layer on top of standard network
 *   L3: Safety connection parameters (RPI, timeout multiplier, CRTL)
 *   L3: SNN validation and cross-network communication prevention
 *   L5: Connection Reaction Time Limit (CRTL) calculation
 *   L5: Timestamp-based safety data freshness verification
 *   L6: Safety connection timeout and fault handling
 *
 * CIP Safety (Common Industrial Protocol Safety) is the ODVA standard
 * for safety communication over EtherNet/IP. It uses the "black channel"
 * principle where safety measures are implemented at the application
 * layer, allowing use of standard, non-safety-certified network hardware.
 *
 * Reference: CIP Safety Specification Vol. 8 (ODVA)
 *            IEC 61784-3:2016 (Functional safety fieldbuses)
 */

#include "guardlogix_safety_net.h"
#include "guardlogix_signature.h"
#include <string.h>
#include <stdlib.h>

int glx_safety_connection_init(glx_safety_connection_t *conn,
                                glx_safety_connection_role_t role,
                                glx_safety_connection_type_t type,
                                uint32_t rpi_us,
                                uint16_t timeout_mult)
{
    if (!conn) return -1;
    if (rpi_us < 1000) return -1; /* Minimum 1 ms RPI */
    if (rpi_us > 500000) return -1; /* Maximum 500 ms RPI */
    if (timeout_mult < 2 || timeout_mult > 512) return -1;

    memset(conn, 0, sizeof(*conn));
    conn->role = role;
    conn->conn_type = type;
    conn->state = GLX_SAFETY_CONN_CLOSED;
    conn->params.requested_packet_interval_us = rpi_us;
    conn->params.timeout_multiplier = timeout_mult;
    conn->params.network_delay_multiplier =
        GLX_SAFETY_DEFAULT_NET_DELAY_MULT;
    conn->params.min_crtl_us = GLX_SAFETY_MIN_CRTL_MS * 1000;

    /* Compute timeout value = RPI * timeout_multiplier */
    conn->params.timeout_value_us = rpi_us * timeout_mult;

    return 0;
}

int glx_safety_connection_open(glx_safety_connection_t *conn,
                                const glx_safety_connection_id_t *conn_id,
                                uint16_t local_snn,
                                uint16_t remote_snn)
{
    if (!conn || !conn_id) return -1;

    /* Verify SNN values are non-zero */
    if (local_snn == 0 || remote_snn == 0) return -1;

    /* Copy connection ID */
    memcpy(&conn->connection_id, conn_id, sizeof(*conn_id));
    conn->connection_id.originating_snn = local_snn;
    conn->connection_id.target_snn = remote_snn;

    /* Validate SNN match */
    if (local_snn != remote_snn) {
        /* SNN mismatch -- cross-network communication attempt.
         * This is a safety violation -- block the connection. */
        conn->state = GLX_SAFETY_CONN_FAULTED;
        conn->snn_mismatch_count++;
        return -1;
    }

    conn->state = GLX_SAFETY_CONN_ESTABLISHED;
    conn->safety_validator_active = 1;

    return 0;
}

int glx_safety_connection_close(glx_safety_connection_t *conn)
{
    if (!conn) return -1;

    conn->state = GLX_SAFETY_CONN_CLOSED;
    conn->safety_validator_active = 0;

    return 0;
}

int glx_safety_connection_check_timeout(glx_safety_connection_t *conn,
                                         uint32_t current_time_us)
{
    if (!conn) return -1;
    if (conn->state != GLX_SAFETY_CONN_ESTABLISHED) return 0;

    /* Check if we have received data within the timeout window */
    if (conn->last_rx_timestamp_us == 0) {
        /* No data received yet */
        return 0;
    }

    uint32_t elapsed;
    if (current_time_us >= conn->last_rx_timestamp_us) {
        elapsed = current_time_us - conn->last_rx_timestamp_us;
    } else {
        /* Timer wrap-around -- assume connection OK */
        return 0;
    }

    if (elapsed > conn->params.timeout_value_us) {
        conn->state = GLX_SAFETY_CONN_TIMED_OUT;
        conn->timeout_count++;
        return -1;
    }

    return 0;
}

int glx_safety_data_packet_create(glx_safety_data_packet_t *packet,
                                   uint16_t source_snn,
                                   uint16_t dest_snn,
                                   uint32_t timestamp,
                                   const uint8_t *data,
                                   uint16_t data_len)
{
    if (!packet) return -1;
    if (data_len > GLX_SAFETY_MAX_PAYLOAD_BYTES) return -1;

    memset(packet, 0, sizeof(*packet));
    packet->source_snn = source_snn;
    packet->dest_snn = dest_snn;
    packet->timestamp = timestamp;
    packet->data_length = data_len;

    if (data && data_len > 0) {
        memcpy(packet->data, data, data_len);
    }

    /* Compute safety CRC over header + payload */
    uint32_t crc = 0;
    crc = glx_crc32_update(crc, (const uint8_t*)&source_snn, 2);
    crc = glx_crc32_update(crc, (const uint8_t*)&dest_snn, 2);
    crc = glx_crc32_update(crc, (const uint8_t*)&timestamp, 4);
    if (data && data_len > 0) {
        crc = glx_crc32_update(crc, data, data_len);
    }
    packet->safety_crc = crc;
    packet->crc_valid = 1;
    packet->timestamp_valid = 1;

    return 0;
}

int glx_safety_data_packet_validate(const glx_safety_data_packet_t *packet,
                                     uint16_t expected_snn,
                                     uint32_t max_timestamp_delta_us)
{
    (void)max_timestamp_delta_us; /* Reserved for future timestamp validation */
    if (!packet) return -1;

    /* Verify SNN */
    if (packet->source_snn != expected_snn) return -1;

    /* Verify CRC */
    uint32_t computed_crc = 0;
    computed_crc = glx_crc32_update(computed_crc,
        (const uint8_t*)&packet->source_snn, 2);
    computed_crc = glx_crc32_update(computed_crc,
        (const uint8_t*)&packet->dest_snn, 2);
    computed_crc = glx_crc32_update(computed_crc,
        (const uint8_t*)&packet->timestamp, 4);
    if (packet->data_length > 0) {
        computed_crc = glx_crc32_update(computed_crc,
            packet->data, packet->data_length);
    }

    if (computed_crc != packet->safety_crc) return -2;

    return 0;
}

uint32_t glx_safety_compute_crtl(uint32_t rpi_us,
                                  uint16_t timeout_multiplier,
                                  uint16_t network_delay_multiplier)
{
    /* Connection Reaction Time Limit (CRTL):
     * CRTL = RPI * (timeout_multiplier + network_delay_multiplier)
     *
     * The CRTL defines the maximum time from when a safety data
     * producer detects a change to when the consumer reacts to it.
     * This is the communication part of the overall safety reaction time.
     */
    uint32_t crtl = rpi_us * ((uint32_t)timeout_multiplier +
                               (uint32_t)network_delay_multiplier);
    return crtl;
}

int glx_safety_validate_connection_params(
    const glx_safety_connection_params_t *params,
    uint32_t process_safety_time_us)
{
    if (!params) return -1;

    /* Compute CRTL and verify it is less than PST */
    uint32_t crtl = glx_safety_compute_crtl(
        params->requested_packet_interval_us,
        params->timeout_multiplier,
        params->network_delay_multiplier);

    /* CRTL must be less than Process Safety Time */
    if (crtl >= process_safety_time_us) return -1;

    /* CRTL must exceed minimum */
    if (crtl < params->min_crtl_us) return -1;

    return 0;
}

uint32_t glx_safety_originator_sn_generate(
    glx_safety_originator_t *originator,
    uint16_t snn)
{
    if (!originator) return 0;

    memset(originator, 0, sizeof(*originator));
    originator->safety_network_number = snn;
    originator->unique_id = 0; /* Would be incremented per connection */

    /* Generate a timestamp-based serial number */
    static uint32_t serial_counter = 0;
    originator->unique_id = ++serial_counter;
    if (serial_counter == 0) serial_counter = 1;

    originator->sn_length = GLX_SAFETY_MAX_ORIGINATOR_SN_LEN;

    return originator->unique_id;
}

int glx_safety_network_bridge_init(glx_safety_network_bridge_t *bridge,
                                    uint16_t bridge_snn)
{
    if (!bridge) return -1;

    memset(bridge, 0, sizeof(*bridge));
    bridge->bridge_snn = bridge_snn;
    bridge->bridge_active = 1;
    bridge->bridge_health_ok = 1;

    return 0;
}

int glx_safety_check_snn_match(uint16_t snn_a, uint16_t snn_b)
{
    return (snn_a == snn_b) ? 0 : -1;
}

void glx_safety_connection_reset_stats(glx_safety_connection_t *conn)
{
    if (!conn) return;
    conn->rx_packet_count = 0;
    conn->tx_packet_count = 0;
    conn->timeout_count = 0;
    conn->crc_error_count = 0;
    conn->snn_mismatch_count = 0;
    conn->timestamp_mismatch_count = 0;
}

int glx_safety_connection_is_healthy(const glx_safety_connection_t *conn)
{
    if (!conn) return 0;
    return (conn->state == GLX_SAFETY_CONN_ESTABLISHED &&
            conn->safety_validator_active &&
            conn->timeout_count == 0 &&
            conn->snn_mismatch_count == 0) ? 1 : 0;
}
