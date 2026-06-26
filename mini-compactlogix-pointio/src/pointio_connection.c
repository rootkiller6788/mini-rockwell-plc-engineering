/**
 * @file pointio_connection.c
 * @brief CIP I/O connection management for Point I/O over EtherNet/IP.
 *
 * Level: L3-L5 (Connection FSM, Forward Open/Close, RPI management)
 *
 * Reference:
 *   - ODVA "CIP Specification" Vol.1 (Common), Chapter 3 "Connection Management"
 *   - ODVA "CIP Specification" Vol.2 (EtherNet/IP Adaptation)
 *   - Rockwell "EtherNet/IP Network Devices User Manual" (ENET-UM001)
 *
 * Implements the CIP connection lifecycle:
 *   1. Forward Open  (Service 0x54) - establishes I/O connection
 *   2. Data Exchange  (Implicit messaging) - cyclic I/O data transfer
 *   3. Forward Close (Service 0x4E) - terminates connection
 *
 * Connection parameters:
 *   - O->T RPI: Rate at which originator sends outputs to target
 *   - T->O RPI: Rate at which target sends inputs to originator
 *   - Timeout: RPI * Multiplier (typically 4x, default 32x for multicast)
 *   - Transport class: 0 (cyclic), 1 (COS), 2 (app-triggered)
 */

#include "pointio_connection.h"
#include "pointio_types.h"
#include "pointio_module.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/*===========================================================================
 * L3: Connection Pool Management
 *===========================================================================*/

/**
 * Initialize the connection pool.
 *
 * Sets all connection slots to IDLE state and zeroes all parameters.
 * The pool uses a simple linear search for free slots (acceptable
 * for the typical < 100 connections in a CompactLogix application).
 */
void pointio_conn_pool_init(pointio_connection_pool_t *pool)
{
    if (!pool) return;

    memset(pool, 0, sizeof(pointio_connection_pool_t));

    /* Initialize all connection IDs to -1 (invalid) */
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        pool->connections[i].connection_id = -1;
        pool->connections[i].state = POINTIO_CONN_STATE_IDLE;
    }

    pool->next_connection_id = 1;
    pool->active_count = 0;
}

/*===========================================================================
 * L2: Forward Open - Connection Establishment
 *
 * Simulates the CIP Forward Open service (Service Code 0x54).
 *
 * Steps:
 *   1. Validate parameters (RPI, module type, connection type)
 *   2. Find free connection slot
 *   3. Configure O->T parameters (output data path)
 *   4. Configure T->O parameters (input data path)
 *   5. Assign connection IDs (O->T and T->O)
 *   6. Transition to RUNNING state
 *
 * In real CIP, Forward Open is a UCMM (Unconnected Message Manager)
 * request sent to the target device. The response includes the
 * O->T and T->O connection IDs assigned by both endpoints.
 *
 * Reference: ODVA CIP Vol.1, Section 3-5.4.2 "Forward Open Request"
 *===========================================================================*/

int pointio_conn_open(pointio_connection_pool_t *pool,
    pointio_connection_type_t type, const pointio_module_t *module,
    uint32_t rpi_us, pointio_trigger_t trigger, int *conn_id)
{
    if (!pool || !module || !conn_id) return -1;
    if (type == POINTIO_CONN_NONE) return -1;
    if (rpi_us < POINTIO_MIN_RPI_MS * 1000 || rpi_us > POINTIO_MAX_RPI_MS * 1000)
        return -1;

    /* Validate connection type against module */
    if (type == POINTIO_CONN_EXCLUSIVE_OWNER && module->type == POINTIO_TYPE_UNKNOWN)
        return -1;
    if (type == POINTIO_CONN_RACK_OPTIMIZED &&
        module->type != POINTIO_TYPE_ADAPTER)
        return -1;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].state == POINTIO_CONN_STATE_IDLE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;  /* Pool full */

    /* Populate connection parameters */
    pointio_connection_t *conn = &pool->connections[slot];
    memset(conn, 0, sizeof(pointio_connection_t));

    conn->connection_id = pool->next_connection_id++;
    conn->type = type;
    conn->state = POINTIO_CONN_STATE_CONNECTING;
    conn->trigger = trigger;
    conn->target_slot = module->slot.slot;

    /* O->T parameters (originator to target = output data) */
    conn->ot.o_t_rpi_us = rpi_us;
    conn->ot.o_t_size_bytes = module->output_size_bytes;
    conn->ot.o_t_priority = 1;  /* High priority for I/O data */
    conn->ot.o_t_connection_id = (uint16_t)(0x4000 + conn->connection_id);
    conn->ot.o_t_api = 0;  /* Default assembly instance */

    /* T->O parameters (target to originator = input data) */
    conn->to.t_o_rpi_us = rpi_us;
    conn->to.t_o_size_bytes = module->input_size_bytes;
    conn->to.t_o_priority = 1;
    conn->to.t_o_connection_id = (uint16_t)(0x8000 + conn->connection_id);
    conn->to.t_o_api = 0;
    conn->to.t_o_is_multicast = (type == POINTIO_CONN_LISTEN_ONLY) ? 1 : 0;
    conn->to.t_o_multicast_ttl = 1;

    /* Timeout configuration */
    conn->timeout_us = rpi_us * POINTIO_TIMEOUT_MULTIPLIER;
    conn->watchdog_counter = POINTIO_TIMEOUT_MULTIPLIER;
    conn->last_packet_time_us = 0;

    /* Transition to RUNNING (simulated - real CIP involves network exchange) */
    conn->state = POINTIO_CONN_STATE_RUNNING;
    conn->stats.connection_opens = 1;

    pool->active_count++;
    *conn_id = conn->connection_id;

    return 0;
}

/*===========================================================================
 * L2: Forward Close - Connection Termination
 *
 * Simulates the CIP Forward Close service (Service Code 0x4E).
 *
 * Steps:
 *   1. Find connection by ID
 *   2. Transition to SHUTDOWN
 *   3. Clean up resources (slot freed)
 *   4. Transition to IDLE
 *
 * Reference: ODVA CIP Vol.1, Section 3-5.5.2 "Forward Close"
 *===========================================================================*/

int pointio_conn_close(pointio_connection_pool_t *pool, int conn_id)
{
    if (!pool) return -1;

    /* Find the connection */
    pointio_connection_t *conn = NULL;
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            conn = &pool->connections[i];
            break;
        }
    }
    if (!conn) return -1;

    if (conn->state != POINTIO_CONN_STATE_RUNNING &&
        conn->state != POINTIO_CONN_STATE_TIMED_OUT &&
        conn->state != POINTIO_CONN_STATE_FAULTED) {
        return -1;  /* Not in a closable state */
    }

    conn->state = POINTIO_CONN_STATE_SHUTDOWN;

    /* Reset the connection slot */
    conn->connection_id = -1;
    conn->state = POINTIO_CONN_STATE_IDLE;
    pool->active_count--;

    return 0;
}

/*===========================================================================
 * L5: Connection Reset (Recovery)
 *
 * When a connection times out, attempt to re-establish it with the
 * same parameters. This simulates the auto-recovery behavior of
 * CompactLogix controllers.
 *===========================================================================*/

int pointio_conn_reset(pointio_connection_pool_t *pool, int conn_id)
{
    if (!pool) return -1;

    pointio_connection_t *conn = NULL;
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            conn = &pool->connections[i];
            break;
        }
    }
    if (!conn) return -1;

    /* Save parameters */
    pointio_connection_type_t type = conn->type;
    uint32_t rpi_us = conn->ot.o_t_rpi_us;
    pointio_trigger_t trigger = conn->trigger;
    uint8_t target_slot = conn->target_slot;

    /* Close old connection */
    conn->state = POINTIO_CONN_STATE_IDLE;
    conn->connection_id = -1;

    /* Re-open with saved parameters */
    /* We need a module to reopen, but we only have the slot */
    /* For reset, we just re-activate the slot with new IDs */
    conn->connection_id = pool->next_connection_id++;
    conn->type = type;
    conn->trigger = trigger;
    conn->target_slot = target_slot;
    conn->ot.o_t_rpi_us = rpi_us;
    conn->to.t_o_rpi_us = rpi_us;
    conn->ot.o_t_connection_id = (uint16_t)(0x4000 + conn->connection_id);
    conn->to.t_o_connection_id = (uint16_t)(0x8000 + conn->connection_id);
    conn->timeout_us = rpi_us * POINTIO_TIMEOUT_MULTIPLIER;
    conn->watchdog_counter = POINTIO_TIMEOUT_MULTIPLIER;
    conn->state = POINTIO_CONN_STATE_RUNNING;
    conn->stats.connection_opens++;

    return 0;
}

/*===========================================================================
 * L5: Data Exchange (Implicit Messaging)
 *
 * Implicit messages are the cyclic I/O data transfers that occur at
 * the configured RPI. These are unacknowledged UDP messages in
 * EtherNet/IP (Class 1 connections).
 *
 * T->O: Target (I/O module) sends input data to Originator (PLC)
 * O->T: Originator (PLC) sends output data to Target (I/O module)
 *===========================================================================*/

/**
 * Simulate receiving T->O input data from a module.
 *
 * In real hardware, this arrives as a UDP packet containing the
 * input assembly. This function simulates that arrival.
 */
int pointio_conn_receive_input(pointio_connection_pool_t *pool,
    int conn_id, const uint8_t *data, uint16_t len)
{
    if (!pool || !data) return -1;

    pointio_connection_t *conn = NULL;
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            conn = &pool->connections[i];
            break;
        }
    }
    if (!conn) return -1;
    if (conn->state != POINTIO_CONN_STATE_RUNNING) return -1;
    if (len > conn->to.t_o_size_bytes) return -1;

    /* Update connection health */
    conn->watchdog_counter = POINTIO_TIMEOUT_MULTIPLIER;
    /* We use a mock timestamp - in real code this comes from the system clock */
    conn->last_packet_time_us = (uint64_t)(conn->stats.total_packets_received *
        conn->ot.o_t_rpi_us);

    /* Update statistics */
    conn->stats.total_packets_received++;
    if (conn->stats.total_packets_received > 0) {
        conn->stats.packet_loss_rate =
            (double)conn->stats.packets_lost /
            (double)(conn->stats.total_packets_sent + conn->stats.packets_lost + 1);
    }

    (void)len;
    (void)data;
    return 0;
}

/**
 * Simulate sending O->T output data to a module.
 */
int pointio_conn_send_output(pointio_connection_pool_t *pool,
    int conn_id, const uint8_t *data, uint16_t len)
{
    if (!pool || !data) return -1;

    pointio_connection_t *conn = NULL;
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            conn = &pool->connections[i];
            break;
        }
    }
    if (!conn) return -1;
    if (conn->state != POINTIO_CONN_STATE_RUNNING) return -1;
    if (len > conn->ot.o_t_size_bytes) return -1;

    conn->stats.total_packets_sent++;

    (void)data;
    (void)len;
    return 0;
}

/*===========================================================================
 * L4: RPI Validation & Timeout Management
 *
 * RPI (Requested Packet Interval) is the fundamental timing parameter
 * for CIP I/O connections. It determines how often I/O data is exchanged.
 *
 * The CompactLogix controller enforces:
 *   - RPI must be an integer ms from 1 to 750
 *   - Total I/O bandwidth = sum(connection_data_size / RPI) cannot
 *     exceed the controller's backplane capacity
 *   - Practical minimum RPI depends on number of connections
 *
 * Timeout = RPI * TimeoutMultiplier (default 4, max 512)
 * The connection watchdog decrements each RPI period. If it reaches 0,
 * the connection is declared timed out.
 *===========================================================================*/

/**
 * Validate RPI against limits and system capacity.
 */
int pointio_conn_validate_rpi(double rpi_ms, int num_connections)
{
    if (rpi_ms < (double)POINTIO_MIN_RPI_MS ||
        rpi_ms > (double)POINTIO_MAX_RPI_MS) {
        return 0;  /* Out of range */
    }

    /* Capacity check: CompactLogix can handle ~500 packets/sec per CPU class */
    double packets_per_sec = (double)num_connections * (1000.0 / rpi_ms);
    double max_packets_per_sec = 500.0;  /* Conservative limit */

    if (packets_per_sec > max_packets_per_sec) {
        return 0;  /* Would overload controller */
    }

    return 1;  /* RPI valid */
}

/**
 * Check all connections for timeouts.
 *
 * Scans the connection pool and decrements watchdog counters.
 * Connections whose watchdog reaches 0 are marked TIMED_OUT.
 *
 * Returns a bitmask of timed-out connection indices.
 */
uint64_t pointio_conn_check_timeouts(pointio_connection_pool_t *pool,
    uint64_t now_us)
{
    if (!pool) return 0;

    uint64_t timed_out_mask = 0;

    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        pointio_connection_t *conn = &pool->connections[i];
        if (conn->state != POINTIO_CONN_STATE_RUNNING) continue;

        /* Check if watchdog has expired */
        if (conn->watchdog_counter == 0) {
            conn->state = POINTIO_CONN_STATE_TIMED_OUT;
            conn->stats.connection_timeouts++;
            timed_out_mask |= ((uint64_t)1 << i);
            continue;
        }

        /* Decrement watchdog */
        uint64_t elapsed = now_us - conn->last_packet_time_us;
        uint64_t periods = elapsed / conn->ot.o_t_rpi_us;
        if (periods > 0) {
            if (periods >= (uint64_t)conn->watchdog_counter) {
                conn->watchdog_counter = 0;
            } else {
                conn->watchdog_counter -= (uint32_t)periods;
            }
            conn->last_packet_time_us = now_us;
        }
    }

    return timed_out_mask;
}

/*===========================================================================
 * L5: Multicast Group Management
 *
 * In EtherNet/IP, Listen Only connections receive T->O data via IP
 * multicast. The target module sends one multicast packet, and
 * multiple listeners can receive it simultaneously. This conserves
 * network bandwidth when multiple controllers need the same input data.
 *
 * Multicast address: 239.192.x.y (administratively scoped)
 * The T->O connection ID encodes the multicast group.
 *===========================================================================*/

int pointio_conn_is_multicast(const pointio_connection_pool_t *pool,
    int conn_id)
{
    if (!pool) return -1;

    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            return pool->connections[i].to.t_o_is_multicast;
        }
    }
    return -1;
}

int pointio_conn_set_multicast(pointio_connection_pool_t *pool,
    int conn_id, int enable)
{
    if (!pool) return -1;

    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            pool->connections[i].to.t_o_is_multicast = (uint8_t)(enable ? 1 : 0);
            return 0;
        }
    }
    return -1;
}

/*===========================================================================
 * L8: Connection Health Check (Advanced)
 *
 * Detects "flapping" connections - connections that repeatedly
 * time out and recover, indicating network instability or
 * marginal signal quality.
 *
 * Flapping detection criteria:
 *   - Packet loss rate > 5%
 *   - OR > 3 timeouts within the last minute
 *
 * Health percentage is computed as:
 *   health = 100 * (1 - packet_loss_rate) * (1 - timeout_penalty)
 *   where timeout_penalty = min(1.0, timeouts_per_minute / 10)
 *===========================================================================*/

int pointio_conn_health_check(const pointio_connection_pool_t *pool,
    int conn_id, int *is_flapping, double *health_pct)
{
    if (!pool || !is_flapping || !health_pct) return -1;

    const pointio_connection_t *conn = NULL;
    for (int i = 0; i < POINTIO_MAX_CONNECTIONS; i++) {
        if (pool->connections[i].connection_id == conn_id) {
            conn = &pool->connections[i];
            break;
        }
    }
    if (!conn) return -1;

    /* Compute packet loss rate */
    double loss_rate = 0.0;
    uint32_t total = conn->stats.total_packets_sent +
                     conn->stats.packets_lost;
    if (total > 0) {
        loss_rate = (double)conn->stats.packets_lost / (double)total;
    }

    /* Flapping detection */
    *is_flapping = 0;
    if (loss_rate > 0.05) {
        *is_flapping = 1;
    }
    if (conn->stats.connection_timeouts > 3) {
        *is_flapping = 1;
    }

    /* Health percentage */
    *health_pct = 100.0 * (1.0 - loss_rate);
    if (conn->stats.connection_timeouts > 0) {
        double timeout_penalty = (double)conn->stats.connection_timeouts / 10.0;
        if (timeout_penalty > 1.0) timeout_penalty = 1.0;
        *health_pct *= (1.0 - timeout_penalty * 0.5);
    }
    if (*health_pct < 0.0) *health_pct = 0.0;
    if (*health_pct > 100.0) *health_pct = 100.0;

    return 0;
}
