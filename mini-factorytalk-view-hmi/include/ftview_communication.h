/**
 * @file ftview_communication.h
 * @brief FactoryTalk View HMI -- Communication Layer (L2/L3/L4/L7)
 *
 * Knowledge points:
 *   L2.12 -- OPC-UA client-server connection lifecycle (connect, browse, read, write, disconnect)
 *   L2.13 -- RSLinx topic-based routing: shortcut -> PLC path
 *   L3.11 -- Connection pool with per-topic state machines
 *   L3.12 -- Pending request queue with timeout and retry
 *   L4.7  -- OPC UA Part 2: Security Model (None, Sign, SignAndEncrypt)
 *   L7.3  -- Rockwell RSLinx Classic / FactoryTalk Linx OPC DA/UA topology
 *   L7.4  -- CIP (Common Industrial Protocol) implicit/explicit messaging
 *
 * Reference: OPC UA Part 4: Services
 *            OPC UA Part 6: Mappings
 *            Rockwell Automation RSLinx Classic SDK
 *            ODVA CIP Specification Volume 1
 */

#ifndef FTVIEW_COMMUNICATION_H
#define FTVIEW_COMMUNICATION_H

#include "ftview_common.h"
#include "ftview_tag.h"
#include <stdint.h>
#include <stddef.h>

#define FTVIEW_COMM_MAX_TOPICS       64
#define FTVIEW_COMM_MAX_CONNECTIONS  16
#define FTVIEW_COMM_TOPIC_NAME_LEN   64
#define FTVIEW_COMM_PATH_LEN        256
#define FTVIEW_COMM_PENDING_MAX      64
#define FTVIEW_COMM_REQ_TIMEOUT_MS 5000
#define FTVIEW_COMM_RETRY_MAX         3

/* =====================================================================
 * L2.13 -- RSLinx Topic (Shortcut -> PLC routing)
 *
 * A topic maps a logical shortcut name to a physical PLC path.
 * Example: Shortcut "PLC1" -> "AB_ETH-1\192.168.1.10\Backplane\0"
 * ===================================================================== */

typedef struct {
    uint32_t                id;
    char                    name[FTVIEW_COMM_TOPIC_NAME_LEN];
    char                    path[FTVIEW_COMM_PATH_LEN];  /**< CIP path or OPC endpoint */
    bool                    connected;
    uint32_t                connection_id;               /**< pool index if connected */
    int64_t                 last_rx_ms;                  /**< last received data */
    uint32_t                message_count;               /**< total messages rx+tx */
    uint32_t                error_count;
    uint32_t                timeout_count;
} ftview_topic_t;

/* =====================================================================
 * L4.7 -- OPC UA Security Mode
 * ===================================================================== */

typedef enum {
    FTVIEW_COMM_SEC_NONE           = 0,  /**< No security */
    FTVIEW_COMM_SEC_SIGN           = 1,  /**< Message signing */
    FTVIEW_COMM_SEC_SIGN_ENCRYPT   = 2   /**< Sign + Encrypt */
} ftview_comm_security_mode_t;

/* =====================================================================
 * L3.11 -- Connection State Machine
 * ===================================================================== */

typedef enum {
    FTVIEW_CONN_STATE_IDLE        = 0,
    FTVIEW_CONN_STATE_CONNECTING  = 1,
    FTVIEW_CONN_STATE_CONNECTED   = 2,
    FTVIEW_CONN_STATE_DISCONNECTED= 3,
    FTVIEW_CONN_STATE_FAULT       = 4
} ftview_conn_state_t;

/** Connection instance */
typedef struct {
    uint32_t                id;
    uint32_t                topic_id;
    char                    endpoint_url[FTVIEW_COMM_PATH_LEN];
    ftview_conn_state_t     state;
    ftview_comm_security_mode_t security_mode;
    int64_t                 state_since_ms;
    uint32_t                retry_count;
    uint32_t                retry_max;
    uint32_t                retry_delay_ms;
} ftview_connection_t;

/* =====================================================================
 * L3.12 -- Pending Request
 * ===================================================================== */

typedef enum {
    FTVIEW_REQ_READ       = 0,
    FTVIEW_REQ_WRITE      = 1,
    FTVIEW_REQ_BROWSE     = 2,
    FTVIEW_REQ_SUBSCRIBE  = 3
} ftview_req_type_t;

typedef struct {
    uint32_t                id;
    ftview_req_type_t       type;
    uint32_t                topic_id;
    char                    address[FTVIEW_TAG_ADDR_MAX_LEN];
    ftview_value_t          write_value;     /**< for WRITE requests */
    int64_t                 submitted_ms;
    int64_t                 deadline_ms;
    uint32_t                retry_count;
    bool                    completed;
    ftview_err_t            result;
    ftview_value_t          resp_value;      /**< response data for READ */
} ftview_req_t;

/* =====================================================================
 * L2.12 -- OPC-UA Style Browse Result
 * ===================================================================== */

typedef struct {
    char                    item_name[FTVIEW_TAG_NAME_MAX_LEN];
    char                    item_id[FTVIEW_TAG_ADDR_MAX_LEN];
    ftview_type_t           type;
    bool                    writable;
} ftview_browse_item_t;

/* =====================================================================
 * Communication Manager
 * ===================================================================== */

typedef struct {
    ftview_topic_t          topics[FTVIEW_COMM_MAX_TOPICS];
    uint32_t                topic_count;
    ftview_connection_t     connections[FTVIEW_COMM_MAX_CONNECTIONS];
    uint32_t                connection_count;
    ftview_req_t            pending[FTVIEW_COMM_PENDING_MAX];
    uint32_t                pending_count;
} ftview_comm_mgr_t;

/* ───────── API ───────── */

void ftview_comm_mgr_init(ftview_comm_mgr_t *mgr);

/** L2.13 -- Define an RSLinx/FactoryTalk topic (logical shortcut -> PLC path).
 *  Path formats supported:
 *    Ethernet:   "AB_ETH-1\<IP>\Backplane\<slot>"
 *    Serial:     "AB_DF1-1\COM1\<node>"
 *    OPC UA:     "opc.tcp://<IP>:4840"
 *  Returns FTVIEW_ERR_DUPLICATE_TAG if topic name exists. */
ftview_err_t ftview_comm_add_topic(ftview_comm_mgr_t *mgr,
                                    const char *name,
                                    const char *path);

/** L3.11 -- Open a connection to a topic.
 *  Drives the connection state machine: IDLE -> CONNECTING -> CONNECTED.
 *  Actual I/O is simulated; this manages the logical state.
 *  Returns connection ID on success. */
uint32_t ftview_comm_connect(ftview_comm_mgr_t *mgr, uint32_t topic_id);

/** Close a connection */
ftview_err_t ftview_comm_disconnect(ftview_comm_mgr_t *mgr,
                                     uint32_t connection_id);

/** L2.12 -- Submit a read request for a tag address on a topic.
 *  Request is queued and completed asynchronously (simulated).
 *  Returns request ID for later status polling. */
uint32_t ftview_comm_read(ftview_comm_mgr_t *mgr,
                           uint32_t topic_id,
                           const char *address);

/** Submit a write request */
uint32_t ftview_comm_write(ftview_comm_mgr_t *mgr,
                            uint32_t topic_id,
                            const char *address,
                            const ftview_value_t *value);

/** Poll for completed requests (simulates async completion).
 *  Removes completed requests from pending queue.
 *  For each completed request, fills result and value.
 *  Returns number of requests completed this cycle. */
uint32_t ftview_comm_poll(ftview_comm_mgr_t *mgr,
                           int64_t now_ms);

/** Get the result of a completed request by ID */
ftview_err_t ftview_comm_get_result(const ftview_comm_mgr_t *mgr,
                                     uint32_t req_id,
                                     ftview_value_t *value_out);

/** L3.11 -- Connection retry logic: attempt to reconnect faulted connections.
 *  Returns number of connections transitioned to CONNECTING. */
uint32_t ftview_comm_retry_faulted(ftview_comm_mgr_t *mgr, int64_t now_ms);

/** L2.12 -- OPC-UA style browse: list available items on a connected topic.
 *  Populates items array and returns count. */
uint32_t ftview_comm_browse(ftview_comm_mgr_t *mgr,
                             uint32_t topic_id,
                             ftview_browse_item_t *items,
                             uint32_t max_items);

/** L7.3 -- Resolve a DRT {[shortcut]address} to the physical path.
 *  e.g. "{[PLC1]N7:0}" -> topic "PLC1" : address "N7:0" */
ftview_err_t ftview_comm_resolve_drt(ftview_comm_mgr_t *mgr,
                                      const char *drt,
                                      uint32_t *topic_id_out,
                                      char *address_out, size_t addr_len);

/** Check connection health: returns true if all topics are responsive */
bool ftview_comm_health_check(const ftview_comm_mgr_t *mgr, int64_t now_ms);

/** Get topic statistics (message count, errors) */
ftview_err_t ftview_comm_topic_stats(const ftview_comm_mgr_t *mgr,
                                      uint32_t topic_id,
                                      uint32_t *msg_count,
                                      uint32_t *err_count,
                                      uint32_t *timeout_count);

#endif /* FTVIEW_COMMUNICATION_H */
