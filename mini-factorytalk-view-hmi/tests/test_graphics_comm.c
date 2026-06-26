#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../include/ftview_graphics.h"
#include "../include/ftview_communication.h"
#include "../include/ftview_datalog.h"
#include "../include/ftview_common.h"

static int tests_run = 0, tests_passed = 0;
#define TEST(n) do { tests_run++; printf("  %s ... ", n); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(m) do { printf("FAIL: %s\n", m); return; } while(0)
#define CHECK(c) do { if (!(c)) { FAIL(#c); return; } } while(0)
#define CHECK_EQ(a,b) do { if((a)!=(b)){printf("FAIL: %s==%s(%dvs%d)\n",#a,#b,(int)(a),(int)(b));return;}}while(0)
#define CHECK_DBL(a,b,t) do { if(fabs((a)-(b))>(t)){printf("FAIL: %s~%s\n",#a,#b);return;}}while(0)

/* ─── Graphics Tests ─── */

static void test_hit_test(void) {
    TEST("AABB hit-test");
    ftview_display_screen_t screen;
    memset(&screen, 0, sizeof(screen));

    ftview_display_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.id = 1;
    obj.type = FTVIEW_OBJ_BUTTON;
    obj.bounds.x = 10; obj.bounds.y = 20;
    obj.bounds.w = 100; obj.bounds.h = 50;
    obj.z_order = 1;
    obj.visible = true;
    obj.enabled = true;
    memcpy(&screen.objects[0], &obj, sizeof(obj));
    screen.object_count = 1;

    CHECK(ftview_display_hit_test(&screen, 60, 45) == 1);   /* centre: hit */
    CHECK(ftview_display_hit_test(&screen, 5, 45) == 0);    /* left: miss */
    CHECK(ftview_display_hit_test(&screen, 60, 5) == 0);    /* above: miss */
    CHECK(ftview_display_hit_test(&screen, 10, 20) == 1);   /* corner: hit */
    PASS();
}

static void test_zorder_sort(void) {
    TEST("Z-order sorting");
    ftview_display_mgr_t mgr;
    ftview_display_mgr_init(&mgr);

    ftview_display_screen_t screen;
    memset(&screen, 0, sizeof(screen));
    strcpy(screen.name, "TestScreen");
    ftview_display_add_screen(&mgr, &screen);

    /* Add objects in reverse z-order */
    ftview_display_object_t obj;
    memset(&obj, 0, sizeof(obj));
    obj.type = FTVIEW_OBJ_RECTANGLE;
    obj.z_order = 3;
    mgr.screens[0].objects[0] = obj; mgr.screens[0].object_count++;
    obj.z_order = 1;
    mgr.screens[0].objects[1] = obj; mgr.screens[0].object_count++;
    obj.z_order = 2;
    mgr.screens[0].objects[2] = obj; mgr.screens[0].object_count++;

    ftview_display_sort_zorder(&mgr, 1);
    CHECK(mgr.screens[0].objects[0].z_order == 1);
    CHECK(mgr.screens[0].objects[1].z_order == 2);
    CHECK(mgr.screens[0].objects[2].z_order == 3);
    PASS();
}

static void test_bilinear_color(void) {
    TEST("Bilinear colour interpolation");
    ftview_color_t TL = {255, 0, 0};    /* red */
    ftview_color_t TR = {0, 255, 0};    /* green */
    ftview_color_t BL = {0, 0, 255};    /* blue */
    ftview_color_t BR = {255, 255, 0};  /* yellow */

    /* Centre interpolation (u=0.5, v=0.5) */
    ftview_color_t c = ftview_color_bilinear(&TL, &TR, &BL, &BR, 0.5, 0.5);
    /* Expected: (0.25*R, 0.25*G, 0.25*B + 0.25*Y) = approx (128,128,128) */
    CHECK(c.r >= 120 && c.r <= 135);
    CHECK(c.g >= 120 && c.g <= 135);

    /* Corner: u=0, v=0 → top-left */
    c = ftview_color_bilinear(&TL, &TR, &BL, &BR, 0.0, 0.0);
    CHECK(c.r == 255 && c.g == 0 && c.b == 0);
    PASS();
}

static void test_animation_eval(void) {
    TEST("Animation evaluation");
    ftview_animation_t anim;
    memset(&anim, 0, sizeof(anim));
    anim.type = FTVIEW_ANIM_FILL_PCT;
    anim.range_min = 0.0;
    anim.range_max = 100.0;
    anim.output_min = 0.0;
    anim.output_max = 1.0;

    ftview_color_t color = {0,0,0};
    bool visible = true;
    double result = ftview_animation_evaluate(&anim, 50.0, &color, &visible);
    CHECK_DBL(result, 0.5, 0.001);

    result = ftview_animation_evaluate(&anim, 100.0, &color, &visible);
    CHECK_DBL(result, 1.0, 0.001);
    PASS();
}

static void test_navigation(void) {
    TEST("Screen navigation");
    ftview_display_mgr_t mgr;
    ftview_display_mgr_init(&mgr);

    ftview_display_screen_t s1, s2;
    memset(&s1, 0, sizeof(s1)); strcpy(s1.name, "Overview"); s1.id = 1;
    memset(&s2, 0, sizeof(s2)); strcpy(s2.name, "Detail");  s2.id = 2;

    mgr.screens[0] = s1; mgr.screen_count++;
    mgr.screens[1] = s2; mgr.screen_count++;

    ftview_display_add_nav_link(&mgr, 1, 2);
    CHECK(ftview_display_navigate(&mgr, 1) == FTVIEW_OK);
    CHECK(mgr.active_screen_id == 1);

    CHECK(ftview_display_navigate(&mgr, 2) == FTVIEW_OK);
    CHECK(mgr.active_screen_id == 2);
    CHECK(mgr.previous_screen_id == 1);

    CHECK(ftview_display_navigate_back(&mgr) == FTVIEW_OK);
    CHECK(mgr.active_screen_id == 1);
    PASS();
}

/* ─── Data Log Tests ─── */

static void test_datalog_create(void) {
    TEST("Data log model creation");
    ftview_datalog_mgr_t mgr;
    ftview_datalog_mgr_init(&mgr);

    ftview_datalog_model_t model;
    memset(&model, 0, sizeof(model));
    strcpy(model.name, "Trend1");
    model.trigger_type = FTVIEW_DATALOG_TRIG_PERIODIC;
    model.interval_ms = 1000;

    CHECK(ftview_datalog_create_model(&mgr, &model) == FTVIEW_OK);
    CHECK(mgr.model_count == 1);
    PASS();
}

static void test_datalog_sample(void) {
    TEST("Data log sample and query");
    ftview_datalog_mgr_t mgr;
    ftview_datalog_mgr_init(&mgr);

    ftview_datalog_model_t model;
    memset(&model, 0, sizeof(model));
    strcpy(model.name, "Log1");
    model.trigger_type = FTVIEW_DATALOG_TRIG_PERIODIC;
    model.interval_ms = 1000;
    model.tag_count = 1;
    ftview_datalog_create_model(&mgr, &model);

    ftview_datalog_sample_t sample;
    for (int i = 0; i < 5; i++) {
        memset(&sample, 0, sizeof(sample));
        sample.timestamp_ms = i * 1000;
        sample.values[0] = 100.0 + i * 10.0;
        sample.tag_count = 1;
        sample.qualities[0] = FTVIEW_QUALITY_GOOD;
        CHECK(ftview_datalog_sample(&mgr, 1, &sample) == 0);
    }

    ftview_datalog_sample_t out[10];
    uint32_t n = ftview_datalog_query_range(&mgr, 1, 0, 10000, out, 10);
    CHECK(n == 5);
    CHECK_DBL(out[0].values[0], 100.0, 0.001);
    CHECK_DBL(out[4].values[0], 140.0, 0.001);
    PASS();
}

static void test_swinging_door(void) {
    TEST("Swinging-door compression");
    ftview_datalog_sample_t last, cand;
    memset(&last, 0, sizeof(last));
    memset(&cand, 0, sizeof(cand));
    last.values[0] = 100.0;
    last.tag_count = 1;
    cand.values[0] = 100.1;
    cand.tag_count = 1;

    /* Within 0.2 band → discard */
    CHECK(ftview_datalog_swinging_door(&last, &cand, 0, 0.2) == false);

    /* Outside band → store */
    cand.values[0] = 101.0;
    CHECK(ftview_datalog_swinging_door(&last, &cand, 0, 0.2) == true);
    PASS();
}

static void test_interpolation(void) {
    TEST("Piecewise linear interpolation");
    ftview_datalog_sample_t s0, s1;
    memset(&s0, 0, sizeof(s0));
    memset(&s1, 0, sizeof(s1));
    s0.timestamp_ms = 0;    s0.values[0] = 100.0;
    s1.timestamp_ms = 1000; s1.values[0] = 200.0;
    s0.tag_count = 1; s1.tag_count = 1;
    s0.qualities[0] = FTVIEW_QUALITY_GOOD;
    s1.qualities[0] = FTVIEW_QUALITY_GOOD;

    double val; ftview_quality_t q;
    /* Midpoint interpolation */
    CHECK(ftview_datalog_interpolate(&s0, &s1, 0, 500, &val, &q) == FTVIEW_OK);
    CHECK_DBL(val, 150.0, 0.01);
    CHECK(q == FTVIEW_QUALITY_GOOD);

    /* At endpoint */
    CHECK(ftview_datalog_interpolate(&s0, &s1, 0, 0, &val, &q) == FTVIEW_OK);
    CHECK_DBL(val, 100.0, 0.01);
    PASS();
}

static void test_statistics(void) {
    TEST("Data log statistics");
    ftview_datalog_mgr_t mgr;
    ftview_datalog_mgr_init(&mgr);

    ftview_datalog_model_t model;
    memset(&model, 0, sizeof(model));
    strcpy(model.name, "StatLog");
    model.tag_count = 1;
    ftview_datalog_create_model(&mgr, &model);

    double values[] = {10.0, 12.0, 14.0, 16.0, 18.0};
    for (int i = 0; i < 5; i++) {
        ftview_datalog_sample_t s;
        memset(&s, 0, sizeof(s));
        s.timestamp_ms = i * 1000;
        s.values[0] = values[i];
        s.tag_count = 1;
        s.qualities[0] = FTVIEW_QUALITY_GOOD;
        ftview_datalog_sample(&mgr, 1, &s);
    }

    double min_v, max_v, avg, stddev;
    CHECK(ftview_datalog_statistics(&mgr, 1, 0, 0, 10000,
                                     &min_v, &max_v, &avg, &stddev) == FTVIEW_OK);
    CHECK_DBL(min_v, 10.0, 0.01);
    CHECK_DBL(max_v, 18.0, 0.01);
    CHECK_DBL(avg, 14.0, 0.01);
    /* stddev of [10,12,14,16,18]: mean=14, squared diffs: 16,4,0,4,16 → var=8 → stddev≈2.828 */
    CHECK_DBL(stddev, 2.828, 0.1);
    PASS();
}

/* ─── Communication Tests ─── */

static void test_comm_topic(void) {
    TEST("Communication topic management");
    ftview_comm_mgr_t mgr;
    ftview_comm_mgr_init(&mgr);

    CHECK(ftview_comm_add_topic(&mgr, "PLC1", "AB_ETH-1\\192.168.1.10\\Backplane\\0") == FTVIEW_OK);
    CHECK(mgr.topic_count == 1);
    CHECK(strcmp(mgr.topics[0].name, "PLC1") == 0);

    /* Duplicate */
    CHECK(ftview_comm_add_topic(&mgr, "PLC1", "other") == FTVIEW_ERR_DUPLICATE_TAG);
    PASS();
}

static void test_comm_connect(void) {
    TEST("Connection lifecycle");
    ftview_comm_mgr_t mgr;
    ftview_comm_mgr_init(&mgr);

    ftview_comm_add_topic(&mgr, "PLC1", "AB_ETH-1\\192.168.1.10\\Backplane\\0");
    uint32_t cid = ftview_comm_connect(&mgr, 1);
    CHECK(cid > 0);
    CHECK(mgr.topics[0].connected == true);

    /* Read via connected topic */
    uint32_t rid = ftview_comm_read(&mgr, 1, "N7:0");
    CHECK(rid > 0);

    /* Poll for completion */
    uint32_t done = ftview_comm_poll(&mgr, 1000);
    CHECK(done >= 1);

    /* Get result */
    ftview_value_t val;
    CHECK(ftview_comm_get_result(&mgr, rid, &val) == FTVIEW_OK);

    CHECK(ftview_comm_disconnect(&mgr, cid) == FTVIEW_OK);
    CHECK(mgr.topics[0].connected == false);
    PASS();
}

static void test_comm_drt_resolve(void) {
    TEST("DRT resolution via communication");
    ftview_comm_mgr_t mgr;
    ftview_comm_mgr_init(&mgr);

    ftview_comm_add_topic(&mgr, "PLC1", "AB_ETH-1\\192.168.1.10\\Backplane\\0");

    uint32_t topic_id;
    char addr[128];
    CHECK(ftview_comm_resolve_drt(&mgr, "{[PLC1]N7:0}", &topic_id, addr, sizeof(addr)) == FTVIEW_OK);
    CHECK(topic_id == 1);
    CHECK(strcmp(addr, "N7:0") == 0);
    PASS();
}

int main(void) {
    printf("=== mini-factorytalk-view-hmi: Graphics/Datalog/Comm Tests ===\n\n");

    test_hit_test();
    test_zorder_sort();
    test_bilinear_color();
    test_animation_eval();
    test_navigation();
    test_datalog_create();
    test_datalog_sample();
    test_swinging_door();
    test_interpolation();
    test_statistics();
    test_comm_topic();
    test_comm_connect();
    test_comm_drt_resolve();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
