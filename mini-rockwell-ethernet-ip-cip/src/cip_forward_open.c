/**
 * cip_forward_open.c - CIP Forward Open/Close Service Implementation
 * Knowledge Domain: L3 Engineering Structures, L5 Algorithms
 * Reference: ODVA CIP Vol 1, Sec 3-3.4: Forward Open Request/Response
 *
 * Each function implements one independent knowledge point:
 *   1. cip_fo_init_request()          - Forward Open request structure
 *   2. cip_fo_set_connection_params() - O->T / T->O endpoint configuration
 *   3. cip_fo_set_transport_params()  - Transport class & trigger setup
 *   4. cip_fo_set_network_params()    - Connection path & VLAN encoding
 *   5. cip_fo_encode_request()        - Serialize Forward Open to wire format
 *   6. cip_fo_parse_response()        - Parse Forward Open response
 *   7. cip_fo_extract_connection_ids()- Extract O->T/T->O IDs from response
 *   8. cip_fo_init_close_request()    - Forward Close request structure
 *   9. cip_fo_validate_connection_size() - Size limits check
 *  10. cip_fo_calculate_api()         - Actual Packet Interval computation
 */
#include <string.h>
#include "../include/cip_connection.h"
#include "../include/cip_message.h"

/* Forward Open / Forward Close service codes (not in header — internal use) */
#define CIP_FORWARD_OPEN_SERVICE   0x54u
#define CIP_FORWARD_CLOSE_SERVICE  0x4Eu
/* Types cip_forward_open_request_t and cip_forward_open_response_t
 * are defined in cip_connection.h */

/* -------------------------------------------------------------------------
 * cip_fo_init_request - Initialize Forward Open request with defaults
 *
 * Knowledge: Forward Open (0x54) �� the CIP service that establishes
 * I/O connections. This is the most complex CIP service. The originator
 * sends connection parameters (RPI, sizes, connection points, transport
 * class/trigger) and receives Connection IDs in response.
 *
 * Default: priority=high, timeout_ticks=32, transport=Class1+Cyclic.
 * Complexity: O(1) - memset + defaults.
 * ------------------------------------------------------------------------- */
void cip_fo_init_request(cip_forward_open_request_t *fo)
{
    if (!fo) return;
    memset(fo, 0, sizeof(cip_forward_open_request_t));
    fo->priority_timetick = (CIP_CONN_PRIORITY_HIGH << 5) | 0x0Fu;
    fo->timeout_ticks = 32;
    fo->transport_trigger = (CIP_TRANSPORT_CLASS_1 << 4) | CIP_TRIGGER_CYCLIC;
}

/* -------------------------------------------------------------------------
 * cip_fo_set_connection_params - Configure O->T and T->O parameters
 *
 * Knowledge: Forward Open endpoint sizing.
 * O->T (consumed): data the originator sends to the target (outputs)
 * T->O (produced): data the target sends to the originator (inputs)
 *
 * Connection type bits encode the ownership model in transport_trigger.
 *
 * Returns: 0 = success, -1 = size exceeds CIP_MAX_CONNECTION_SIZE.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_fo_set_connection_params(cip_forward_open_request_t *fo,
                                 uint32_t o_to_t_rpi, uint16_t o_to_t_size,
                                 uint32_t t_to_o_rpi, uint16_t t_to_o_size,
                                 uint8_t conn_type)
{
    if (!fo) return -1;
    uint32_t total = (uint32_t)o_to_t_size + (uint32_t)t_to_o_size;
    if (total > CIP_MAX_CONNECTION_SIZE) return -1;

    fo->o_to_t_rpi            = o_to_t_rpi;
    fo->o_to_t_network_params  = o_to_t_size;
    fo->t_to_o_rpi            = t_to_o_rpi;
    fo->t_to_o_network_params  = t_to_o_size;

    /* Encode connection type into transport_trigger byte */
    fo->transport_trigger |= (conn_type & 0x03u);

    return 0;
}

/* -------------------------------------------------------------------------
 * cip_fo_set_transport_params - Configure transport class and trigger
 *
 * Knowledge: Forward Open transport configuration.
 * The transport_trigger byte encodes:
 *   Bits 7-4: Transport Class (0-3)
 *   Bits 3-2: Production Trigger (0=Cyclic, 1=COS, 2=Application)
 *   Bits 1-0: Connection Type
 *
 * Returns: 0 = success, -1 = invalid class/trigger combination.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_fo_set_transport_params(cip_forward_open_request_t *fo,
                                uint8_t transport_class, uint8_t trigger)
{
    if (!fo) return -1;
    if (transport_class > CIP_TRANSPORT_CLASS_3) return -1;
    if (trigger > CIP_TRIGGER_APPLICATION) return -1;

    /* Preserve connection type bits (lower 2 bits) */
    uint8_t conn_type_bits = fo->transport_trigger & 0x03u;
    fo->transport_trigger = (transport_class << 4) | (trigger << 2) | conn_type_bits;

    return 0;
}

/* -------------------------------------------------------------------------
 * cip_fo_set_network_params - Set connection path and optional VLAN
 *
 * Knowledge: CIP connection routing path (network connection path EPATH).
 * The connection path encodes the route from originator to target:
 *   Port segment: which port to use (e.g., backplane port 1)
 *   Link segment: address on that port (e.g., slot 3)
 *
 * For ControlLogix: path = [backplane_port_1, slot_number].
 * VLAN ID (0 = no VLAN, 1-4094 = tagged).
 *
 * Complexity: O(n) where n = path_size (memcpy).
 * ------------------------------------------------------------------------- */
int cip_fo_set_network_params(cip_forward_open_request_t *fo,
                              const uint8_t *path, uint8_t path_size,
                              uint16_t vlan_id)
{
    if (!fo || !path) return -1;
    if (path_size > sizeof(fo->connection_path)) return -2;

    memcpy(fo->connection_path, path, path_size);
    fo->connection_path_size = path_size;

    if (vlan_id > 0 && vlan_id <= 4094) {
        /* VLAN tag goes in O->T network params high bits */
        fo->o_to_t_network_params |= (uint16_t)((vlan_id & 0x0FFFu) << 4);
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * cip_fo_encode_request - Serialize Forward Open request to wire format
 *
 * Knowledge: Forward Open serialization for the Message Router.
 * The Forward Open request is sent as a CIP unconnected explicit message
 * via SendRRData. The service code is 0x54, path is Connection Manager
 * (Class 0x06, Instance 0x01).
 *
 * Returns: total bytes written to buffer.
 * Complexity: O(1) - structure serialization.
 * ------------------------------------------------------------------------- */
int cip_fo_encode_request(const cip_forward_open_request_t *fo,
                          uint8_t *buffer, uint16_t buffer_size)
{
    if (!fo || !buffer) return 0;
    if (buffer_size < 50) return 0; /* minimum request size */

    uint16_t offset = 0;
    buffer[offset++] = fo->priority_timetick;
    buffer[offset++] = fo->timeout_ticks;

    /* O->T Connection ID (4 bytes) */
    buffer[offset++] = (uint8_t)(fo->o_to_t_connection_id & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_connection_id >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_connection_id >> 16) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_connection_id >> 24) & 0xFF);

    /* T->O Connection ID (4 bytes) */
    buffer[offset++] = (uint8_t)(fo->t_to_o_connection_id & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_connection_id >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_connection_id >> 16) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_connection_id >> 24) & 0xFF);

    /* Connection Serial Number (2 bytes) */
    buffer[offset++] = (uint8_t)(fo->connection_serial_number & 0xFF);
    buffer[offset++] = (uint8_t)((fo->connection_serial_number >> 8) & 0xFF);

    /* Originator Vendor ID (2 bytes) */
    buffer[offset++] = (uint8_t)(fo->originator_vendor_id & 0xFF);
    buffer[offset++] = (uint8_t)((fo->originator_vendor_id >> 8) & 0xFF);

    /* Originator Serial Number (4 bytes) */
    buffer[offset++] = (uint8_t)(fo->originator_serial_number & 0xFF);
    buffer[offset++] = (uint8_t)((fo->originator_serial_number >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->originator_serial_number >> 16) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->originator_serial_number >> 24) & 0xFF);

    /* Timeout Multiplier */
    buffer[offset++] = fo->timeout_multiplier;

    /* Reserved (3 bytes) */
    buffer[offset++] = 0; buffer[offset++] = 0; buffer[offset++] = 0;

    /* O->T RPI (4 bytes) */
    buffer[offset++] = (uint8_t)(fo->o_to_t_rpi & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_rpi >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_rpi >> 16) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_rpi >> 24) & 0xFF);

    /* O->T Network Params (2 bytes) */
    buffer[offset++] = (uint8_t)(fo->o_to_t_network_params & 0xFF);
    buffer[offset++] = (uint8_t)((fo->o_to_t_network_params >> 8) & 0xFF);

    /* T->O RPI (4 bytes) */
    buffer[offset++] = (uint8_t)(fo->t_to_o_rpi & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_rpi >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_rpi >> 16) & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_rpi >> 24) & 0xFF);

    /* T->O Network Params (2 bytes) */
    buffer[offset++] = (uint8_t)(fo->t_to_o_network_params & 0xFF);
    buffer[offset++] = (uint8_t)((fo->t_to_o_network_params >> 8) & 0xFF);

    /* Transport + Trigger */
    buffer[offset++] = fo->transport_trigger;

    /* Connection Path Size */
    buffer[offset++] = (uint8_t)(fo->connection_path_size / 2);

    /* Connection Path */
    if (fo->connection_path_size > 0) {
        memcpy(&buffer[offset], fo->connection_path, fo->connection_path_size);
        offset += fo->connection_path_size;
    }

    return (int)offset;
}

/* -------------------------------------------------------------------------
 * cip_fo_parse_response - Parse Forward Open response from wire
 *
 * Knowledge: Forward Open response parsing (ODVA Vol 1 Sec 3-3.4.3).
 * On success, the target returns O->T and T->O Connection IDs plus
 * the Actual Packet Intervals (API) which may differ from RPI.
 *
 * Returns 0 on success, -1 on parse error.
 * Complexity: O(1) - deserialization from fixed format.
 * ------------------------------------------------------------------------- */
int cip_fo_parse_response(const uint8_t *data, uint16_t data_len,
                          cip_forward_open_response_t *resp)
{
    if (!data || !resp) return -1;
    if (data_len < 26) return -2; /* minimum response size */

    uint16_t offset = 0;

    /* Skip service, reserved, status (4 bytes) */
    offset += 4;

    resp->o_to_t_connection_id  = (uint32_t)data[offset]
                               | ((uint32_t)data[offset+1] << 8)
                               | ((uint32_t)data[offset+2] << 16)
                               | ((uint32_t)data[offset+3] << 24);
    offset += 4;

    resp->t_to_o_connection_id  = (uint32_t)data[offset]
                               | ((uint32_t)data[offset+1] << 8)
                               | ((uint32_t)data[offset+2] << 16)
                               | ((uint32_t)data[offset+3] << 24);
    offset += 4;

    resp->connection_serial_number = (uint16_t)data[offset]
                                  | ((uint16_t)data[offset+1] << 8);
    offset += 2;

    resp->originator_vendor_id = (uint16_t)data[offset]
                               | ((uint16_t)data[offset+1] << 8);
    offset += 2;

    resp->originator_serial_number = (uint32_t)data[offset]
                                  | ((uint32_t)data[offset+1] << 8)
                                  | ((uint32_t)data[offset+2] << 16)
                                  | ((uint32_t)data[offset+3] << 24);
    offset += 4;

    resp->o_to_t_api = (uint32_t)data[offset]
                    | ((uint32_t)data[offset+1] << 8)
                    | ((uint32_t)data[offset+2] << 16)
                    | ((uint32_t)data[offset+3] << 24);
    offset += 4;

    resp->t_to_o_api = (uint32_t)data[offset]
                    | ((uint32_t)data[offset+1] << 8)
                    | ((uint32_t)data[offset+2] << 16)
                    | ((uint32_t)data[offset+3] << 24);
    offset += 4;

    resp->application_reply_size = data[offset++];
    resp->reserved = data[offset++];

    return 0;
}

/* -------------------------------------------------------------------------
 * cip_fo_extract_connection_ids - Extract Connection IDs from response
 *
 * Knowledge: CIP Connection ID assignment.
 * The target assigns both O->T and T->O Connection IDs in the Forward
 * Open response. These IDs are used in all subsequent I/O packets.
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
void cip_fo_extract_connection_ids(const cip_forward_open_response_t *resp,
                                   uint32_t *ot_id, uint32_t *to_id)
{
    if (!resp || !ot_id || !to_id) return;
    *ot_id = resp->o_to_t_connection_id;
    *to_id = resp->t_to_o_connection_id;
}

/* -------------------------------------------------------------------------
 * cip_fo_init_close_request - Initialize Forward Close request
 *
 * Knowledge: Forward Close (0x4E) �� terminates a CIP I/O connection.
 * The Forward Close service releases connection resources on the target.
 * Path: Connection Manager Object, Instance = connection point.
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
void cip_fo_init_close_request(uint16_t connection_serial,
                               uint16_t originator_vendor,
                               uint32_t originator_serial,
                               uint8_t *buffer, uint16_t *buf_size)
{
    if (!buffer || !buf_size) return;

    uint16_t offset = 0;
    buffer[offset++] = (uint8_t)(connection_serial & 0xFF);
    buffer[offset++] = (uint8_t)((connection_serial >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)(originator_vendor & 0xFF);
    buffer[offset++] = (uint8_t)((originator_vendor >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)(originator_serial & 0xFF);
    buffer[offset++] = (uint8_t)((originator_serial >> 8) & 0xFF);
    buffer[offset++] = (uint8_t)((originator_serial >> 16) & 0xFF);
    buffer[offset++] = (uint8_t)((originator_serial >> 24) & 0xFF);

    *buf_size = offset;
}

/* -------------------------------------------------------------------------
 * cip_fo_validate_connection_size - Check connection data sizes
 *
 * Knowledge: Forward Open size validation per ODVA Vol 1 Sec 3-4.5.
 * Total connection (O->T + T->O) must not exceed 65511 bytes.
 *
 * Returns: 0 = valid, -1 = exceeds limit.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_fo_validate_connection_size(uint16_t o_to_t_size, uint16_t t_to_o_size)
{
    uint32_t total = (uint32_t)o_to_t_size + (uint32_t)t_to_o_size;
    if (total > CIP_MAX_CONNECTION_SIZE) return -1;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_fo_calculate_api - Calculate API from RPI
 *
 * Knowledge: Actual Packet Interval computation.
 * The API = ceil(RPI / tick) * tick. The target may adjust RPI to API
 * to align with its internal scheduling tick (typically 1ms or 500us).
 *
 * Returns API in microseconds. Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint32_t cip_fo_calculate_api(uint32_t rpi_us, uint32_t tick_us)
{
    if (tick_us == 0) return rpi_us;
    uint32_t q = rpi_us / tick_us;
    uint32_t r = rpi_us % tick_us;
    return (q + (r > 0 ? 1u : 0u)) * tick_us;
}
