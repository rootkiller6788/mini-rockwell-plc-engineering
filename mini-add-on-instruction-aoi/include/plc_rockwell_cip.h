/**
 * @file plc_rockwell_cip.h
 * @brief Common Industrial Protocol (CIP) ? EtherNet/IP, Tag Read/Write, Messaging
 *
 * Knowledge Coverage: L1 ? L2 ? L3 ? L4 ? L5 ? L7
 *
 * CIP (Common Industrial Protocol) is the application-layer protocol used by
 * Rockwell controllers for:
 *   - Implicit (I/O) messaging: Real-time cyclic data exchange via UDP
 *   - Explicit messaging: Acyclic request/response via TCP
 *   - Tag read/write: Access controller tags from HMI, SCADA, or other controllers
 *   - Controller management: Firmware updates, configuration changes
 *
 * CIP is defined by ODVA (Open DeviceNet Vendors Association) and is used across
 * EtherNet/IP, DeviceNet, ControlNet, and CompoNet. This implementation focuses
 * on EtherNet/IP (CIP Volume 2).
 *
 * Reference: ODVA CIP Specification Vol 1 (Common Aspects)
 *            ODVA CIP Specification Vol 2 (EtherNet/IP Adaptation)
 *            Rockwell 1756-PM020 (EtherNet/IP Communication Modules)
 */

#ifndef PLC_ROCKWELL_CIP_H
#define PLC_ROCKWELL_CIP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: CIP Service Codes ? Standard CIP object operations
 * ============================================================================ */

#define CIP_SERVICE_GET_ATTRIBUTE_ALL    0x01
#define CIP_SERVICE_SET_ATTRIBUTE_ALL    0x02
#define CIP_SERVICE_GET_ATTRIBUTE_SINGLE 0x0E
#define CIP_SERVICE_SET_ATTRIBUTE_SINGLE 0x10
#define CIP_SERVICE_GET_ATTRIBUTE_LIST   0x03
#define CIP_SERVICE_SET_ATTRIBUTE_LIST   0x04
#define CIP_SERVICE_RESET               0x05
#define CIP_SERVICE_START               0x06
#define CIP_SERVICE_STOP                0x07
#define CIP_SERVICE_CREATE              0x08
#define CIP_SERVICE_DELETE              0x09
#define CIP_SERVICE_MULTIPLE_SERVICE    0x0A
#define CIP_SERVICE_APPLY_ATTRIBUTES    0x0D
#define CIP_SERVICE_SAVE                0x16
#define CIP_SERVICE_RESTORE             0x17
#define CIP_SERVICE_FORWARD_OPEN        0x54
#define CIP_SERVICE_FORWARD_CLOSE       0x4E
#define CIP_SERVICE_LARGE_FORWARD_OPEN  0x5B
#define CIP_SERVICE_READ_TAG            0x4C
#define CIP_SERVICE_READ_TAG_FRAGMENTED 0x52
#define CIP_SERVICE_WRITE_TAG           0x4D
#define CIP_SERVICE_WRITE_TAG_FRAGMENTED 0x53
#define CIP_SERVICE_READ_MODIFY_WRITE   0x4E

/* ============================================================================
 * L1: CIP Object Class IDs ? Standard CIP objects
 * ============================================================================ */

#define CIP_CLASS_IDENTITY            0x01
#define CIP_CLASS_MESSAGE_ROUTER      0x02
#define CIP_CLASS_DEVICENET           0x03
#define CIP_CLASS_ASSEMBLY            0x04
#define CIP_CLASS_CONNECTION          0x05
#define CIP_CLASS_CONNECTION_MANAGER  0x06
#define CIP_CLASS_REGISTER            0x07
#define CIP_CLASS_DISCRETE_INPUT      0x08
#define CIP_CLASS_DISCRETE_OUTPUT     0x09
#define CIP_CLASS_ANALOG_INPUT        0x0A
#define CIP_CLASS_ANALOG_OUTPUT       0x0B
#define CIP_CLASS_PRESENCE_SENSING    0x0E
#define CIP_CLASS_PARAMETER           0x0F
#define CIP_CLASS_PARAMETER_GROUP     0x10
#define CIP_CLASS_GROUP               0x12
#define CIP_CLASS_TIME_SYNC           0x43
#define CIP_CLASS_MOTION_DEVICE       0x43
#define CIP_CLASS_POSITION_SENSOR     0x23
#define CIP_CLASS_FILE                0x37
#define CIP_CLASS_ORIGINATOR_CONNECTION_LIST 0x45
#define CIP_CLASS_TCPIP_INTERFACE     0xF5
#define CIP_CLASS_ETHERNET_LINK       0xF6
#define CIP_CLASS_SAFETY_VALVE        0x3F
#define CIP_CLASS_SAFETY_RELAY        0x40

/* ============================================================================
 * L1: CIP Connection Types
 * ============================================================================ */

/** CIP connection transport types */
typedef enum {
    CIP_CONN_NONE          = 0,  /**< No connection                        */
    CIP_CONN_EXPLICIT      = 1,  /**< Class 3 explicit messaging (TCP)     */
    CIP_CONN_IMPLICIT_IO   = 2,  /**< Class 1 implicit I/O (UDP multicast) */
    CIP_CONN_LISTEN_ONLY   = 3,  /**< Listen-only I/O connection           */
    CIP_CONN_INPUT_ONLY    = 4,  /**< Input-only I/O (monitoring)          */
    CIP_CONN_REDUNDANT_OWNER=5, /**< Redundant owner of output connection  */
    CIP_CONN_UNCONNECTED   = 6   /**< UCMM (UnConnected Message Manager)   */
} cip_connection_type_t;

/** CIP connection trigger types (production triggers) */
typedef enum {
    CIP_TRIGGER_CYCLIC     = 0,  /**< Fixed interval (RPI)                  */
    CIP_TRIGGER_COS        = 1,  /**< Change of State                       */
    CIP_TRIGGER_APPLICATION = 2  /**< Application object triggers           */
} cip_trigger_type_t;

/** CIP connection transport class */
typedef enum {
    CIP_TRANSPORT_CLASS_0  = 0,  /**< Pure I/O (fixed format)              */
    CIP_TRANSPORT_CLASS_1  = 1,  /**< I/O with optional data               */
    CIP_TRANSPORT_CLASS_2  = 2,  /**< Explicit messaging (TCP)             */
    CIP_TRANSPORT_CLASS_3  = 3,  /**< Connected explicit (TCP)             */
} cip_transport_class_t;

/* ============================================================================
 * L2: CIP Connection Parameters
 * ============================================================================ */

/** Forward Open request parameters */
typedef struct {
    uint8_t                 priority;           /**< 0=low, 1=high, 2=scheduled */
    uint8_t                 tick_time;          /**< 0-15 (2^tick ms)            */
    uint8_t                 timeout_ticks;      /**< Connection timeout          */
    uint32_t                o_to_t_network_conn_id;/**< Originator->Target CID   */
    uint32_t                t_to_o_network_conn_id;/**< Target->Originator CID   */
    cip_trigger_type_t      transport_trigger;  /**< Cyclic/COS/Application      */
    uint16_t                connection_serial;  /**< Vendor-specific serial      */
    uint16_t                originator_vendor;  /**< ODVA vendor ID              */
    uint32_t                originator_serial;  /**< Originator device serial    */
    uint32_t                connection_timeout_mult; /**< Timeout multiplier    */
    uint32_t                o_to_t_rpi_ns;      /**< O->T RPI (nanoseconds)     */
    cip_transport_class_t   o_to_t_class;       /**< O->T transport class        */
    uint32_t                t_to_o_rpi_ns;      /**< T->O RPI (nanoseconds)     */
    cip_transport_class_t   t_to_o_class;       /**< T->O transport class        */
    uint16_t                o_to_t_size;        /**< O->T data size (bytes)     */
    uint16_t                t_to_o_size;        /**< T->O data size (bytes)     */
    char                    connection_path[256];/**< EPath to target object      */
    uint16_t                connection_path_len;/**< EPath length (words)        */
} cip_forward_open_params_t;

/* ============================================================================
 * L3: CIP Message Packet Structure
 * ============================================================================
 * The CIP explicit message packet is embedded within EtherNet/IP frames.
 * The packet format:
 *
 *   [ItemCount:UINT16] [AddressItem:UINT16][AddrLen:UINT16][AddrData...]
 *                      [DataItem:UINT16][DataLen:UINT16][CIPMessage...]
 *
 * The CIP Message itself:
 *   [Service:1B] [RequestPathSize:1B] [RequestPath:N words]
 *   [RequestData...]
 */

#define CIP_ENCAP_HEADER_SIZE      24
#define CIP_MAX_MESSAGE_SIZE       1500
#define CIP_MAX_EPATH_SEGMENTS     12
#define CIP_EPATH_SEGMENT_SIZE     2

/** CIP Electronic Path Segment (EPATH) */
typedef struct {
    uint8_t     segment_type;       /**< Class(0x20)/Instance(0x24)/Attr(0x30)
                                         Logical(0x40)/Symbolic(0x80)         */
    uint8_t     segment_format;     /**< 8/16/32-bit value format              */
    union {
        uint8_t     value_8;
        uint16_t    value_16;
        uint32_t    value_32;
        char        value_symbolic[32]; /**< Symbolic (ANSI extended) segment   */
    } segment_data;
} cip_epath_segment_t;

/** CIP electronic path (logical route to an object) */
typedef struct {
    uint16_t            segment_count;
    cip_epath_segment_t segments[CIP_MAX_EPATH_SEGMENTS];
} cip_epath_t;

/** CIP request (explicit message) */
typedef struct {
    uint8_t         service_code;       /**< Get/Set/Reset/etc.               */
    cip_epath_t     request_path;       /**< Class/Instance/Attribute path    */
    uint8_t         request_data[CIP_MAX_MESSAGE_SIZE];
    uint16_t        request_data_len;    /**< Length of request data           */
    uint8_t         sender_context[8];   /**< Transaction identifier           */
    uint32_t        transaction_id;      /**< Rolling transaction counter       */
} cip_request_t;

/** CIP response (explicit message) */
typedef struct {
    uint8_t         service_code;       /**< Echoed service | 0x80              */
    uint8_t         reserved;           /**< Reserved (0)                       */
    uint8_t         general_status;     /**< 0=Success, else error code         */
    uint8_t         extended_status_size;/**< Size of extended status            */
    uint16_t        extended_status[4];  /**< Extended status (vendor-specific) */
    uint8_t         response_data[CIP_MAX_MESSAGE_SIZE];
    uint16_t        response_data_len;
} cip_response_t;

/** CIP error status codes */
typedef enum {
    CIP_STATUS_SUCCESS              = 0x00,
    CIP_STATUS_CONNECTION_FAILURE   = 0x01,
    CIP_STATUS_RESOURCE_UNAVAILABLE = 0x02,
    CIP_STATUS_INVALID_PARAMETER    = 0x03,
    CIP_STATUS_PATH_SEGMENT_ERROR   = 0x04,
    CIP_STATUS_PATH_DEST_UNKNOWN    = 0x05,
    CIP_STATUS_PARTIAL_TRANSFER     = 0x06,
    CIP_STATUS_CONNECTION_LOST      = 0x07,
    CIP_STATUS_SERVICE_NOT_SUPPORTED= 0x08,
    CIP_STATUS_INVALID_ATTRIBUTE    = 0x09,
    CIP_STATUS_ATTRIBUTE_LIST_ERROR = 0x0A,
    CIP_STATUS_ALREADY_IN_MODE      = 0x0B,
    CIP_STATUS_OBJECT_STATE_CONFLICT= 0x0C,
    CIP_STATUS_OBJECT_ALREADY_EXISTS= 0x0D,
    CIP_STATUS_ATTRIBUTE_NOT_SETTABLE=0x0E,
    CIP_STATUS_PRIVILEGE_VIOLATION  = 0x0F,
    CIP_STATUS_DEVICE_STATE_CONFLICT= 0x10,
    CIP_STATUS_REPLY_DATA_TOO_LARGE = 0x11,
    CIP_STATUS_NOT_ENOUGH_DATA      = 0x13,
    CIP_STATUS_ATTRIBUTE_NOT_SUPPORTED=0x14,
    CIP_STATUS_TOO_MUCH_DATA        = 0x15,
    CIP_STATUS_OBJECT_DOES_NOT_EXIST= 0x16,
    CIP_STATUS_SERVICE_SEQUENCE_ERROR=0x19,
    CIP_STATUS_NO_STORED_ATTRIBUTE  = 0x1A,
    CIP_STATUS_STORE_OPERATION_FAILURE=0x1B,
    CIP_STATUS_INVALID_ORIGINATOR   = 0x1E,
    CIP_STATUS_INVALID_TARGET       = 0x20,
    CIP_STATUS_WRONG_MODULE         = 0x24
} cip_status_code_t;

/* ============================================================================
 * L2: CIP Identity Object ? Device identification
 * ============================================================================
 * Every CIP device implements the Identity Object (Class 0x01, Instance 0x01).
 * This is the mandatory root object that provides device identification.
 *
 * Identity Object Attributes:
 *   - Vendor ID (ODVA-assigned): Rockwell = 1
 *   - Device Type: PLC (14), Communication Adapter (12)
 *   - Product Code: Model-specific
 *   - Revision: Major.Minor
 *   - Status: Owned, Configured, Minor Fault, Major Fault
 *   - Serial Number: Unique 32-bit
 *   - Product Name: Human-readable string
 */

/** CIP identity object structure */
typedef struct {
    uint16_t    vendor_id;          /**< ODVA vendor ID (Rockwell = 1)     */
    uint16_t    device_type;        /**< ODVA device profile               */
    uint16_t    product_code;       /**< Vendor-assigned product code      */
    uint8_t     major_revision;     /**< Firmware major revision           */
    uint8_t     minor_revision;     /**< Firmware minor revision           */
    uint16_t    status;             /**< Device status word                */
    uint32_t    serial_number;      /**< Unique device serial number       */
    char        product_name[32];   /**< Human-readable product name       */
    uint8_t     state;              /**< 0=Nonexistent, 1=Self-test,       */
                                    /**<   2=Standby, 3=Operational,       */
                                    /**<   4=Major Recoverable, 5=Major    */
                                    /**<   Unrecoverable, 255=Default      */
} cip_identity_t;

/* Device status bit masks */
#define CIP_STATUS_OWNED          0x0001  /**< Device has an owner         */
#define CIP_STATUS_CONFIGURED     0x0004  /**< Device is configured        */
#define CIP_STATUS_MINOR_FAULT    0x0100  /**< Minor recoverable fault     */
#define CIP_STATUS_MAJOR_FAULT    0x0200  /**< Major unrecoverable fault   */

/* ============================================================================
 * L2: CIP Assembly Object ? I/O data grouping
 * ============================================================================
 * Assembly objects group data attributes into single-access blocks for
 * implicit (I/O) messaging. This is how Logix I/O data is packed into
 * Ethernet frames.
 */

/** CIP assembly object */
typedef struct {
    uint32_t    instance_id;        /**< Assembly instance number             */
    uint16_t    data_size;          /**< Byte count of assembly data          */
    uint8_t     data[1024];         /**< Assembly data buffer                 */
    bool        is_input;           /**< true=Input assembly (to scanner)     */
} cip_assembly_t;

/* ============================================================================
 * L2: CIP Connection Object ? Active connection instance
 * ============================================================================
 * Each active CIP connection is represented by a Connection Object
 * (Class 0x05). This tracks the state, timing, and data buffers for
 * one logical CIP connection.
 */

/** CIP connection states */
typedef enum {
    CIP_CONN_STATE_NONEXISTENT  = 0,   /**< Connection does not exist        */
    CIP_CONN_STATE_CONFIGURING  = 1,   /**< Forward Open in progress          */
    CIP_CONN_STATE_WAITING      = 2,   /**< Waiting for connection ID         */
    CIP_CONN_STATE_ESTABLISHED  = 3,   /**< Connection active                 */
    CIP_CONN_STATE_TIMED_OUT    = 4,   /**< Connection timeout                */
    CIP_CONN_STATE_DEFERRED     = 5,   /**< Deferred deletion                 */
    CIP_CONN_STATE_CLOSING      = 6    /**< Close in progress                 */
} cip_conn_state_t;

/** CIP connection object (Class 0x05 instance) */
typedef struct {
    uint32_t            connection_id;      /**< Unique connection ID        */
    cip_connection_type_t conn_type;        /**< Explicit/Implicit/etc.      */
    cip_conn_state_t    state;              /**< Current connection state     */
    uint16_t            o_to_t_instance;    /**< O->T connection point       */
    uint16_t            t_to_o_instance;    /**< T->O connection point       */
    uint16_t            o_to_t_size;        /**< O->T maximum data size      */
    uint16_t            t_to_o_size;        /**< T->O maximum data size      */
    uint32_t            requested_packet_interval; /**< RPI (us)             */
    uint32_t            actual_packet_interval;    /**< Actual interval (us) */
    uint32_t            watchdog_timeout;   /**< Inactivity timeout (us)     */
    uint32_t            production_trigger; /**< Cyclic/COS/Application       */
    /* Transport layer */
    uint32_t            o_to_t_ip;          /**< O->T unicast IP              */
    uint16_t            o_to_t_udp_port;    /**< O->T UDP port                */
    uint32_t            t_to_o_ip;          /**< T->O IP (unicast/multicast) */
    uint16_t            t_to_o_udp_port;    /**< T->O UDP port                */
    /* Statistics */
    uint64_t            packets_sent;       /**< Total packets produced       */
    uint64_t            packets_received;   /**< Total packets consumed       */
    uint32_t            sequence_number;    /**< Rolling sequence number      */
    uint32_t            timeout_count;      /**< Number of timeouts           */
    uint32_t            last_packet_time_ms;/**< Timestamp of last packet     */
} cip_connection_t;

/* ============================================================================
 * L5: Algorithms ? Logix 5000 Tag Read/Write Service
 * ============================================================================
 * The Tag Read (0x4C) and Tag Write (0x4D) services provide direct access to
 * Logix controller tags over CIP. These are the most commonly used explicit
 * message services in Rockwell automation systems.
 *
 * Tag Read Protocol:
 *   1. Client sends CIP request with service 0x4C, epath to Symbol Object
 *   2. Symbol segment contains the tag name as ANSI Extended Symbol
 *   3. Request data specifies number of elements to read
 *   4. Controller responds with tag data
 *
 * Tag Write Protocol:
 *   1. Client sends CIP request with service 0x4D
 *   2. Request data contains data type specifier + data to write
 *   3. Controller responds with success/failure
 *
 * Data Type Codes for Tag Read/Write:
 *   0xC1 = BOOL, 0xC2 = SINT, 0xC3 = INT, 0xC4 = DINT
 *   0xC5 = LINT, 0xCA = REAL, 0xCB = LREAL
 *   0xD0 = STRUCT (UDT), 0xA0+ = Array of base type
 *
 * Ref: Rockwell KB QA1932 ? CIP Data Table Read/Write
 */

/** CIP Data Type Codes for Tag Read/Write */
#define CIP_DATATYPE_BOOL     0xC1
#define CIP_DATATYPE_SINT     0xC2
#define CIP_DATATYPE_INT      0xC3
#define CIP_DATATYPE_DINT     0xC4
#define CIP_DATATYPE_LINT     0xC5
#define CIP_DATATYPE_USINT    0xC6
#define CIP_DATATYPE_UINT     0xC7
#define CIP_DATATYPE_UDINT    0xC8
#define CIP_DATATYPE_ULINT    0xC9
#define CIP_DATATYPE_REAL     0xCA
#define CIP_DATATYPE_LREAL    0xCB
#define CIP_DATATYPE_STIME    0xCC  /**< DINT[2] time-of-day                 */
#define CIP_DATATYPE_DATE     0xCD  /**< DINT[2] date                        */
#define CIP_DATATYPE_SHORT_STRING 0xDA
#define CIP_DATATYPE_STRING   0xDB
#define CIP_DATATYPE_STRUCT   0xA0  /**< Structured type (bit 7 set)         */
#define CIP_DATATYPE_ARRAY    0x60  /**< Array prefix (bits 6-7 set)         */

/** Tag read request */
typedef struct {
    char            tag_name[256];      /**< Fully-qualified tag name           */
    uint16_t        element_count;      /**< Number of elements to read         */
    uint8_t         data_type_code;     /**< CIP data type code                 */
} cip_tag_read_request_t;

/** Tag read response */
typedef struct {
    uint8_t         data_type_code;     /**< Echoed data type code              */
    uint8_t         reserved;           /**< Reserved (0)                        */
    uint16_t        data_size_bytes;    /**< Size of returned data               */
    uint8_t         data[CIP_MAX_MESSAGE_SIZE - 32]; /**< Tag data buffer        */
} cip_tag_read_response_t;

/** Tag write request */
typedef struct {
    char            tag_name[256];      /**< Fully-qualified tag name           */
    uint8_t         data_type_code;     /**< CIP data type code                 */
    uint16_t        element_count;      /**< Number of elements to write        */
    uint16_t        data_size_bytes;    /**< Size of data being written         */
    uint8_t         data[CIP_MAX_MESSAGE_SIZE - 32]; /**< Data payload          */
} cip_tag_write_request_t;

/* ============================================================================
 * L3: CIP Connection Manager
 * ============================================================================ */

#define CIP_MAX_CONNECTIONS    256

/** CIP connection manager ? tracks all active connections */
typedef struct {
    uint16_t            connection_count;
    cip_connection_t    connections[CIP_MAX_CONNECTIONS];
    uint32_t            next_transaction_id;
} cip_connection_manager_t;

/* ============================================================================
 * Core API: Identity & Device Discovery
 * ============================================================================ */

cip_identity_t* cip_identity_create(uint16_t vendor_id, uint16_t device_type,
                                     uint16_t product_code, const char* product_name);
void cip_identity_free(cip_identity_t* id);

/** Build a CIP Get_Attribute_All request for the Identity Object */
bool cip_build_identity_request(cip_request_t* request);

/** Parse a CIP Get_Attribute_All response for the Identity Object */
bool cip_parse_identity_response(const cip_response_t* response, cip_identity_t* id);

/* ============================================================================
 * Core API: Connection Management
 * ============================================================================ */

void cip_conn_mgr_init(cip_connection_manager_t* mgr);

/** Open a new CIP connection (Forward Open service) */
cip_status_code_t cip_conn_mgr_forward_open(cip_connection_manager_t* mgr,
                                              const cip_forward_open_params_t* params,
                                              cip_connection_t** conn_out);

/** Close a CIP connection (Forward Close service) */
cip_status_code_t cip_conn_mgr_forward_close(cip_connection_manager_t* mgr,
                                               uint32_t connection_id);

/** Find a connection by ID */
cip_connection_t* cip_conn_mgr_find(const cip_connection_manager_t* mgr,
                                     uint32_t connection_id);

/** Check for timed-out connections */
uint16_t cip_conn_mgr_cleanup_timeouts(cip_connection_manager_t* mgr,
                                        uint32_t current_time_ms);

/* ============================================================================
 * Core API: Tag Read/Write (Logix 5000 Symbol Object)**
 * ============================================================================ */

/**
 * @brief Build a CIP Tag Read request.
 * @param tag_name Fully-qualified tag name (e.g., "Program:MainProgram.MyTag")
 * @param element_count Number of elements to read (1 for scalars)
 * @param data_type_code CIP data type code
 * @param request Output request structure
 * @return true on success.
 *
 * This prepares the CIP explicit message for reading a Logix 5000 tag.
 * The tag name is encoded into the electronic path as ANSI extended symbol
 * segments, following the Logix 5000 Symbol Object specification.
 */
bool cip_build_tag_read_request(const char* tag_name, uint16_t element_count,
                                 uint8_t data_type_code, cip_request_t* request);

/**
 * @brief Parse a CIP Tag Read response.
 * @param response Response received from the controller
 * @param data_out Buffer to receive tag data
 * @param data_out_size Size of data_out buffer
 * @param actual_size Pointer to receive actual data size
 * @return CIP_STATUS_SUCCESS on success, or error code.
 */
cip_status_code_t cip_parse_tag_read_response(const cip_response_t* response,
                                                uint8_t* data_out,
                                                uint16_t data_out_size,
                                                uint16_t* actual_size);

/**
 * @brief Build a CIP Tag Write request.
 * @return true on success.
 */
bool cip_build_tag_write_request(const char* tag_name, uint8_t data_type_code,
                                  const uint8_t* data, uint16_t data_size,
                                  cip_request_t* request);

/* ============================================================================
 * Core API: EPATH Construction
 * ============================================================================ */

/** Build an EPATH from class/instance/attribute IDs */
void cip_epath_init(cip_epath_t* epath);

bool cip_epath_add_class(cip_epath_t* epath, uint32_t class_id);
bool cip_epath_add_instance(cip_epath_t* epath, uint32_t instance_id);
bool cip_epath_add_attribute(cip_epath_t* epath, uint32_t attribute_id);

/** Build an EPATH to Logix Symbol Object with tag name */
bool cip_epath_add_symbol(cip_epath_t* epath, const char* tag_name);

/** Serialize EPATH to byte format for CIP message */
uint16_t cip_epath_serialize(const cip_epath_t* epath,
                              uint8_t* buffer, uint16_t buffer_size);

/* ============================================================================
 * Core API: Request/Response Serialization
 * ============================================================================ */

/** Serialize a CIP request into a byte buffer for EtherNet/IP encapsulation */
uint16_t cip_request_serialize(const cip_request_t* request,
                                uint8_t* buffer, uint16_t buffer_size);

/** Parse a byte buffer into a CIP response */
bool cip_response_parse(const uint8_t* buffer, uint16_t buffer_size,
                         cip_response_t* response);

/** Get a human-readable error string for a CIP status code */
const char* cip_status_to_string(cip_status_code_t status);

#ifdef __cplusplus
}
#endif

#endif /* PLC_ROCKWELL_CIP_H */
