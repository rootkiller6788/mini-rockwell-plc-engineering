/**
 * @file pointio_connection.h
 * @brief CIP I/O connection management for Point I/O over EtherNet/IP.
 *
 * Level: L3 Engineering Structures + L4 Engineering Laws
 * Reference:
 *   - ODVA "CIP Specification" Vol.1 (Common), Vol.2 (EtherNet/IP Adaptation)
 *   - ODVA "CIP Network Library" Vol.7 (Integration)
 *   - Rockwell "EtherNet/IP Network Devices User Manual" (ENET-UM001)
 *
 * Implements the CIP connection lifecycle: Forward Open, data exchange via
 * implicit (I/O) messaging, and Forward Close. Handles RPI-based cyclic
 * connections, Change-of-State triggers, and multicast T->O data.
 */

#ifndef POINTIO_CONNECTION_H
#define POINTIO_CONNECTION_H

#include "pointio_types.h"
#include "pointio_module.h"

/*===========================================================================
 * L1: CIP Connection Parameters
 *===========================================================================*/

/**
 * @brief CIP connection originator-to-target (O->T) parameters.
 */
typedef struct {
    uint32_t    o_t_rpi_us;         /* O->T RPI in microseconds */
    uint16_t    o_t_size_bytes;     /* O->T data size */
    uint8_t     o_t_priority;       /* 0=low, 1=high, 2=scheduled */
    uint16_t    o_t_connection_id;  /* Assigned O->T connection ID */
    uint32_t    o_t_api;            /* Application Path Instance */
} pointio_conn_ot_params_t;

/**
 * @brief CIP connection target-to-originator (T->O) parameters.
 */
typedef struct {
    uint32_t    t_o_rpi_us;         /* T->O RPI in microseconds */
    uint16_t    t_o_size_bytes;     /* T->O data size */
    uint8_t     t_o_priority;       /* 0=low, 1=high, 2=scheduled */
    uint16_t    t_o_connection_id;  /* Assigned T->O connection ID */
    uint32_t    t_o_api;            /* Application Path Instance */
    uint8_t     t_o_is_multicast;   /* 1 = multicast connection */
    uint8_t     t_o_multicast_ttl;  /* Time-to-live for multicast */
} pointio_conn_to_params_t;

/**
 * @brief Full CIP I/O connection descriptor.
 */
typedef struct {
    int                         connection_id;      /* Local handle */
    pointio_connection_type_t   type;
    pointio_connection_state_t  state;
    pointio_conn_ot_params_t    ot;                 /* O->T (output data) */
    pointio_conn_to_params_t    to;                 /* T->O (input data) */
    pointio_trigger_t           trigger;
    uint32_t                    timeout_us;         /* RPI * multiplier */
    uint64_t                    last_packet_time_us; /* Time of last valid packet */
    uint32_t                    watchdog_counter;    /* Counts down each RPI */
    uint8_t                     target_slot;        /* Slot of target module */
    uint8_t                     target_mac[6];      /* Target MAC address */
    pointio_conn_stats_t        stats;
} pointio_connection_t;

/*===========================================================================
 * L3: Connection Pool & Forward Open/Close
 *===========================================================================*/

#define POINTIO_MAX_CONNECTIONS 256

/**
 * @brief Connection pool (manages all active CIP connections).
 */
typedef struct {
    pointio_connection_t    connections[POINTIO_MAX_CONNECTIONS];
    int                     active_count;
    int                     next_connection_id;
} pointio_connection_pool_t;

/*===========================================================================
 * L2: Connection Pool Lifecycle
 *===========================================================================*/

/**
 * @brief Initialize the connection pool to empty state.
 *
 * All connection slots are set to IDLE with zeroed parameters.
 *
 * @param pool  Pointer to connection pool
 */
void pointio_conn_pool_init(pointio_connection_pool_t *pool);

/**
 * @brief Open a CIP I/O connection (Forward Open).
 *
 * Simulates the CIP Forward Open service (Service Code 0x54).
 * Allocates a connection ID, sets up O->T and T->O parameters,
 * and transitions state to RUNNING.
 *
 * Reference: ODVA CIP Vol.1, Section 3-5.4 "Connection Establishment"
 *
 * @param pool       Connection pool
 * @param type       Connection type (Exclusive Owner, Input Only, etc.)
 * @param module     Target module
 * @param rpi_us     Requested Packet Interval (microseconds)
 * @param trigger    Transport class trigger (Cyclic, COS)
 * @param conn_id    Output: assigned connection ID
 * @return 0 on success, -1 if pool full or invalid parameters
 */
int pointio_conn_open(pointio_connection_pool_t *pool,
    pointio_connection_type_t type, const pointio_module_t *module,
    uint32_t rpi_us, pointio_trigger_t trigger, int *conn_id);

/**
 * @brief Close a CIP I/O connection (Forward Close).
 *
 * Simulates the CIP Forward Close service (Service Code 0x4E).
 * Transitions connection to IDLE and frees the slot.
 *
 * The module's outputs are set to fault state before closing
 * (unless the connection is Input Only).
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID to close
 * @return 0 on success, -1 if connection not found
 */
int pointio_conn_close(pointio_connection_pool_t *pool, int conn_id);

/**
 * @brief Reset (re-establish) a connection after timeout.
 *
 * Performs a Forward Close followed by Forward Open with the same parameters.
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID to reset
 * @return 0 on success, -1 on failure
 */
int pointio_conn_reset(pointio_connection_pool_t *pool, int conn_id);

/*===========================================================================
 * L5: Connection Data Exchange
 *===========================================================================*/

/**
 * @brief Simulate receiving T->O data (input assembly from module).
 *
 * In real hardware, this data arrives via EtherNet/IP implicit messaging.
 * This function simulates the arrival of a new input assembly packet.
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID
 * @param data       Input data buffer
 * @param len        Data length in bytes
 * @return 0 on success, -1 on invalid conn_id or data too large
 */
int pointio_conn_receive_input(pointio_connection_pool_t *pool,
    int conn_id, const uint8_t *data, uint16_t len);

/**
 * @brief Simulate sending O->T data (output assembly to module).
 *
 * Prepares and "transmits" the output assembly data.
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID
 * @param data       Output data buffer
 * @param len        Data length in bytes
 * @return 0 on success, -1 on error
 */
int pointio_conn_send_output(pointio_connection_pool_t *pool,
    int conn_id, const uint8_t *data, uint16_t len);

/*===========================================================================
 * L4: RPI & Timeout Management
 *===========================================================================*/

/**
 * @brief Check and handle connection timeouts.
 *
 * Scans all active connections, decrements watchdog counters, and detects
 * timeouts. A timeout occurs when no valid packet is received within
 * (RPI * POINTIO_TIMEOUT_MULTIPLIER) microseconds.
 *
 * Called periodically (typically every RPI cycle).
 *
 * @param pool       Connection pool
 * @param now_us     Current timestamp in microseconds
 * @return Bitmask of timed-out connection IDs (bit N set = conn N timed out)
 */
uint64_t pointio_conn_check_timeouts(pointio_connection_pool_t *pool,
    uint64_t now_us);

/**
 * @brief Validate RPI value against limits.
 *
 * Checks that RPI is within [POINTIO_MIN_RPI_MS, POINTIO_MAX_RPI_MS]
 * and that it's achievable with the number of connections in the pool.
 *
 * @param rpi_ms          Requested Packet Interval (ms)
 * @param num_connections Number of active connections in pool
 * @return 1 if valid, 0 if invalid
 */
int pointio_conn_validate_rpi(double rpi_ms, int num_connections);

/*===========================================================================
 * L5: Multicast Group Management
 *===========================================================================*/

/**
 * @brief Check if a connection uses multicast for T->O data.
 *
 * In Listen Only connections, the T->O data is multicast from the
 * target module. Multiple Listen Only connections can receive the
 * same multicast stream, saving network bandwidth.
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID
 * @return 1 if multicast, 0 if unicast, -1 if invalid conn_id
 */
int pointio_conn_is_multicast(const pointio_connection_pool_t *pool,
    int conn_id);

/**
 * @brief Set multicast delivery for a connection.
 *
 * Enable/disable multicast delivery of T->O data.
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID
 * @param enable     1 to enable multicast, 0 for unicast
 * @return 0 on success
 */
int pointio_conn_set_multicast(pointio_connection_pool_t *pool,
    int conn_id, int enable);

/*===========================================================================
 * L8: Connection Performance Analysis
 *===========================================================================*/

/**
 * @brief Analyze connection performance and detect degradation.
 *
 * Computes packet loss rate, average RTT, and checks if the connection
 * is experiencing intermittent drops (flapping).
 *
 * A flapping connection: packet_loss_rate > 5% OR
 * connection_timeouts > 3 per minute.
 *
 * @param pool       Connection pool
 * @param conn_id    Connection ID
 * @param is_flapping Output: 1 if connection is flapping
 * @param health_pct Output: health percentage (100 = perfect)
 * @return 0 on success
 */
int pointio_conn_health_check(const pointio_connection_pool_t *pool,
    int conn_id, int *is_flapping, double *health_pct);

#endif /* POINTIO_CONNECTION_H */
