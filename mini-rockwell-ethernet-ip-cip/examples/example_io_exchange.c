#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/cip_connection.h"
#include "../include/cip_message.h"
#include "../include/cip_object.h"

int main(void) {
    printf("=== Example: CIP I/O Connection Lifecycle ===

");

    /* Step 1: Initialize connection */
    cip_connection_t conn;
    cip_conn_init(&conn);
    printf("1. Connection initialized: state=%s, RPI=%u us
",
           cip_conn_state_string(conn.connection_state), conn.rpi_us);

    /* Step 2: Configure I/O parameters */
    int rc = cip_conn_configure(&conn, 64, 32,
                                CIP_CONN_POINT_OUTPUT_ASSEMBLY,
                                CIP_CONN_POINT_INPUT_ASSEMBLY,
                                10000);
    printf("2. Configured: O->T=%u bytes, T->O=%u bytes, RPI=%u us (rc=%d)
",
           conn.o_to_t_size, conn.t_to_o_size, conn.rpi_us, rc);

    /* Step 3: Set transport */
    cip_conn_set_transport(&conn, CIP_TRANSPORT_CLASS_1, CIP_TRIGGER_CYCLIC);
    printf("3. Transport: Class %d, Trigger=%d
",
           conn.transport_class, conn.production_trigger);

    /* Step 4: State machine - Forward Open */
    cip_conn_transition(&conn, CIP_CONN_STATE_CONFIGURING);
    printf("4. FwdOpen sent: state=%s
",
           cip_conn_state_string(conn.connection_state));

    cip_conn_transition(&conn, CIP_CONN_STATE_WAITING_FOR_ID);
    cip_conn_transition(&conn, CIP_CONN_STATE_ESTABLISHED);
    printf("5. Established: state=%s, active=%d
",
           cip_conn_state_string(conn.connection_state), conn.is_active);

    /* Step 5: Compute timeout */
    uint32_t timeout = cip_conn_calculate_timeout(&conn);
    printf("6. Timeout: %u us (%u ms)
",
           timeout, timeout / 1000);

    /* Step 6: Validate connection */
    printf("7. Validation: %s
",
           cip_conn_validate(&conn) == 0 ? "PASS" : "FAIL");

    /* Step 7: Forward Close */
    cip_conn_transition(&conn, CIP_CONN_STATE_CLOSING);
    printf("8. FwdClose sent: state=%s
",
           cip_conn_state_string(conn.connection_state));
    cip_conn_close(&conn);
    printf("9. Closed: state=%s
",
           cip_conn_state_string(conn.connection_state));

    printf("
Connection lifecycle complete.
");
    return 0;
}
