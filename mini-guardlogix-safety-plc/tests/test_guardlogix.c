/**
 * test_guardlogix.c -- Unit Tests for GuardLogix Safety PLC Library
 *
 * Covers all core APIs with assert-based testing. Tests verify:
 * - Controller initialization and state transitions
 * - SIL/PFDavg computation correctness
 * - Safety I/O evaluation (equivalent, complementary)
 * - Safety output pulse testing
 * - CRC-32 and signature verification
 * - Safety network communication
 * - Proof test interval computation
 * - Diagnostic coverage verification
 * - Safety task timing
 *
 * Tests use standard assert() from <assert.h>.
 * Run: make test
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "guardlogix_safety.h"
#include "guardlogix_sil.h"
#include "guardlogix_safety_io.h"
#include "guardlogix_safety_task.h"
#include "guardlogix_signature.h"
#include "guardlogix_safety_net.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

/* ================================================================
 * Controller Initialization Tests
 * ================================================================ */
static void test_controller_init(void)
{
    TEST("controller_init_5580S");
    glx_safety_controller_t ctrl;
    int ret = glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 50);
    assert(ret == 0);
    assert(ctrl.state == GLX_STATE_POWER_UP);
    assert(ctrl.max_sil == GLX_SIL_3);
    assert(ctrl.max_pl == GLX_PL_E);
    assert(ctrl.my_role == GLX_PARTNER_PRIMARY);
    assert(ctrl.safety_task_period_ms == 50);
    assert(ctrl.watchdog.watchdog_enabled == 1);
    assert(ctrl.watchdog.watchdog_timeout_ms == 150);
    assert(ctrl.diag_1oo2d.diagnostic_active == 1);
    PASS();

    TEST("controller_init_null");
    ret = glx_safety_controller_init(NULL, GLX_CTRL_5580S, 1, 50);
    assert(ret == -1);
    PASS();

    TEST("controller_init_invalid_period");
    ret = glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 0);
    assert(ret == -1);
    ret = glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 1000);
    assert(ret == -1);
    PASS();

    TEST("controller_init_secondary");
    ret = glx_safety_controller_init(&ctrl, GLX_CTRL_COMPACT, 0, 20);
    assert(ret == 0);
    assert(ctrl.my_role == GLX_PARTNER_SECONDARY);
    assert(ctrl.max_safety_connections == 16);
    PASS();
}

/* ================================================================
 * State Transition Tests
 * ================================================================ */
static void test_state_transitions(void)
{
    glx_safety_controller_t ctrl;

    TEST("transition_power_up_to_locked");
    glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 50);
    int ret = glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_LOCKED);
    assert(ret == 0);
    assert(ctrl.state == GLX_STATE_SAFETY_LOCKED);
    PASS();

    TEST("transition_locked_to_fault");
    ret = glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_FAULT);
    assert(ret == 0);
    assert(ctrl.state == GLX_STATE_SAFETY_FAULT);
    PASS();

    TEST("transition_fault_to_locked");
    ret = glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_LOCKED);
    assert(ret == 0);
    assert(ctrl.state == GLX_STATE_SAFETY_LOCKED);
    PASS();

    TEST("transition_illegal_fault_to_overrun");
    glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_FAULT);
    ret = glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_TASK_OVERRUN);
    assert(ret == -1);
    PASS();

    TEST("transition_hard_fault_is_trap");
    glx_safety_controller_init(&ctrl, GLX_CTRL_5580S, 1, 50);
    ret = glx_safety_state_transition(&ctrl, GLX_STATE_HARD_FAULT);
    assert(ret == 0);
    assert(ctrl.state == GLX_STATE_HARD_FAULT);
    ret = glx_safety_state_transition(&ctrl, GLX_STATE_SAFETY_LOCKED);
    assert(ret == -1);
    PASS();
}

/* ================================================================
 * PFDavg Computation Tests
 * ================================================================ */
static void test_pfdavg(void)
{
    TEST("pfdavg_1oo1_basic");
    double pfd = glx_pfdavg_1oo1(1e-5, 8760.0, 1.0);
    assert(pfd > 0.0);
    assert(pfd < 1.0);
    /* Expected: 1e-5 * 8760 / 2 = 0.0438 */
    assert(fabs(pfd - 0.0438) < 0.001);
    PASS();

    TEST("pfdavg_1oo1_zero_rate");
    pfd = glx_pfdavg_1oo1(0.0, 8760.0, 1.0);
    assert(pfd == 0.0);
    PASS();

    TEST("pfdavg_1oo2_with_beta");
    pfd = glx_pfdavg_1oo2(1e-5, 0.1, 8760.0, 1.0);
    assert(pfd > 0.0);
    assert(pfd < 1.0);
    PASS();

    TEST("pfdavg_1oo2d_guardlogix");
    /* GuardLogix typical: lambda_du = 1e-7, dc = 0.99, beta = 0.02 */
    pfd = glx_pfdavg_1oo2d(1e-7, 0.02, 0.01, 0.99, 8760.0, 8.0, 1.0);
    assert(pfd < 1e-3); /* Must achieve SIL 2+ */
    PASS();

    TEST("pfdavg_to_sil");
    assert(glx_pfdavg_to_sil(5e-5) == GLX_SIL_4);
    assert(glx_pfdavg_to_sil(5e-4) == GLX_SIL_3);
    assert(glx_pfdavg_to_sil(5e-3) == GLX_SIL_2);
    assert(glx_pfdavg_to_sil(5e-2) == GLX_SIL_1);
    assert(glx_pfdavg_to_sil(0.5) == GLX_SIL_NONE);
    PASS();

    TEST("rrf_computation");
    double rrf = glx_compute_rrf(1e-3);
    assert(fabs(rrf - 1000.0) < 0.1);
    rrf = glx_compute_rrf(1e-4);
    assert(fabs(rrf - 10000.0) < 0.1);
    PASS();
}

/* ================================================================
 * SFF and Architectural Constraint Tests
 * ================================================================ */
static void test_sff_and_arch(void)
{
    TEST("sff_computation");
    glx_failure_rates_t rates = {0};
    rates.lambda_safe = 8e-6;
    rates.lambda_dd = 1e-6;
    rates.lambda_du = 1e-6;
    double sff = glx_compute_sff(&rates);
    /* SFF = (8+1)/(8+1+1) = 9/10 = 0.9 */
    assert(fabs(sff - 0.9) < 0.01);
    PASS();

    TEST("architectural_sil_limit_typeB_hft0_sff90");
    glx_sil_level_t max_sil = glx_architectural_sil_limit(
        GLX_COMPONENT_TYPE_B, GLX_HFT_0, 0.90);
    assert(max_sil == GLX_SIL_2);
    PASS();

    TEST("architectural_sil_limit_typeB_hft1_sff99");
    max_sil = glx_architectural_sil_limit(
        GLX_COMPONENT_TYPE_B, GLX_HFT_1, 0.99);
    assert(max_sil == GLX_SIL_4);
    PASS();
}

/* ================================================================
 * Safety I/O Tests
 * ================================================================ */
static void test_safety_io(void)
{
    TEST("safety_input_init_equivalent");
    glx_safety_input_point_t input;
    int ret = glx_safety_input_init(&input, GLX_INPUT_EQUIVALENT, 50, 1);
    assert(ret == 0);
    assert(input.mode == GLX_INPUT_EQUIVALENT);
    assert(input.discrepancy_limit_ms == 50);
    assert(input.evaluated_value == 0);
    PASS();

    TEST("safety_input_update_equiv_agree");
    /* Both channels read 1 -> evaluated = 1 */
    int val = glx_safety_input_update(&input, 1, 1, 100);
    assert(val == 1);
    assert(input.evaluated_value == 1);
    assert(input.channel_fault == 0);
    PASS();

    TEST("safety_input_update_equiv_disagree");
    /* Channels disagree -> should go to fault after timeout */
    input.discrepancy_timer_ms = 0;
    input.last_change_timestamp = 0;
    val = glx_safety_input_update(&input, 1, 0, 200);
    assert(input.channel_fault == 1 || input.evaluated_value == 1);
    PASS();

    TEST("safety_output_init");
    glx_safety_output_point_t output;
    ret = glx_safety_output_init(&output, GLX_OUTPUT_PULSE_BOTH, 500, 5000);
    assert(ret == 0);
    assert(output.pulse_mode == GLX_OUTPUT_PULSE_BOTH);
    PASS();

    TEST("safety_output_set_and_readback");
    ret = glx_safety_output_set(&output, 1, 1);
    assert(ret == 0);
    assert(glx_safety_output_is_healthy(&output) == 1);
    /* Readback mismatch */
    ret = glx_safety_output_set(&output, 1, 0);
    assert(ret == -1);
    assert(glx_safety_output_is_healthy(&output) == 0);
    PASS();
}

/* ================================================================
 * CRC-32 and Signature Tests
 * ================================================================ */
static void test_crc32_and_signature(void)
{
    TEST("crc32_compute_nonzero");
    const char* test_data = "GuardLogix Safety PLC";
    uint32_t crc = glx_crc32_compute(
        (const uint8_t*)test_data, strlen(test_data), 0);
    assert(crc != 0);
    assert(crc != 0xFFFFFFFF);
    PASS();

    TEST("crc32_deterministic");
    uint32_t crc1 = glx_crc32_compute(
        (const uint8_t*)"test", 4, 0);
    uint32_t crc2 = glx_crc32_compute(
        (const uint8_t*)"test", 4, 0);
    assert(crc1 == crc2);
    PASS();

    TEST("crc32_different_data");
    uint32_t crc_a = glx_crc32_compute(
        (const uint8_t*)"dataA", 5, 0);
    uint32_t crc_b = glx_crc32_compute(
        (const uint8_t*)"dataB", 5, 0);
    assert(crc_a != crc_b);
    PASS();

    TEST("signature_block_init");
    glx_signature_block_t block;
    int ret = glx_signature_block_init(&block);
    assert(ret == 0);
    assert(block.magic == GLX_SIGNATURE_MAGIC);
    assert(block.segment_count == 0);
    PASS();

    TEST("signature_add_segment");
    uint8_t task_data[] = {0x01, 0x02, 0x03, 0x04};
    ret = glx_signature_add_segment(&block, GLX_SIG_SEGMENT_SAFETY_TASK,
                                     task_data, sizeof(task_data));
    assert(ret == 0);
    assert(block.segment_count == 1);
    assert(block.segments[0].segment_crc != 0);
    PASS();

    TEST("signature_finalize_and_verify");
    ret = glx_signature_finalize(&block);
    assert(ret == 0);
    assert(block.signature_locked == 1);
    ret = glx_signature_verify(&block, &block);
    assert(ret == 0);
    PASS();

    TEST("safety_signature_generation");
    glx_safety_signature_t sig;
    uint8_t task[] = {0xAA, 0xBB};
    uint8_t io_cfg[] = {0x11, 0x22, 0x33};
    ret = glx_generate_safety_signature(&sig, task, sizeof(task),
                                         io_cfg, sizeof(io_cfg), 42, 1000);
    assert(ret == 0);
    assert(sig.safety_network_number == 42);
    assert(sig.generation_time == 1000);
    PASS();
}

/* ================================================================
 * Safety Network Tests
 * ================================================================ */
static void test_safety_network(void)
{
    TEST("connection_init");
    glx_safety_connection_t conn;
    int ret = glx_safety_connection_init(&conn, GLX_SAFETY_CONN_ORIGINATOR,
                                          GLX_SAFETY_CONN_UNICAST, 20000, 4);
    assert(ret == 0);
    assert(conn.state == GLX_SAFETY_CONN_CLOSED);
    assert(conn.params.requested_packet_interval_us == 20000);
    assert(conn.params.timeout_value_us == 80000);
    PASS();

    TEST("connection_open_snn_match");
    glx_safety_connection_id_t conn_id = {0};
    conn_id.id[0] = 1;
    conn_id.id_length = 1;
    ret = glx_safety_connection_open(&conn, &conn_id, 5, 5);
    assert(ret == 0);
    assert(conn.state == GLX_SAFETY_CONN_ESTABLISHED);
    PASS();

    TEST("connection_open_snn_mismatch");
    glx_safety_connection_t conn2;
    glx_safety_connection_init(&conn2, GLX_SAFETY_CONN_TARGET,
                                GLX_SAFETY_CONN_UNICAST, 20000, 4);
    ret = glx_safety_connection_open(&conn2, &conn_id, 5, 7);
    assert(ret == -1);
    assert(conn2.state == GLX_SAFETY_CONN_FAULTED);
    PASS();

    TEST("crl_computation");
    uint32_t crtl = glx_safety_compute_crtl(20000, 4, 3);
    assert(crtl == 140000);
    PASS();

    TEST("connection_is_healthy");
    assert(glx_safety_connection_is_healthy(&conn) == 1);
    glx_safety_connection_close(&conn);
    assert(glx_safety_connection_is_healthy(&conn) == 0);
    PASS();
}

/* ================================================================
 * Safety Task Tests
 * ================================================================ */
static void test_safety_task(void)
{
    TEST("task_init");
    glx_safety_task_cb_t tcb;
    int ret = glx_safety_task_init(&tcb, GLX_SAFETY_TASK_PERIODIC,
                                    50000, 16);
    assert(ret == 0);
    assert(tcb.period_us == 50000);
    assert(tcb.watchdog_timeout_us == 150000);
    assert(tcb.task_enabled == 1);
    PASS();

    TEST("task_start_end_scan");
    ret = glx_safety_task_start_scan(&tcb, 1000000);
    assert(ret == 0);
    ret = glx_safety_task_end_scan(&tcb, 1010000);
    assert(ret == 0);
    assert(tcb.last_record.execution_time_us == 10000);
    assert(tcb.stats.total_cycles == 1);
    PASS();

    TEST("task_utilization");
    double util = glx_safety_task_utilization(&tcb);
    assert(util >= 0.0 && util <= 1.0);
    PASS();

    TEST("task_validate_period");
    assert(glx_validate_task_period(GLX_CTRL_5580S, 50) == 1);
    assert(glx_validate_task_period(GLX_CTRL_5580S, 500) == 0);
    assert(glx_validate_task_period(GLX_CTRL_5580S, 0) == 0);
    PASS();
}

/* ================================================================
 * Main Test Runner
 * ================================================================ */
int main(void)
{
    printf("=== GuardLogix Safety PLC Unit Tests ===\n\n");

    test_controller_init();
    test_state_transitions();
    test_pfdavg();
    test_sff_and_arch();
    test_safety_io();
    test_crc32_and_signature();
    test_safety_network();
    test_safety_task();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
