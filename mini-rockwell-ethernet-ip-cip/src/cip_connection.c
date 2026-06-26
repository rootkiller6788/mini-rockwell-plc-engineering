/**
 * cip_connection.c
 */
#include <string.h>
#include "../include/cip_connection.h"

static uint32_t g_connection_serial_counter = 0;

static int state_transition_legal[7][7] = {
    { 0, 1, 0, 0, 0, 0, 0 },
    { 1, 0, 1, 0, 0, 0, 0 },
    { 1, 0, 0, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 1, 0, 1 },
    { 0, 0, 0, 1, 0, 1, 0 },
    { 1, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 0, 0, 0, 0 },
};

void cip_conn_init(cip_connection_t *conn)
{
    if (!conn) return;
    memset(conn, 0, sizeof(cip_connection_t));
    conn->connection_state  = CIP_CONN_STATE_NONEXISTENT;
    conn->rpi_us            = CIP_DEFAULT_RPI_US;
    conn->timeout_multiplier = CIP_DEFAULT_TIMEOUT_MULTIPLIER;
    conn->connection_type   = CIP_CONN_TYPE_EXCLUSIVE_OWNER;
    conn->transport_class   = CIP_TRANSPORT_CLASS_1;
    conn->production_trigger = CIP_TRIGGER_CYCLIC;
    conn->connection_priority = CIP_CONN_PRIORITY_HIGH;
    conn->connection_size_type = CIP_CONN_SIZE_FIXED;
    conn->is_active         = 0;
    conn->connection_timeout_us = cip_conn_calculate_timeout(conn);
}

int cip_conn_configure(cip_connection_t *conn,
                       uint16_t o_to_t_size, uint16_t t_to_o_size,
                       uint16_t ot_point, uint16_t to_point,
                       uint32_t rpi_us)
{
    if (!conn) return -1;
    uint32_t total_size = (uint32_t)o_to_t_size + (uint32_t)t_to_o_size;
    if (total_size > CIP_MAX_CONNECTION_SIZE) return -1;
    if (rpi_us < CIP_MIN_RPI_US) return -2;
    if (ot_point == 0 || to_point == 0) return -3;
    conn->o_to_t_size            = o_to_t_size;
    conn->t_to_o_size            = t_to_o_size;
    conn->o_to_t_connection_point = ot_point;
    conn->t_to_o_connection_point = to_point;
    conn->rpi_us                 = rpi_us;
    conn->connection_timeout_us  = cip_conn_calculate_timeout(conn);
    return 0;
}

int cip_conn_transition(cip_connection_t *conn, cip_connection_state_t new_state)
{
    if (!conn) return -1;
    uint8_t current = conn->connection_state;
    if (current > 6 || new_state > 6) return -2;
    if (!state_transition_legal[current][new_state]) return -3;
    conn->connection_state = (uint8_t)new_state;
    if (new_state == CIP_CONN_STATE_NONEXISTENT) conn->is_active = 0;
    if (new_state == CIP_CONN_STATE_ESTABLISHED) conn->is_active = 1;
    return 0;
}

uint32_t cip_conn_calculate_timeout(cip_connection_t *conn)
{
    if (!conn) return 0;
    uint8_t multiplier = conn->timeout_multiplier;
    if (multiplier == 0) multiplier = CIP_DEFAULT_TIMEOUT_MULTIPLIER;
    uint32_t timeout = conn->rpi_us * 4u * (uint32_t)multiplier;
    if (timeout > 3600000000u) timeout = 3600000000u;
    conn->connection_timeout_us = timeout;
    return timeout;
}

int cip_conn_set_transport(cip_connection_t *conn,
                           uint8_t transport_class, uint8_t trigger)
{
    if (!conn) return -1;
    if (transport_class > CIP_TRANSPORT_CLASS_3) return -2;
    if (trigger > CIP_TRIGGER_APPLICATION) return -3;
    switch (transport_class) {
    case CIP_TRANSPORT_CLASS_0:
        if (trigger != CIP_TRIGGER_CYCLIC) return -4;
        break;
    case CIP_TRANSPORT_CLASS_1:
        break;
    case CIP_TRANSPORT_CLASS_2:
    case CIP_TRANSPORT_CLASS_3:
        if (trigger != CIP_TRIGGER_APPLICATION) return -4;
        break;
    default:
        return -2;
    }
    conn->transport_class   = transport_class;
    conn->production_trigger = trigger;
    return 0;
}

int cip_conn_set_type(cip_connection_t *conn, uint8_t conn_type)
{
    if (!conn) return -1;
    if (conn_type > CIP_CONN_TYPE_REDUNDANT_OWNER) return -2;
    conn->connection_type = conn_type;
    return 0;
}

int cip_conn_validate(const cip_connection_t *conn)
{
    if (!conn) return -1;
    uint32_t total = (uint32_t)conn->o_to_t_size + (uint32_t)conn->t_to_o_size;
    if (total > CIP_MAX_CONNECTION_SIZE) return -1;
    if (conn->o_to_t_connection_point == 0 && conn->t_to_o_connection_point == 0) {
        if (conn->o_to_t_size > 0 || conn->t_to_o_size > 0) return -2;
    }
    if (conn->rpi_us < CIP_MIN_RPI_US) return -3;
    if (conn->connection_state > CIP_CONN_STATE_CLOSING) return -4;
    if (conn->transport_class > CIP_TRANSPORT_CLASS_3) return -5;
    if (conn->timeout_multiplier == 0 && conn->connection_state == CIP_CONN_STATE_ESTABLISHED)
        return -6;
    return 0;
}

const char *cip_conn_state_string(cip_connection_state_t state)
{
    switch (state) {
    case CIP_CONN_STATE_NONEXISTENT:     return "NONEXISTENT";
    case CIP_CONN_STATE_CONFIGURING:     return "CONFIGURING";
    case CIP_CONN_STATE_WAITING_FOR_ID:  return "WAITING_FOR_ID";
    case CIP_CONN_STATE_ESTABLISHED:     return "ESTABLISHED";
    case CIP_CONN_STATE_TIMING_OUT:      return "TIMING_OUT";
    case CIP_CONN_STATE_DEFERRED_DELETE: return "DEFERRED_DELETE";
    case CIP_CONN_STATE_CLOSING:         return "CLOSING";
    default:                             return "UNKNOWN_STATE";
    }
}

void cip_conn_close(cip_connection_t *conn)
{
    if (!conn) return;
    cip_conn_init(conn);
    conn->connection_state = CIP_CONN_STATE_NONEXISTENT;
}

uint16_t cip_conn_next_serial(void)
{
    g_connection_serial_counter++;
    if (g_connection_serial_counter > 0xFFFFu) g_connection_serial_counter = 1;
    return g_connection_serial_counter;
}

uint32_t cip_conn_calculate_api(uint32_t rpi_us, uint32_t tick_us)
{
    if (tick_us == 0) return rpi_us;
    uint32_t quotient = rpi_us / tick_us;
    uint32_t remainder = rpi_us % tick_us;
    uint32_t ticks = quotient + (remainder > 0 ? 1u : 0u);
    return ticks * tick_us;
}

uint32_t cip_conn_ticks_per_cycle(uint32_t rpi_us, uint32_t tick_us)
{
    if (tick_us == 0) return 0;
    if (rpi_us == 0) return 0;
    uint32_t quotient = rpi_us / tick_us;
    uint32_t remainder = rpi_us % tick_us;
    return quotient + (remainder > 0 ? 1u : 0u);
}
