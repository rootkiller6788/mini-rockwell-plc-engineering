/**
 * ethernet_ip_encap.c - EtherNet/IP Encapsulation Layer Implementation
 * Knowledge Domain: L3 Engineering Structures, L5 Algorithms
 * Reference: ODVA Vol 2, Chapter 2: Encapsulation Protocol
 *
 * Each function implements one independent knowledge point:
 *   1. eip_encap_init_header()         - 24-byte encapsulation header construction
 *   2. eip_encap_register_session()    - TCP session registration with target
 *   3. eip_encap_unregister_session()  - Session teardown
 *   4. eip_encap_send_rr_data()        - Unconnected messaging (SendRRData)
 *   5. eip_encap_send_unit_data()      - Connected I/O messaging (SendUnitData)
 *   6. eip_encap_parse_header()        - Incoming packet header parser
 *   7. eip_encap_list_identity()       - ListIdentity response construction
 *   8. eip_encap_cpf_build()           - CPF (Common Packet Format) building
 *   9. eip_encap_cpf_add_item()        - Add item to CPF packet
 *  10. eip_encap_validate_session()    - Session validity check
 *  11. eip_encap_calculate_timeout()   - Timeout based on RPI
 *  12. eip_encap_command_string()      - Command code to name
 *  13. eip_encap_status_string()       - Status code to string
 */
#include <string.h>
#include "../include/ethernet_ip_encap.h"

/* -------------------------------------------------------------------------
 * eip_encap_init_header - Construct 24-byte encapsulation header
 *
 * Knowledge: EtherNet/IP encapsulation header format (ODVA Vol 2 Sec 2-3).
 *
 * The 24-byte header is the outer wrapper for all EtherNet/IP messages
 * over TCP and UDP. It contains:
 *   Bytes 0-1:  Command (e.g., 0x006F = SendRRData)
 *   Bytes 2-3:  Length (of data following the header)
 *   Bytes 4-7:  Session Handle (assigned by RegisterSession)
 *   Bytes 8-11: Status (0 = success)
 *   Bytes 12-19: Sender Context (8 bytes, echoed in response)
 *   Bytes 20-23: Options (0 = standard)
 *
 * Complexity: O(1) - fixed-size memset + field assignment.
 * ------------------------------------------------------------------------- */
void eip_encap_init_header(eip_encap_header_t *hdr, uint16_t command,
                           uint16_t length, uint32_t session, uint32_t status)
{
    if (!hdr) return;
    memset(hdr, 0, sizeof(eip_encap_header_t));
    hdr->command       = command;
    hdr->length        = length;
    hdr->session_handle = session;
    hdr->status        = status;
    hdr->options       = 0x00000000u;
}

/* -------------------------------------------------------------------------
 * eip_encap_register_session - Register a new EIP session
 *
 * Knowledge: EtherNet/IP session management (ODVA Vol 2 Sec 2-4.3.6).
 *
 * Before any explicit messaging can occur, the originator must register
 * a session with the target via the RegisterSession command (0x0065).
 * The target returns a 32-bit session handle used in all subsequent
 * messages. Sessions time out after inactivity.
 *
 * Returns: 0 = success, -1 = invalid parameters.
 * Complexity: O(1) - field assignment.
 * ------------------------------------------------------------------------- */
int eip_encap_register_session(eip_session_t *session,
                               uint32_t ip_addr, uint16_t port)
{
    if (!session) return -1;
    memset(session, 0, sizeof(eip_session_t));
    session->session_handle = 0x00000001u; /* assigned by RegisterSession response */
    session->ip_address     = ip_addr;
    session->tcp_port       = port;
    session->is_active      = 1;
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_unregister_session - Tear down an EIP session
 *
 * Knowledge: Session cleanup (UnRegisterSession command 0x0066).
 * Releases the session handle and marks the session inactive.
 *
 * Returns: 0 = success. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int eip_encap_unregister_session(eip_session_t *session)
{
    if (!session) return -1;
    session->is_active = 0;
    session->session_handle = 0;
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_send_rr_data - Send unconnected explicit message (SendRRData)
 *
 * Knowledge: EtherNet/IP UCMM (Unconnected Message Manager).
 * SendRRData (0x006F) wraps a CIP unconnected explicit message in the
 * encapsulation layer. Used for:
 *   - Forward Open / Forward Close requests
 *   - Get_Attribute_Single / Set_Attribute_Single
 *   - Device discovery queries
 *
 * The CPF (Common Packet Format) inside SendRRData contains:
 *   - Interface Handle (0 = CIP)
 *   - Timeout (in seconds)
 *   - Unconnected Data Item (type 0x00B2)
 *
 * Returns: 0 = data accepted, -1 = session invalid, -2 = data too large.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int eip_encap_send_rr_data(eip_session_t *session,
                           const uint8_t *cip_data, uint16_t cip_len,
                           uint32_t timeout_ms)
{
    (void)timeout_ms; /* used by real implementation for response waiting */
    if (!session || !session->is_active) return -1;
    if (!cip_data || cip_len == 0) return -1;
    if (cip_len > EIP_MAX_FRAME_SIZE) return -2;

    /* In a real implementation, this would:
     *   1. Build the CPF header (interface handle, timeout)
     *   2. Wrap with encapsulation header (command=0x006F)
     *   3. Send over TCP to port 44818
     *   4. Wait for response with matching sender_context
     */
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_send_unit_data - Send connected I/O data (SendUnitData)
 *
 * Knowledge: EtherNet/IP Implicit (I/O) Messaging.
 * SendUnitData (0x0070) is used for Class 1 connected I/O data exchange.
 * Unlike SendRRData which uses TCP, SendUnitData typically uses UDP
 * multicast for producer-consumer I/O data.
 *
 * Returns: 0 = data accepted. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int eip_encap_send_unit_data(eip_session_t *session,
                             const uint8_t *cip_data, uint16_t cip_len)
{
    if (!session || !session->is_active) return -1;
    if (!cip_data || cip_len == 0) return -1;
    if (cip_len > EIP_MAX_FRAME_SIZE) return -2;
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_parse_header - Parse incoming encapsulation header
 *
 * Knowledge: EIP packet parsing.
 * Extracts the 24-byte header from a raw byte buffer and validates
 * minimum size. The data_offset output points to the start of the
 * payload (after the 24-byte header).
 *
 * Returns: 0 = valid header, -1 = buffer too small.
 * Complexity: O(1) - memcpy + bounds check.
 * ------------------------------------------------------------------------- */
int eip_encap_parse_header(const uint8_t *raw_data, uint16_t raw_len,
                           eip_encap_header_t *hdr, uint16_t *data_offset)
{
    if (!raw_data || !hdr || !data_offset) return -1;
    if (raw_len < EIP_ENCAP_HEADER_SIZE) return -2;

    memcpy(hdr, raw_data, EIP_ENCAP_HEADER_SIZE);
    *data_offset = EIP_ENCAP_HEADER_SIZE;

    /* Validate length field: header + data <= raw_len */
    if ((uint32_t)EIP_ENCAP_HEADER_SIZE + hdr->length > (uint32_t)raw_len) {
        return -3;
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_list_identity - Build ListIdentity response
 *
 * Knowledge: ListIdentity (0x0063) �� the device discovery mechanism.
 * ListIdentity is the first command sent to discover EtherNet/IP devices
 * on the network. The response contains the device's CIP Identity Object
 * data (vendor, device type, product code, revision, serial, product name).
 *
 * Writes the formatted response to the output buffer.
 * Returns: 0 = success, -1 = buffer too small.
 * Complexity: O(n) where n = product_name_len.
 * ------------------------------------------------------------------------- */
int eip_encap_list_identity(const eip_identity_item_t *id,
                            uint8_t *response, uint16_t *resp_len,
                            uint16_t max_len)
{
    if (!id || !response || !resp_len) return -1;

    uint16_t needed = (uint16_t)(sizeof(eip_identity_item_t));
    if (needed > max_len) {
        *resp_len = needed;
        return -1;
    }

    memset(response, 0, max_len);
    uint16_t offset = 0;

    /* Encap header filled by caller before transmission */
    offset += EIP_ENCAP_HEADER_SIZE;

    /* Item count = 1 */
    response[offset++] = 1; response[offset++] = 0;
    response[offset++] = 0; response[offset++] = 0;

    /* Identity item type = 0x000C */
    response[offset++] = 0x0C; response[offset++] = 0x00;
    /* Item length */
    uint16_t item_len = (uint16_t)(sizeof(eip_identity_item_t) - 4);
    response[offset++] = (uint8_t)(item_len & 0xFF);
    response[offset++] = (uint8_t)((item_len >> 8) & 0xFF);

    /* Protocol version */
    response[offset++] = (uint8_t)(id->protocol_version & 0xFF);
    response[offset++] = (uint8_t)((id->protocol_version >> 8) & 0xFF);

    /* Socket address (16 bytes) �� standard sin_family = AF_INET */
    memcpy(&response[offset], id->socket_addr_sin_family, 2); offset += 2;
    response[offset++] = (uint8_t)(id->socket_addr_sin_port & 0xFF);
    response[offset++] = (uint8_t)((id->socket_addr_sin_port >> 8) & 0xFF);
    memcpy(&response[offset], id->socket_addr_sin_addr, 4); offset += 4;
    memcpy(&response[offset], id->socket_addr_sin_zero, 8); offset += 8;

    /* Vendor ID, Device Type, Product Code */
    response[offset++] = (uint8_t)(id->vendor_id & 0xFF);
    response[offset++] = (uint8_t)((id->vendor_id >> 8) & 0xFF);
    response[offset++] = (uint8_t)(id->device_type & 0xFF);
    response[offset++] = (uint8_t)((id->device_type >> 8) & 0xFF);
    response[offset++] = (uint8_t)(id->product_code & 0xFF);
    response[offset++] = (uint8_t)((id->product_code >> 8) & 0xFF);

    /* Revision */
    response[offset++] = id->major_revision;
    response[offset++] = id->minor_revision;

    /* Status */
    response[offset++] = (uint8_t)(id->status & 0xFF);
    response[offset++] = (uint8_t)((id->status >> 8) & 0xFF);

    /* Serial number */
    response[offset++] = (uint8_t)(id->serial_number & 0xFF);
    response[offset++] = (uint8_t)((id->serial_number >> 8) & 0xFF);
    response[offset++] = (uint8_t)((id->serial_number >> 16) & 0xFF);
    response[offset++] = (uint8_t)((id->serial_number >> 24) & 0xFF);

    /* Product name */
    response[offset++] = id->product_name_len;
    if (id->product_name_len > 0 && id->product_name_len <= 32) {
        memcpy(&response[offset], id->product_name, id->product_name_len);
        offset += id->product_name_len;
    }

    /* State */
    response[offset++] = id->state;

    *resp_len = offset;
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_cpf_build - Initialize a CPF (Common Packet Format) packet
 *
 * Knowledge: CPF format (ODVA Vol 2 Sec 2-6.3).
 * The CPF wraps address items and data items for CIP routing.
 * Item count starts at 0; items are added via eip_encap_cpf_add_item().
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int eip_encap_cpf_build(eip_cpf_packet_t *cpf,
                        uint16_t iface_handle, uint16_t timeout)
{
    if (!cpf) return -1;
    memset(cpf, 0, sizeof(eip_cpf_packet_t));
    cpf->item_count      = 0;
    cpf->interface_handle = iface_handle;
    cpf->timeout         = timeout;
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_cpf_add_item - Add an item to a CPF packet
 *
 * Knowledge: CPF item encoding.
 * Each CPF item has: Type ID (2 bytes), Length (2 bytes), Data (variable).
 * Common types: 0x0000 = Null, 0x00B2 = Unconnected Data,
 *               0x00A1 = Connected Data, 0x8000 = Socket Address.
 *
 * Returns: 0 = added, -1 = packet full (max 8 items).
 * Complexity: O(n) where n = data_len (memcpy).
 * ------------------------------------------------------------------------- */
int eip_encap_cpf_add_item(eip_cpf_packet_t *cpf, uint16_t type_code,
                           const uint8_t *data, uint16_t data_len)
{
    if (!cpf || !data) return -1;
    if (cpf->item_count >= 8) return -2;
    if (data_len > sizeof(cpf->items[0].data)) return -3;

    eip_cpf_item_t *item = &cpf->items[cpf->item_count];
    item->item_type_code = type_code;
    item->item_length    = data_len;
    memcpy(item->data, data, data_len);
    cpf->item_count++;
    return 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_validate_session - Check session validity
 *
 * Knowledge: EIP session state check.
 * A valid session must be active and have a non-zero session handle.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int eip_encap_validate_session(const eip_session_t *session)
{
    if (!session) return 0;
    return session->is_active && session->session_handle != 0 ? 1 : 0;
}

/* -------------------------------------------------------------------------
 * eip_encap_calculate_timeout - Calculate encapsulation timeout
 *
 * Knowledge: EIP timeout formula.
 * Timeout = RPI * 4 * multiplier, same formula as CIP connection timeout.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint32_t eip_encap_calculate_timeout(uint32_t rpi_us, uint8_t multiplier)
{
    if (multiplier == 0) multiplier = 4;
    uint32_t timeout = rpi_us * 4u * (uint32_t)multiplier;
    if (timeout > 3600000000u) timeout = 3600000000u;
    return timeout;
}

/* -------------------------------------------------------------------------
 * eip_encap_command_string - Command code to name
 *
 * Knowledge: EIP command identification for debugging.
 * Complexity: O(1) - switch/case.
 * ------------------------------------------------------------------------- */
const char *eip_encap_command_string(uint16_t command)
{
    switch (command) {
    case EIP_ENCAP_COMMAND_NOP:               return "NOP";
    case EIP_ENCAP_COMMAND_LIST_SERVICES:     return "ListServices";
    case EIP_ENCAP_COMMAND_LIST_IDENTITY:     return "ListIdentity";
    case EIP_ENCAP_COMMAND_LIST_INTERFACES:   return "ListInterfaces";
    case EIP_ENCAP_COMMAND_REGISTER_SESSION:  return "RegisterSession";
    case EIP_ENCAP_COMMAND_UNREGISTER_SESSION:return "UnRegisterSession";
    case EIP_ENCAP_COMMAND_SEND_RR_DATA:      return "SendRRData";
    case EIP_ENCAP_COMMAND_SEND_UNIT_DATA:    return "SendUnitData";
    case EIP_ENCAP_COMMAND_INDICATE_STATUS:   return "IndicateStatus";
    case EIP_ENCAP_COMMAND_CANCEL:            return "Cancel";
    default:                                  return "Unknown Command";
    }
}

/* -------------------------------------------------------------------------
 * eip_encap_status_string - Status code to name
 *
 * Knowledge: EIP status interpretation.
 * Complexity: O(1) - switch/case.
 * ------------------------------------------------------------------------- */
const char *eip_encap_status_string(uint32_t status)
{
    switch (status) {
    case EIP_ENCAP_STATUS_SUCCESS:            return "Success";
    case EIP_ENCAP_STATUS_INVALID_COMMAND:    return "Invalid Command";
    case EIP_ENCAP_STATUS_INSUFFICIENT_MEMORY:return "Insufficient Memory";
    case EIP_ENCAP_STATUS_INCORRECT_DATA:     return "Incorrect Data";
    case EIP_ENCAP_STATUS_INVALID_SESSION:    return "Invalid Session";
    case EIP_ENCAP_STATUS_INVALID_LENGTH:     return "Invalid Length";
    case EIP_ENCAP_STATUS_UNSUPPORTED_PROTOCOL:return "Unsupported Protocol";
    default:                                  return "Unknown Status";
    }
}
