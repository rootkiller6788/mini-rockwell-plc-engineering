#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../include/ftview_alarm.h"
#include "../include/ftview_tag.h"
#include "../include/ftview_common.h"

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return; } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)
#define CHECK_EQ(a,b) do { if ((a)!=(b)) { printf("FAIL: %s == %s (%d vs %d)\n", #a,#b,(int)(a),(int)(b)); return; } } while(0)
#define CHECK_DBL_EQ(a,b,tol) do { if (fabs((a)-(b))>(tol)) { printf("FAIL: %s ≈ %s\n", #a,#b); return; } } while(0)

static void test_deadband_high_alarm(void) {
    TEST("Deadband: high alarm hysteresis");
    /* Not in alarm, crosses threshold → alarm */
    CHECK(ftview_alarm_deadband_check(51.0, 50.0, 2.0, true, false) == true);
    /* Just at threshold, not yet past → no alarm */
    CHECK(ftview_alarm_deadband_check(50.0, 50.0, 2.0, true, false) == false);
    /* In alarm, must drop below threshold - deadband to clear */
    CHECK(ftview_alarm_deadband_check(49.0, 50.0, 2.0, true, true) == true);  /* still in */
    CHECK(ftview_alarm_deadband_check(47.0, 50.0, 2.0, true, true) == false); /* cleared */
    PASS();
}

static void test_deadband_low_alarm(void) {
    TEST("Deadband: low alarm hysteresis");
    /* Not in alarm, drops below threshold → alarm */
    CHECK(ftview_alarm_deadband_check(29.0, 30.0, 2.0, false, false) == true);
    /* In alarm, must rise above threshold + deadband to clear */
    CHECK(ftview_alarm_deadband_check(31.0, 30.0, 2.0, false, true) == true);  /* still in */
    CHECK(ftview_alarm_deadband_check(33.0, 30.0, 2.0, false, true) == false); /* cleared */
    PASS();
}

static void test_on_delay_timer(void) {
    TEST("On-delay timer");
    int64_t acc = 0;
    /* Condition false → timer reset, not tripped */
    CHECK(ftview_alarm_on_delay_timer(false, 100, 0, &acc) == false);
    CHECK(acc == 0);

    /* With no real time accumulation in simulation, the timer
       relies on the caller to pass accumulated persistent duration.
       Verify the interface handles NULL timer correctly. */
    CHECK(ftview_alarm_on_delay_timer(true, 0, 0, NULL) == true);
    CHECK(ftview_alarm_on_delay_timer(false, 100, 0, NULL) == false);
    PASS();
}

static void test_off_delay_timer(void) {
    TEST("Off-delay timer");
    int64_t acc = 0;
    /* Condition true → definitely in alarm */
    CHECK(ftview_alarm_off_delay_timer(true, 100, 0, &acc) == true);
    CHECK(ftview_alarm_off_delay_timer(true, 0, 0, NULL) == true);
    PASS();
}

static void test_alarm_mgr_init(void) {
    TEST("Alarm manager initialisation");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);
    CHECK(mgr.def_count == 0);
    CHECK(mgr.instance_count == 0);
    CHECK(mgr.queue_count == 0);
    CHECK(mgr.log_count == 0);
    PASS();
}

static void test_alarm_add_def(void) {
    TEST("Alarm definition add");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);

    ftview_alarm_def_t def;
    memset(&def, 0, sizeof(def));
    strcpy(def.tag_name, "TIC101.PV");
    strcpy(def.message, "Reactor temperature high");
    strcpy(def.group, "Reactor");
    def.severity = FTVIEW_ALARM_SEV_HIGH;
    def.condition = FTVIEW_ALARM_COND_HI;
    def.threshold = 250.0;
    def.deadband = 2.0;
    def.enabled = true;

    CHECK(ftview_alarm_add_def(&mgr, &def) == FTVIEW_OK);
    CHECK(mgr.def_count == 1);
    CHECK(strcmp(mgr.defs[0].tag_name, "TIC101.PV") == 0);

    /* Duplicate tag name */
    CHECK(ftview_alarm_add_def(&mgr, &def) == FTVIEW_ERR_DUPLICATE_TAG);
    PASS();
}

static void test_alarm_acknowledge(void) {
    TEST("Alarm acknowledge");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);

    ftview_alarm_def_t def;
    memset(&def, 0, sizeof(def));
    strcpy(def.tag_name, "PIC101.PV");
    strcpy(def.message, "Pressure alarm");
    def.severity = FTVIEW_ALARM_SEV_MEDIUM;
    def.condition = FTVIEW_ALARM_COND_HI;
    def.threshold = 10.0;
    def.enabled = true;
    ftview_alarm_add_def(&mgr, &def);

    /* Create instance in UNACK_ALM state */
    mgr.instance_count = 1;
    mgr.instances[0].alarm_def_id = 1;
    mgr.instances[0].state = FTVIEW_ALARM_STATE_UNACK_ALM;

    CHECK(ftview_alarm_acknowledge(&mgr, 1, "operator1", "Acknowledged") == FTVIEW_OK);
    CHECK(mgr.instances[0].state == FTVIEW_ALARM_STATE_ACK_ALM);

    /* Acknowledge non-existent */
    CHECK(ftview_alarm_acknowledge(&mgr, 999, "op", "") == FTVIEW_ERR_TAG_NOT_FOUND);
    PASS();
}

static void test_alarm_shelve(void) {
    TEST("Alarm shelve/unshelve");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);

    ftview_alarm_def_t def;
    memset(&def, 0, sizeof(def));
    strcpy(def.tag_name, "TAG1");
    def.severity = FTVIEW_ALARM_SEV_LO;
    def.condition = FTVIEW_ALARM_COND_HI;
    def.threshold = 100;
    def.enabled = true;
    ftview_alarm_add_def(&mgr, &def);

    mgr.instance_count = 1;
    mgr.instances[0].alarm_def_id = 1;
    mgr.instances[0].state = FTVIEW_ALARM_STATE_UNACK_ALM;

    CHECK(ftview_alarm_shelve(&mgr, 1, 3600) == FTVIEW_OK);
    CHECK(mgr.instances[0].state == FTVIEW_ALARM_STATE_SHELVED);
    CHECK(mgr.instances[0].shelf_expiry_ms == 3600000);

    CHECK(ftview_alarm_unshelve(&mgr, 1) == FTVIEW_OK);
    CHECK(mgr.instances[0].state == FTVIEW_ALARM_STATE_NORMAL);
    PASS();
}

static void test_alarm_oos(void) {
    TEST("Alarm out-of-service / in-service");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);

    CHECK(ftview_alarm_out_of_service(&mgr, 1) == FTVIEW_OK);
    CHECK(mgr.instances[0].state == FTVIEW_ALARM_STATE_OO_SERVICE);

    CHECK(ftview_alarm_in_service(&mgr, 1) == FTVIEW_OK);
    CHECK(mgr.instances[0].state == FTVIEW_ALARM_STATE_NORMAL);
    PASS();
}

static void test_alarm_queue(void) {
    TEST("Alarm queue push/pop");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);

    /* Pre-fill queue (simulating evaluate) */
    for (uint32_t i = 0; i < 4; i++) {
        mgr.queue[mgr.queue_tail].alarm_id = i + 1;
        mgr.queue[mgr.queue_tail].severity = FTVIEW_ALARM_SEV_MEDIUM;
        mgr.queue[mgr.queue_tail].transition_to = FTVIEW_ALARM_STATE_UNACK_ALM;
        mgr.queue_tail = (mgr.queue_tail + 1) % FTVIEW_ALARM_QUEUE_SIZE;
        mgr.queue_count++;
    }
    CHECK(mgr.queue_count == 4);

    ftview_alarm_queue_entry_t out;
    CHECK(ftview_alarm_queue_pop(&mgr, &out) == true);
    CHECK(out.alarm_id == 1);
    CHECK(mgr.queue_count == 3);

    /* Peek */
    ftview_alarm_queue_entry_t peek_buf[8];
    uint32_t n = ftview_alarm_queue_peek(&mgr, peek_buf, 8);
    CHECK(n == 3);
    CHECK(peek_buf[0].alarm_id == 2);
    PASS();
}

static void test_alarm_rate(void) {
    TEST("Alarm rate calculation");
    ftview_alarm_mgr_t mgr;
    ftview_alarm_mgr_init(&mgr);

    /* Populate log with 6 alarms over a 1-hour window */
    for (int i = 0; i < 6; i++) {
        uint32_t idx = mgr.log_write_idx;
        mgr.log[idx].timestamp_ms = (int64_t)i * 600000;  /* every 10 min */
        mgr.log[idx].state = FTVIEW_ALARM_STATE_UNACK_ALM;
        mgr.log_write_idx = (mgr.log_write_idx + 1) % FTVIEW_ALARM_LOG_SIZE;
        mgr.log_count++;
    }

    double rate = ftview_alarm_rate_per_hour(&mgr, 3600000, 3600000);
    /* 6 alarms in 1 hour → rate ≈ 6 alarms/hr */
    CHECK(rate > 0.0 && rate < 20.0);
    PASS();
}

int main(void) {
    printf("=== mini-factorytalk-view-hmi: Alarm Management Tests ===\n\n");

    test_deadband_high_alarm();
    test_deadband_low_alarm();
    test_on_delay_timer();
    test_off_delay_timer();
    test_alarm_mgr_init();
    test_alarm_add_def();
    test_alarm_acknowledge();
    test_alarm_shelve();
    test_alarm_oos();
    test_alarm_queue();
    test_alarm_rate();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
