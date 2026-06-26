#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../include/ethernet_ip_cip.h"
#include "../include/cip_message.h"
#include "../include/cip_connection.h"
#include "../include/cip_services.h"
#include "../include/cip_object.h"
#include "../include/ethernet_ip_encap.h"
#include "../include/cip_safety.h"

static int tests_passed = 0;
static int tests_failed = 0;
#define TEST(n) printf("  TEST: %s ... ", n)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); tests_failed++; } while(0)

/* L1 tests */
static void test_class_codes(void) {
    TEST("CIP class codes");
    assert(CIP_CLASS_IDENTITY == 0x01u);
    assert(CIP_CLASS_ASSEMBLY == 0x04u);
    assert(CIP_CLASS_CONNECTION_MANAGER == 0x06u);
    assert(CIP_CLASS_TCPIP_INTERFACE == 0xF5u);
    assert(CIP_CLASS_ETHERNET_LINK == 0xF6u);
    PASS();
}
static void test_conn_types(void) {
    TEST("Connection types");
    assert(CIP_CONN_TYPE_EXCLUSIVE_OWNER == 0x00u);
    assert(CIP_CONN_TYPE_INPUT_ONLY == 0x01u);
    assert(CIP_CONN_TYPE_LISTEN_ONLY == 0x02u);
    assert(CIP_CONN_TYPE_REDUNDANT_OWNER == 0x03u);
    PASS();
}
static void test_transport_types(void) {
    TEST("Transport class/trigger");
    assert(CIP_TRANSPORT_CLASS_0 == 0x00u);
    assert(CIP_TRANSPORT_CLASS_1 == 0x01u);
    assert(CIP_TRIGGER_CYCLIC == 0x00u);
    assert(CIP_TRIGGER_CHANGE_OF_STATE == 0x01u);
    PASS();
}

/* L2: Connection Management */
static void test_conn_init(void) {
    TEST("cip_conn_init defaults");
    cip_connection_t conn;
    cip_conn_init(&conn);
    assert(conn.connection_state == CIP_CONN_STATE_NONEXISTENT);
    assert(conn.rpi_us == CIP_DEFAULT_RPI_US);
    assert(conn.timeout_multiplier == CIP_DEFAULT_TIMEOUT_MULTIPLIER);
    assert(conn.connection_type == CIP_CONN_TYPE_EXCLUSIVE_OWNER);
    assert(conn.transport_class == CIP_TRANSPORT_CLASS_1);
    assert(conn.is_active == 0);
    PASS();
}
static void test_conn_configure(void) {
    TEST("cip_conn_configure valid");
    cip_connection_t conn;
    cip_conn_init(&conn);
    assert(cip_conn_configure(&conn, 64, 32, 0x64, 0x65, 5000) == 0);
    assert(conn.o_to_t_size == 64);
    assert(conn.t_to_o_size == 32);
    assert(conn.rpi_us == 5000);
    PASS();
}
static void test_conn_oversize(void) {
    TEST("cip_conn_configure rejects oversize");
    cip_connection_t conn;
    cip_conn_init(&conn);
    assert(cip_conn_configure(&conn, 40000, 40000, 0x64, 0x65, 5000) == -1);
    PASS();
}
static void test_state_transitions(void) {
    TEST("connection state machine");
    cip_connection_t conn;
    cip_conn_init(&conn);
    assert(cip_conn_transition(&conn, CIP_CONN_STATE_CONFIGURING) == 0);
    assert(conn.connection_state == CIP_CONN_STATE_CONFIGURING);
    assert(cip_conn_transition(&conn, CIP_CONN_STATE_WAITING_FOR_ID) == 0);
    assert(cip_conn_transition(&conn, CIP_CONN_STATE_ESTABLISHED) == 0);
    assert(conn.is_active == 1);
    PASS();
}
static void test_illegal_transition(void) {
    TEST("blocks illegal transition");
    cip_connection_t conn;
    cip_conn_init(&conn);
    conn.connection_state = CIP_CONN_STATE_ESTABLISHED;
    assert(cip_conn_transition(&conn, CIP_CONN_STATE_CONFIGURING) == -3);
    PASS();
}
static void test_timeout(void) {
    TEST("timeout = RPI*4*multiplier");
    cip_connection_t conn;
    cip_conn_init(&conn);
    conn.rpi_us = 10000;
    conn.timeout_multiplier = 4;
    assert(cip_conn_calculate_timeout(&conn) == 160000);
    PASS();
}
static void test_transport(void) {
    TEST("Class1 + COS");
    cip_connection_t conn;
    cip_conn_init(&conn);
    assert(cip_conn_set_transport(&conn, CIP_TRANSPORT_CLASS_1, CIP_TRIGGER_CHANGE_OF_STATE) == 0);
    PASS();
}
static void test_transport_class0_reject(void) {
    TEST("Class0 rejects COS");
    cip_connection_t conn;
    cip_conn_init(&conn);
    assert(cip_conn_set_transport(&conn, CIP_TRANSPORT_CLASS_0, CIP_TRIGGER_CHANGE_OF_STATE) == -4);
    PASS();
}
static void test_conn_validate(void) {
    TEST("conn_validate");
    cip_connection_t conn;
    cip_conn_init(&conn);
    cip_conn_configure(&conn, 64, 32, 0x64, 0x65, 5000);
    assert(cip_conn_validate(&conn) == 0);
    PASS();
}
static void test_conn_close(void) {
    TEST("conn_close resets");
    cip_connection_t conn;
    cip_conn_init(&conn);
    cip_conn_transition(&conn, CIP_CONN_STATE_CONFIGURING);
    cip_conn_close(&conn);
    assert(conn.connection_state == CIP_CONN_STATE_NONEXISTENT);
    PASS();
}
static void test_serial(void) {
    TEST("serial incrementing");
    uint16_t s1 = cip_conn_next_serial();
    uint16_t s2 = cip_conn_next_serial();
    assert(s1 > 0);
    assert(s2 == s1 + 1);
    PASS();
}
static void test_api(void) {
    TEST("API ceil(RPI/tick)*tick");
    assert(cip_conn_calculate_api(12000, 5000) == 15000);
    assert(cip_conn_calculate_api(5000, 5000) == 5000);
    assert(cip_conn_calculate_api(5500, 5000) == 10000);
    PASS();
}

/* L3: EPATH and Message Router */
static void test_mr_init(void) {
    TEST("MR request init");
    cip_mr_request_t req;
    memset(&req, 0xFF, sizeof(req));
    cip_mr_request_init(&req);
    assert(req.service == 0);
    assert(req.request_data_size == 0);
    PASS();
}
static void test_epath_class8(void) {
    TEST("EPATH 8-bit class");
    uint8_t buf[4];
    assert(cip_epath_encode_class(buf, 0x04) == 2);
    assert(buf[0] == EPATH_SEGMENT_LOGICAL_CLASS);
    assert(buf[1] == 0x04);
    PASS();
}
static void test_epath_class16(void) {
    TEST("EPATH 16-bit class");
    uint8_t buf[4];
    /* 8-bit encoding with 2-byte format for all values */
    assert(cip_epath_encode_class(buf, 0xF5) == 2);
    assert(buf[0] == EPATH_SEGMENT_LOGICAL_CLASS);
    assert(buf[1] == 0xF5);
    PASS();
}
static void test_epath_build(void) {
    TEST("EPATH build");
    uint8_t buf[32];
    int n = cip_epath_build(buf, 32, 0x04, 0x01, 0x03);
    assert(n == 6); /* 2 bytes per segment: class + instance + attribute */
    PASS();
}
static void test_epath_decode(void) {
    TEST("EPATH decode");
    uint8_t buf[32];
    int n = cip_epath_build(buf, 32, 0x04, 0x01, 0x03);
    cip_path_t path;
    assert(cip_epath_decode(buf, (uint8_t)n, &path) == 0);
    assert(path.class_id == 0x04);
    assert(path.instance_id == 0x01);
    PASS();
}
static void test_status(void) {
    TEST("status strings");
    assert(strcmp(cip_status_string(0x00), "Success") == 0);
    assert(strcmp(cip_status_string(0x08), "Service not supported") == 0);
    PASS();
}

/* L4: Safety */
static void test_safety_init(void) {
    TEST("safety init");
    cip_safety_config_t cfg;
    cip_safety_init(&cfg);
    assert(cfg.watchdog_timeout_ms == 50);
    assert(cfg.safety_state == CIP_SAFETY_STATE_IDLE);
    PASS();
}
static void test_safety_snn(void) {
    TEST("safety SNN validation");
    cip_safety_config_t cfg;
    cip_safety_init(&cfg);
    assert(cip_safety_validate_snn(&cfg) == 0);
    uint8_t snn[6] = {1,2,3,4,5,6};
    cip_safety_set_snn(&cfg, snn);
    assert(cip_safety_validate_snn(&cfg) == 1);
    PASS();
}
static void test_safety_crc(void) {
    TEST("safety CRC deterministic");
    uint8_t d[4] = {1,2,3,4};
    uint32_t c1, c2;
    cip_safety_calculate_crc(d, 4, &c1);
    cip_safety_calculate_crc(d, 4, &c2);
    assert(c1 == c2);
    assert(c1 != 0);
    PASS();
}
static void test_safety_state(void) {
    TEST("safety state transitions");
    cip_safety_config_t cfg;
    cip_safety_init(&cfg);
    assert(cip_safety_transition_state(&cfg, CIP_SAFETY_STATE_CONFIGURING) == 0);
    cip_safety_init(&cfg);
    assert(cip_safety_transition_state(&cfg, CIP_SAFETY_STATE_FAULTED) == -3);
    PASS();
}

/* L5: Services & Algorithms */
static void test_svc_registry(void) {
    TEST("service registry");
    uint8_t code;
    assert(cip_service_get_code_by_name("Get_Attribute_Single", &code) == 0);
    assert(code == 0x0E);
    assert(strcmp(cip_service_get_name(0x0E), "Get_Attribute_Single") == 0);
    PASS();
}
static void test_svc_path(void) {
    TEST("service path requirements");
    assert(cip_service_is_path_required(0x0E) == 1);
    assert(cip_service_is_path_required(0x05) == 0);
    PASS();
}
static void test_svc_execute(void) {
    TEST("Get_Attribute_Single execution");
    cip_service_data_t svc;
    svc.service = 0x0E;
    svc.data[0] = 0x42;
    svc.data_size = 1;
    uint8_t resp[256];
    uint16_t sz;
    assert(cip_service_execute_get_attribute_single(&svc, resp, &sz) == 0);
    assert(resp[0] == (0x0E | 0x80));
    assert(resp[2] == 0x00);
    PASS();
}
static void test_fo_parse(void) {
    TEST("Forward Open response parse");
    uint8_t data[42];
    memset(data, 0, 42);
    data[4]=1; data[7]=4;
    data[8]=2; data[11]=0x80;
    data[12]=0x42;
    cip_forward_open_response_t resp;
    assert(cip_fo_parse_response(data, 42, &resp) == 0);
    assert(resp.o_to_t_connection_id == 0x04000001u);
    assert(resp.t_to_o_connection_id == 0x80000002u);
    PASS();
}
static void test_assembly(void) {
    TEST("assembly pack/unpack");
    cip_da_template_t tmpl;
    cip_da_init_template(&tmpl, 100);
    cip_da_add_member(&tmpl, 1, 0xC4, 4, 0, 0, "V1");
    cip_da_add_member(&tmpl, 2, 0xCA, 4, 0, 0, "V2");
    assert(cip_da_calculate_size(&tmpl) == 8);
    uint8_t d1[4]={0x2A,0,0,0}, d2[4]={0,0,0x80,0x3F};
    const uint8_t *p[2]={d1,d2};
    uint16_t s[2]={4,4};
    uint8_t buf[16];
    assert(cip_da_pack_data(&tmpl, p, s, buf, 16) == 0);
    uint8_t o1[4], o2[4];
    uint8_t *op[2]={o1,o2};
    uint16_t os[2]={4,4};
    assert(cip_da_unpack_data(&tmpl, buf, 8, op, os) == 0);
    assert(memcmp(o1, d1, 4) == 0);
    PASS();
}
static void test_tag_validate(void) {
    TEST("tag name validation");
    assert(cip_tag_validate_name("MyTag") == 1);
    assert(cip_tag_validate_name("_private") == 1);
    assert(cip_tag_validate_name("123start") == 0);
    assert(cip_tag_validate_name("") == 0);
    PASS();
}

/* L6: Object Model */
static void test_encap_header(void) {
    TEST("encap header init");
    eip_encap_header_t hdr;
    eip_encap_init_header(&hdr, 0x0065, 0, 0, 0);
    assert(hdr.command == 0x0065);
    assert(hdr.status == 0);
    PASS();
}
static void test_encap_cmd_str(void) {
    TEST("encap command strings");
    assert(strcmp(eip_encap_command_string(0x006F), "SendRRData") == 0);
    assert(strcmp(eip_encap_command_string(0x0070), "SendUnitData") == 0);
    PASS();
}
static void test_identity_obj(void) {
    TEST("identity object init");
    cip_identity_object_t id;
    cip_obj_identity_init(&id, 1, 0x0E, 0x100, 32, 11, 0x12345678, "1756-L83E");
    assert(id.vendor_id == 1);
    assert(id.major_revision == 32);
    assert(strcmp(id.product_name, "1756-L83E") == 0);
    PASS();
}
static void test_class_names(void) {
    TEST("class name lookup");
    assert(strcmp(cip_obj_class_name(0x01), "Identity") == 0);
    assert(strcmp(cip_obj_class_name(0x04), "Assembly") == 0);
    assert(strcmp(cip_obj_class_name(0xF5), "TCP/IP Interface") == 0);
    PASS();
}
static void test_type_names(void) {
    TEST("type name lookup");
    assert(strcmp(cip_obj_attr_type_name(0xC4), "DINT") == 0);
    assert(strcmp(cip_obj_attr_type_name(0xCA), "REAL") == 0);
    assert(strcmp(cip_obj_attr_type_name(0xC1), "BOOL") == 0);
    PASS();
}

int main(void) {
    printf("=== EtherNet/IP & CIP Module Tests ===\n\n");
    printf("L1: Core Definitions\n");
    test_class_codes(); test_conn_types(); test_transport_types();

    printf("\nL2: Connection Management\n");
    test_conn_init(); test_conn_configure(); test_conn_oversize();
    test_state_transitions(); test_illegal_transition(); test_timeout();
    test_transport(); test_transport_class0_reject(); test_conn_validate();
    test_conn_close(); test_serial(); test_api();

    printf("\nL3: EPATH and Message Router\n");
    test_mr_init(); test_epath_class8(); test_epath_class16();
    test_epath_build(); test_epath_decode(); test_status();

    printf("\nL4: Safety and Engineering Laws\n");
    test_safety_init(); test_safety_snn(); test_safety_crc();
    test_safety_state();

    printf("\nL5: Services and Algorithms\n");
    test_svc_registry(); test_svc_path(); test_svc_execute();
    test_fo_parse(); test_assembly(); test_tag_validate();

    printf("\nL6: Encapsulation and Object Model\n");
    test_encap_header(); test_encap_cmd_str();
    test_identity_obj(); test_class_names(); test_type_names();

    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
