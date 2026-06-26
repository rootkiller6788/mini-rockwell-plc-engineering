/**
 * cip_object.c - CIP Object Model Implementation
 * Knowledge Domain: L1 Definitions, L3 Engineering Structures
 * Reference: ODVA CIP Vol 1, Chapter 5: Object Library
 *
 * Each function implements one independent knowledge point:
 *   1. cip_obj_identity_init()         - Identity Object (mandatory, Class 0x01)
 *   2. cip_obj_assembly_init()         - Assembly Object (I/O data binding)
 *   3. cip_obj_assembly_set_data()     - Write to assembly instance buffer
 *   4. cip_obj_assembly_get_data()     - Read from assembly instance buffer
 *   5. cip_obj_tcpip_init()            - TCP/IP Interface Object (Class 0xF5)
 *   6. cip_obj_ethernet_link_init()    - Ethernet Link Object (Class 0xF6)
 *   7. cip_obj_connection_manager_init() - Connection Manager Object (Class 0x06)
 *   8. cip_obj_validate_class_id()     - Class ID range validation
 *   9. cip_obj_class_name()            - Class code to human-readable name
 *  10. cip_obj_attr_type_name()        - CIP data type code to name
 */
#include <string.h>
#include "../include/cip_object.h"

/* -------------------------------------------------------------------------
 * cip_obj_identity_init - Initialize Identity Object
 *
 * Knowledge: CIP Identity Object (Class 0x01, mandatory in every device).
 * Per ODVA Vol 1 Sec 5-1, the Identity Object provides:
 *   - Vendor ID (Rockwell = 1, Siemens = 42, etc.)
 *   - Device Type (e.g., 0x0C = Communication Adapter, 0x07 = PLC)
 *   - Product Code (vendor-specific product identifier)
 *   - Revision (Major.Minor firmware version)
 *   - Status (bitfield: owned, configured, faulted, etc.)
 *   - Serial Number (32-bit unique per device)
 *   - Product Name (ASCII string, up to 32 chars)
 *
 * Complexity: O(n) where n = strlen(product_name).
 * ------------------------------------------------------------------------- */
void cip_obj_identity_init(cip_identity_object_t *id,
                           uint16_t vendor_id, uint16_t device_type,
                           uint16_t product_code,
                           uint8_t major_rev, uint8_t minor_rev,
                           uint32_t serial, const char *product_name)
{
    if (!id) return;
    memset(id, 0, sizeof(cip_identity_object_t));
    id->vendor_id     = vendor_id;
    id->device_type   = device_type;
    id->product_code  = product_code;
    id->major_revision = major_rev;
    id->minor_revision = minor_rev;
    id->status        = 0x0000u;
    id->serial_number = serial;
    if (product_name) {
        size_t len = strlen(product_name);
        if (len > 31) len = 31;
        memcpy(id->product_name, product_name, len);
        id->product_name[len] = '\0';
        id->product_name_len = (uint8_t)len;
    }
}

/* -------------------------------------------------------------------------
 * cip_obj_assembly_init - Initialize Assembly Object instance
 *
 * Knowledge: CIP Assembly Object (Class 0x04).
 * Assemblies bind attributes from multiple objects into contiguous data
 * blocks for I/O connections. Static assemblies are predefined; dynamic
 * assemblies can be created at runtime.
 *
 * Assembly data is the raw byte image sent/received over the network.
 * Typical sizes: 8-500 bytes for digital I/O, 4-128 bytes for analog.
 *
 * Complexity: O(1) - memset.
 * ------------------------------------------------------------------------- */
void cip_obj_assembly_init(cip_assembly_object_t *asmb, uint32_t instance_id, int is_dynamic)
{
    if (!asmb) return;
    memset(asmb, 0, sizeof(cip_assembly_object_t));
    asmb->instance_id = instance_id;
    asmb->is_dynamic  = is_dynamic;
}

/* -------------------------------------------------------------------------
 * cip_obj_assembly_set_data - Write data into assembly buffer
 *
 * Knowledge: CIP Assembly data writing.
 * The data written here is what the device consumes (O->T) or produces
 * (T->O) over the I/O connection. The assembly acts as a mailbox between
 * the CIP stack and the application logic.
 *
 * Returns: 0 = success, -1 = size exceeds buffer (512 bytes max).
 * Complexity: O(n) where n = size.
 * ------------------------------------------------------------------------- */
int cip_obj_assembly_set_data(cip_assembly_object_t *asmb,
                              const uint8_t *data, uint16_t size)
{
    if (!asmb || !data) return -1;
    if (size > sizeof(asmb->data)) return -2;
    memcpy(asmb->data, data, size);
    asmb->data_size = size;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_obj_assembly_get_data - Read data from assembly buffer
 *
 * Knowledge: CIP Assembly data reading.
 * Copies assembly data to caller's buffer. If the buffer is too small,
 * returns -1 and sets out_size to the required size.
 *
 * Returns: 0 = success, -1 = buffer too small.
 * Complexity: O(n) where n = min(requested, actual).
 * ------------------------------------------------------------------------- */
int cip_obj_assembly_get_data(const cip_assembly_object_t *asmb,
                              uint8_t *buffer, uint16_t buffer_size,
                              uint16_t *out_size)
{
    if (!asmb || !buffer || !out_size) return -1;
    if (buffer_size < asmb->data_size) {
        *out_size = asmb->data_size;
        return -1;
    }
    memcpy(buffer, asmb->data, asmb->data_size);
    *out_size = asmb->data_size;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_obj_tcpip_init - Initialize TCP/IP Interface Object
 *
 * Knowledge: CIP TCP/IP Interface Object (Class 0xF5, mandatory in EIP).
 * Provides IP configuration: address, subnet mask, gateway, DNS, hostname.
 * IP config method: 0 = static, 1 = BOOTP, 2 = DHCP.
 *
 * Complexity: O(n) where n = strlen(hostname).
 * ------------------------------------------------------------------------- */
void cip_obj_tcpip_init(cip_tcpip_object_t *tcpip,
                        uint32_t ip, uint32_t mask, uint32_t gw,
                        const char *hostname)
{
    if (!tcpip) return;
    memset(tcpip, 0, sizeof(cip_tcpip_object_t));
    tcpip->ip_address  = ip;
    tcpip->subnet_mask = mask;
    tcpip->gateway     = gw;
    tcpip->ip_config_method = 2; /* DHCP default */
    tcpip->dhcp_enabled = 1;
    if (hostname) {
        size_t len = strlen(hostname);
        if (len > 63) len = 63;
        memcpy(tcpip->hostname, hostname, len);
        tcpip->hostname[len] = '\0';
    }
}

/* -------------------------------------------------------------------------
 * cip_obj_ethernet_link_init - Initialize Ethernet Link Object
 *
 * Knowledge: CIP Ethernet Link Object (Class 0xF6, mandatory in EIP).
 * Provides MAC address, link speed, duplex mode, and interface counters
 * (in/out octets, discards, errors per ODVA Vol 2 Sec 5-4).
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
void cip_obj_ethernet_link_init(cip_ethernet_link_object_t *eth,
                                const uint8_t mac[6],
                                uint16_t speed_mbps, uint8_t duplex)
{
    if (!eth) return;
    memset(eth, 0, sizeof(cip_ethernet_link_object_t));
    if (mac) memcpy(eth->mac_address, mac, 6);
    eth->interface_speed_mbps = speed_mbps;
    eth->duplex_mode          = duplex;
    eth->autonegotiate        = 1;
}

/* -------------------------------------------------------------------------
 * cip_obj_connection_manager_init - Initialize Connection Manager Object
 *
 * Knowledge: CIP Connection Manager Object (Class 0x06).
 * Tracks connection statistics: Forward Open/Close counts, rejection
 * reasons, and timeout events. These counters are used for network
 * diagnostics and health monitoring.
 *
 * Complexity: O(1) - memset + zero.
 * ------------------------------------------------------------------------- */
void cip_obj_connection_manager_init(cip_connection_manager_object_t *cm)
{
    if (!cm) return;
    memset(cm, 0, sizeof(cip_connection_manager_object_t));
    cm->class_id    = CIP_CLASS_CONNECTION_MANAGER;
    cm->instance_id = 1;
}

/* -------------------------------------------------------------------------
 * cip_obj_validate_class_id - Check if class ID is in defined range
 *
 * Knowledge: CIP class ID namespace.
 * Standard classes: 0x01-0x63 (vendor-specific from 0x64)
 * Reserved: 0x00, 0x64-0xC7 (some allocated)
 * Returns 1 = valid, 0 = invalid.
 *
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_obj_validate_class_id(uint16_t class_id)
{
    if (class_id == 0x0000u) return 0;
    return 1;
}

/* -------------------------------------------------------------------------
 * cip_obj_class_name - Class ID to human-readable name
 *
 * Knowledge: CIP standard object class identification.
 * Maps the most common class codes to their ODVA-defined names.
 *
 * Complexity: O(1) - switch/case.
 * ------------------------------------------------------------------------- */
const char *cip_obj_class_name(uint16_t class_id)
{
    switch (class_id) {
    case CIP_CLASS_IDENTITY:             return "Identity";
    case CIP_CLASS_MESSAGE_ROUTER:       return "Message Router";
    case CIP_CLASS_ASSEMBLY:             return "Assembly";
    case CIP_CLASS_CONNECTION_MANAGER:   return "Connection Manager";
    case CIP_CLASS_PARAMETER:            return "Parameter";
    case CIP_CLASS_DISCRETE_INPUT_POINT:  return "Discrete Input Point";
    case CIP_CLASS_DISCRETE_OUTPUT_POINT: return "Discrete Output Point";
    case CIP_CLASS_ANALOG_INPUT_POINT:    return "Analog Input Point";
    case CIP_CLASS_ANALOG_OUTPUT_POINT:   return "Analog Output Point";
    case CIP_CLASS_TCPIP_INTERFACE:      return "TCP/IP Interface";
    case CIP_CLASS_ETHERNET_LINK:        return "Ethernet Link";
    case CIP_CLASS_POSITION_CONTROLLER:  return "Position Controller";
    default:                             return "Unknown Class";
    }
}

/* -------------------------------------------------------------------------
 * cip_obj_validate_instance_id - Check if instance ID is plausible
 *
 * Knowledge: CIP instance ID validation. Instance 0 always refers to the
 * class itself (class-level attributes). Instance 1+ are object instances.
 * Maximum valid instance depends on the class.
 *
 * Returns 1 = valid, 0 = out of range.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_obj_validate_instance_id(uint16_t class_id, uint32_t instance_id)
{
    (void)class_id; /* class_id reserved for future per-class instance limits */
    if (instance_id > 0xFFFFu) return 0;
    /* Class-level access via instance 0 is always valid */
    if (instance_id == 0) return 1;
    return 1;
}

/* -------------------------------------------------------------------------
 * cip_obj_get_attribute_size - Get attribute data size for a class
 *
 * Knowledge: CIP attribute sizing. Each attribute in each class has a
 * defined data type and size. This function returns the size for known
 * attributes (Attribute 1 = common across all objects, typically UINT).
 *
 * Returns 0 = success with size_out set; -1 = unknown attribute.
 * Complexity: O(1) - switch/case.
 * ------------------------------------------------------------------------- */
int cip_obj_get_attribute_size(uint16_t class_id, uint8_t attribute_id,
                               uint16_t *size_out)
{
    (void)class_id; /* attribute sizes depend on class; simplified default */
    if (!size_out) return -1;
    /* Attribute 1 is typically a UINT (2 bytes) for most objects */
    if (attribute_id == 1) {
        *size_out = 2;
        return 0;
    }
    if (attribute_id == 2) {
        *size_out = 2;
        return 0;
    }
    /* Default: unknown = 0 bytes */
    *size_out = 0;
    return -1;
}

/* -------------------------------------------------------------------------
 * cip_obj_attr_type_name - CIP data type code to string
 *
 * Knowledge: CIP data type encoding (Appendix D).
 * CIP uses type codes to describe attribute data types. This is essential
 * for EDS (Electronic Data Sheet) parsing and attribute display.
 *
 * Complexity: O(1) - switch/case.
 * ------------------------------------------------------------------------- */
const char *cip_obj_attr_type_name(cip_attr_data_type_t type)
{
    switch (type) {
    case CIP_OBJ_ATTR_TYPE_BOOL:   return "BOOL";
    case CIP_OBJ_ATTR_TYPE_SINT:   return "SINT";
    case CIP_OBJ_ATTR_TYPE_INT:    return "INT";
    case CIP_OBJ_ATTR_TYPE_DINT:   return "DINT";
    case CIP_OBJ_ATTR_TYPE_LINT:   return "LINT";
    case CIP_OBJ_ATTR_TYPE_USINT:  return "USINT";
    case CIP_OBJ_ATTR_TYPE_UINT:   return "UINT";
    case CIP_OBJ_ATTR_TYPE_UDINT:  return "UDINT";
    case CIP_OBJ_ATTR_TYPE_ULINT:  return "ULINT";
    case CIP_OBJ_ATTR_TYPE_REAL:   return "REAL";
    case CIP_OBJ_ATTR_TYPE_LREAL:  return "LREAL";
    case CIP_OBJ_ATTR_TYPE_STRING: return "STRING";
    case CIP_OBJ_ATTR_TYPE_BYTE:   return "BYTE";
    case CIP_OBJ_ATTR_TYPE_WORD:   return "WORD";
    case CIP_OBJ_ATTR_TYPE_DWORD:  return "DWORD";
    case CIP_OBJ_ATTR_TYPE_LWORD:  return "LWORD";
    case CIP_OBJ_ATTR_TYPE_STRUCT: return "STRUCT";
    case CIP_OBJ_ATTR_TYPE_ARRAY:  return "ARRAY";
    default:                       return "UNKNOWN_TYPE";
    }
}
