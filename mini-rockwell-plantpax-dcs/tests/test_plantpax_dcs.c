/**
 * @file test_plantpax_dcs.c
 * @brief Comprehensive test suite for PlantPAx DCS module
 *
 * Tests all core APIs across architecture, controller, I/O, alarms,
 * historian, and control loop subsystems using assert-based verification.
 */
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "plantpax_architecture.h"
#include "plantpax_controller.h"
#include "plantpax_io_subsystem.h"
#include "plantpax_alarm_manager.h"
#include "plantpax_historian.h"
#include "plantpax_control_loop.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  TEST: %s... ", name); \
    tests_run++; \
} while(0)

#define PASS() do { \
    printf("PASSED\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAILED: %s\n", msg); \
} while(0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); return; } \
} while(0)

#define CHECK_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("FAILED: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); \
        return; \
    } \
} while(0)

#define CHECK_DOUBLE_EQ(a, b, tol, msg) do { \
    if (fabs((a) - (b)) > (tol)) { \
        printf("FAILED: %s (got %f, expected %f)\n", msg, (a), (b)); \
        return; \
    } \
} while(0)

#define CHECK_NONNULL(ptr, msg) do { \
    if ((ptr) == NULL) { FAIL(msg); return; } \
} while(0)

/* ---------------------------------------------------------------------------
 * Architecture Tests (L1-L4)
 * ------------------------------------------------------------------------- */

static void test_system_config_init(void)
{
    TEST("system_config_init");
    pax_system_config_t config;
    pax_system_config_init(&config, "TestProject", "TestPlant");
    CHECK_EQ(strcmp(config.project_name, "TestProject"), 0,
             "project_name mismatch");
    CHECK_EQ(config.num_entities, 0, "num_entities not zero");
    CHECK_EQ(config.project_revision, 1, "revision not 1");
    PASS();
}

static void test_entity_registration(void)
{
    TEST("entity_registration");
    pax_entity_t entity;
    int rc = pax_register_entity(&entity, PAX_ENT_UNIT, "R-101", 0);
    CHECK_EQ(rc, 0, "register_entity failed");
    CHECK_EQ(entity.level, PAX_ENT_UNIT, "entity level mismatch");
    CHECK_EQ(strcmp(entity.tag_name, "R-101"), 0, "tag name mismatch");
    CHECK_EQ(entity.state, PAX_STATE_IDLE, "initial state not IDLE");
    PASS();
}

static void test_procedural_state_names(void)
{
    TEST("procedural_state_names");
    CHECK_EQ(strcmp(pax_procedural_state_name(PAX_STATE_RUNNING), "RUNNING"), 0,
             "state name mismatch");
    CHECK_EQ(strcmp(pax_procedural_state_name(PAX_STATE_HELD), "HELD"), 0,
             "state name mismatch");
    PASS();
}

static void test_system_availability(void)
{
    TEST("system_availability");
    double comps[] = {99.9, 99.9, 99.9, 99.9};
    double avail = pax_compute_system_availability(comps, 4, 0);
    /* Series: 0.999^4 = 0.996006 -> 99.6006% */
    CHECK(avail > 99.5 && avail < 99.7, "series availability wrong");

    /* Redundant pair of 99.9% each */
    double avail_redun = pax_compute_system_availability(comps, 2, 1);
    /* Redundant: 1-(1-0.999)^2 = 0.999999 -> 99.9999% */
    CHECK(avail_redun > 99.99, "redundant availability wrong");
    PASS();
}

static void test_network_load(void)
{
    TEST("network_load");
    pax_network_segment_t seg;
    pax_register_network_segment(&seg, PAX_TOPOLOGY_STAR,
                                  PAX_NET_ETHERNET_IP, 100.0, 64);
    seg.active_nodes = 10;
    seg.rpi_ms = 20.0;
    double load = pax_compute_network_load(&seg);
    CHECK(load >= 0.0 && load < 5.0, "unexpected network load");
    PASS();
}

/* ---------------------------------------------------------------------------
 * Controller Tests (L1-L5)
 * ------------------------------------------------------------------------- */

static void test_ctrl_spec_init(void)
{
    TEST("ctrl_spec_init");
    pax_ctrl_spec_t spec;
    pax_ctrl_spec_init(&spec, PAX_CTRL_CONTROLLOGIX_L8);
    CHECK(strcmp(spec.model_number, "1756-L85E") == 0, "wrong model");
    CHECK(spec.supports_redundancy == true, "L8x should support redundancy");
    CHECK(spec.supports_safety == true, "L8x should support safety");
    PASS();
}

static void test_controller_add_task(void)
{
    TEST("controller_add_task");
    pax_controller_t ctrl;
    pax_controller_init(&ctrl, "CTRL_01", PAX_CTRL_CONTROLLOGIX_L8);
    int idx = pax_controller_add_task(&ctrl, "PID_Task",
                                       PAX_TASK_PERIODIC,
                                       PAX_PRIORITY_HIGH, 100.0);
    CHECK(idx == 1, "task not added at expected index");
    CHECK(ctrl.num_tasks == 2, "num_tasks not incremented");
    PASS();
}

static void test_controller_scan(void)
{
    TEST("controller_scan");
    pax_controller_t ctrl;
    pax_controller_init(&ctrl, "CTRL_02", PAX_CTRL_CONTROLLOGIX_L8);
    pax_controller_add_task(&ctrl, "FastTask", PAX_TASK_PERIODIC,
                             PAX_PRIORITY_HIGH, 50.0);
    int rc = pax_controller_simulate_scan(&ctrl, 1, 5.0);
    CHECK_EQ(rc, 0, "scan simulation failed");
    CHECK(ctrl.tasks[1].scan_count == 1, "scan count not incremented");
    PASS();
}

static void test_cpu_utilization(void)
{
    TEST("cpu_utilization");
    pax_controller_t ctrl;
    pax_controller_init(&ctrl, "CTRL_03", PAX_CTRL_CONTROLLOGIX_L8);
    pax_controller_add_task(&ctrl, "T1", PAX_TASK_PERIODIC,
                             PAX_PRIORITY_MEDIUM, 100.0);
    ctrl.tasks[1].measured_scan_ms = 15.0;
    double util = pax_controller_cpu_utilization(&ctrl);
    /* 15/100 = 0.15 = 15% plus continuous task fraction */
    CHECK(util > 10.0 && util < 30.0, "CPU util unexpected");
    PASS();
}

static void test_schedulability(void)
{
    TEST("schedulability");
    pax_controller_t ctrl;
    pax_controller_init(&ctrl, "CTRL_04", PAX_CTRL_CONTROLLOGIX_L8);
    pax_controller_add_task(&ctrl, "T1", PAX_TASK_PERIODIC,
                             PAX_PRIORITY_MEDIUM, 100.0);
    pax_controller_add_task(&ctrl, "T2", PAX_TASK_PERIODIC,
                             PAX_PRIORITY_LOW, 200.0);
    ctrl.tasks[1].measured_scan_ms = 10.0;
    ctrl.tasks[2].measured_scan_ms = 20.0;
    /* U = 10/100 + 20/200 = 0.1 + 0.1 = 0.2 */
    int rc = pax_controller_check_schedulability(&ctrl);
    CHECK_EQ(rc, 0, "should be schedulable");
    PASS();
}

/* ---------------------------------------------------------------------------
 * I/O Subsystem Tests (L1-L5)
 * ------------------------------------------------------------------------- */

static void test_ai_scaling(void)
{
    TEST("ai_scaling");
    pax_ai_channel_t ch;
    pax_ai_channel_init(&ch, 0, "TIC101", 0.0, 100.0, "degC");
    double eu = pax_ai_scale_raw_to_eu(&ch, 12.0);  /* 12 mA = 50% */
    CHECK_DOUBLE_EQ(eu, 50.0, 0.01, "4-20mA linear scaling wrong");
    double eu_4ma = pax_ai_scale_raw_to_eu(&ch, 4.0);
    CHECK_DOUBLE_EQ(eu_4ma, 0.0, 0.01, "4mA should be 0%");
    PASS();
}

static void test_namur_classification(void)
{
    TEST("namur_classification");
    CHECK_EQ(pax_ai_classify_namur(12.0), PAX_NAMUR_NORMAL, "12mA not NORMAL");
    CHECK_EQ(pax_ai_classify_namur(2.0), PAX_NAMUR_UNDER_RANGE, "2mA not UNDERRANGE");
    CHECK_EQ(pax_ai_classify_namur(22.0), PAX_NAMUR_OVER_RANGE, "22mA not OVERRANGE");
    CHECK_EQ(pax_ai_classify_namur(3.7), PAX_NAMUR_SENSOR_FAULT_LOW,
             "3.7mA not SENSOR_FAULT_LOW");
    PASS();
}

static void test_alarm_checking(void)
{
    TEST("ai_alarm_checking");
    pax_ai_channel_t ch;
    pax_ai_channel_init(&ch, 0, "PIC101", 0.0, 10.0, "bar");
    ch.alarm_hi = 8.0;
    ch.alarm_hihi = 9.5;
    ch.alarm_lo = 1.0;
    ch.alarm_lolo = 0.5;
    CHECK_EQ(pax_ai_check_alarms(&ch, 5.0), 0, "should be no alarm");
    CHECK_EQ(pax_ai_check_alarms(&ch, 9.6), 4, "should be HIHI");
    CHECK_EQ(pax_ai_check_alarms(&ch, 0.4), 2, "should be LOLO");
    PASS();
}

static void test_filter(void)
{
    TEST("ai_filter");
    /* tau=1.0s, Ts=0.1s, alpha=0.1/1.1=0.0909 */
    double filtered = pax_ai_apply_filter(50.0, 60.0, 1.0, 0.1);
    CHECK(filtered > 50.0 && filtered < 51.5, "filter should reduce step");
    /* No filter: tau=0 -> passthrough */
    double no_filter = pax_ai_apply_filter(50.0, 60.0, 0.0, 0.1);
    CHECK_DOUBLE_EQ(no_filter, 60.0, 0.001, "zero tau should pass through");
    PASS();
}

static void test_rtd_conversion(void)
{
    TEST("rtd_conversion");
    /* PT100 at 0C: 100 ohm */
    double t0 = pax_rtd_resistance_to_temp(100.0, PAX_ANALOG_RTD_PT100);
    CHECK_DOUBLE_EQ(t0, 0.0, 0.1, "PT100 at 100ohm should be 0C");
    /* PT100 at ~100C: 138.51 ohm */
    double t100 = pax_rtd_resistance_to_temp(138.51, PAX_ANALOG_RTD_PT100);
    CHECK(t100 > 95.0 && t100 < 105.0, "PT100 at ~139ohm should be ~100C");
    PASS();
}

/* ---------------------------------------------------------------------------
 * Alarm Manager Tests (L1-L5)
 * ------------------------------------------------------------------------- */

static void test_alarm_init(void)
{
    TEST("alarm_init");
    pax_alarm_t alarm;
    pax_alarm_init(&alarm, 1, "PAH101", "High Pressure Reactor",
                    PAX_ALARM_TYPE_PROCESS, PAX_ALARM_PRIORITY_HIGH,
                    10.0, 0.5, 2.0, 1.0);
    CHECK_EQ(alarm.alarm_id, 1, "alarm_id mismatch");
    CHECK_EQ(alarm.priority, PAX_ALARM_PRIORITY_HIGH, "priority mismatch");
    CHECK_EQ(alarm.state, PAX_ALARM_STATE_NORMAL, "initial state not NORMAL");
    PASS();
}

static void test_alarm_state_machine(void)
{
    TEST("alarm_state_machine");
    pax_alarm_t alarm;
    pax_alarm_init(&alarm, 1, "LAH101", "High Level",
                    PAX_ALARM_TYPE_PROCESS, PAX_ALARM_PRIORITY_MEDIUM,
                    80.0, 2.0, 0.0, 0.0);

    /* Normal ? Active (PV exceeds setpoint + deadband/2, delay elapsed) */
    pax_alarm_state_t state = pax_alarm_evaluate(&alarm, 85.0, 100.0, 0.0);
    CHECK_EQ(state, PAX_ALARM_STATE_ACTIVE, "should transition to ACTIVE");

    /* Acknowledge */
    int rc = pax_alarm_acknowledge(&alarm);
    CHECK_EQ(rc, 0, "acknowledge should succeed");
    CHECK_EQ(alarm.state, PAX_ALARM_STATE_ACKNOWLEDGED, "should be ACKNOWLEDGED");

    /* Clear condition */
    state = pax_alarm_evaluate(&alarm, 78.0, 0.0, 100.0);
    CHECK_EQ(state, PAX_ALARM_STATE_NORMAL, "should return to NORMAL");
    PASS();
}

static void test_alarm_chatter(void)
{
    TEST("alarm_chatter");
    pax_alarm_t alarm;
    pax_alarm_init(&alarm, 2, "TAL101", "Low Temperature",
                    PAX_ALARM_TYPE_PROCESS, PAX_ALARM_PRIORITY_LOW,
                    20.0, 1.0, 0.0, 0.0);
    /* Simulate many activations in 1 hour */
    alarm.chatter_count_1h = 10;
    bool is_chatter = pax_alarm_detect_chatter(&alarm, 1.0);
    CHECK(is_chatter == true, "should detect chatter");
    CHECK(alarm.is_nuisance == true, "should flag as nuisance");
    PASS();
}

static void test_alarm_rationalization(void)
{
    TEST("alarm_rationalization");
    pax_alarm_t alarm;
    pax_alarm_init(&alarm, 3, "PALL101", "Low-Low Pressure",
                    PAX_ALARM_TYPE_SAFETY, PAX_ALARM_PRIORITY_CRITICAL,
                    0.5, 0.1, 0.0, 0.0);
    /* Missing consequence + operator action */
    int issues = pax_alarm_rationalize(&alarm);
    CHECK(issues != 0, "should have issues (no consequence/action)");

    /* Fill in required fields */
    strncpy(alarm.consequence, "Reactor overpressure leading to rupture",
            sizeof(alarm.consequence) - 1);
    strncpy(alarm.operator_action, "Reduce feed flow and open vent valve XV-102",
            sizeof(alarm.operator_action) - 1);
    alarm.time_to_respond_s = 60.0;
    issues = pax_alarm_rationalize(&alarm);
    CHECK_EQ(issues, 0, "should pass rationalization");
    PASS();
}

/* ---------------------------------------------------------------------------
 * Historian Tests (L1-L5)
 * ------------------------------------------------------------------------- */

static void test_hist_point_init(void)
{
    TEST("hist_point_init");
    pax_hist_point_t point;
    pax_hist_point_init(&point, 1, "FIC101.SP", "Flow Controller Setpoint",
                          PAX_HIST_TYPE_FLOAT64, "m3/h", 0.0, 500.0);
    CHECK_EQ(point.point_id, 1, "point_id mismatch");
    CHECK_DOUBLE_EQ(point.compression_deviation, 0.1, 0.001,
                    "default deviation wrong");
    PASS();
}

static void test_hist_add_sample(void)
{
    TEST("hist_add_sample");
    pax_hist_point_t point;
    pax_hist_point_init(&point, 1, "TAG1", "Test", PAX_HIST_TYPE_FLOAT64,
                          "EU", 0.0, 100.0);
    point.scan_period_ms = 1000;

    /* First sample always stored */
    int stored = pax_hist_add_sample(&point, 50.0, 1000, PAX_IO_QUALITY_GOOD);
    CHECK_EQ(stored, 1, "first sample should be stored");
    CHECK_EQ(point.archived_samples, 1, "archived count wrong");

    /* Same value, no exception -> not stored */
    stored = pax_hist_add_sample(&point, 50.0, 2000, PAX_IO_QUALITY_GOOD);
    CHECK_EQ(stored, 0, "unchanged value should not be stored");

    /* Significant change -> stored */
    stored = pax_hist_add_sample(&point, 55.0, 3000, PAX_IO_QUALITY_GOOD);
    CHECK_EQ(stored, 1, "changed value should be stored");
    PASS();
}

static void test_swinging_door_compression(void)
{
    TEST("swinging_door_compression");
    pax_hist_sample_t samples[10];
    uint32_t i;
    /* Generate a linear ramp: 0, 1, 2, ..., 9 */
    for (i = 0; i < 10; i++) {
        samples[i].timestamp_ms = (uint64_t)i * 1000;
        samples[i].value = (double)i;
        samples[i].quality = PAX_IO_QUALITY_GOOD;
        samples[i].is_compressed = false;
    }
    uint32_t count = pax_hist_compress_swinging_door(samples, 10, 2.0);
    /* With deviation 2.0 on linear ramp (slope=1), compression occurs */
    CHECK(count <= 10, "compressed count should not exceed original");
    CHECK(count >= 2, "should keep at least endpoints");
    PASS();
}

static void test_deadband_compression(void)
{
    TEST("deadband_compression");
    pax_hist_sample_t samples[5];
    samples[0].timestamp_ms = 0;   samples[0].value = 0.0;
    samples[1].timestamp_ms = 100; samples[1].value = 0.01;
    samples[2].timestamp_ms = 200; samples[2].value = 0.02;
    samples[3].timestamp_ms = 300; samples[3].value = 5.0;
    samples[4].timestamp_ms = 400; samples[4].value = 5.01;
    uint32_t count = pax_hist_compress_deadband(samples, 5, 1.0);
    /* Should keep 0.0, 5.0, 5.01 = 3 samples */
    CHECK_EQ(count, 3, "deadband compression should keep 3 samples");
    PASS();
}

static void test_hist_summary(void)
{
    TEST("hist_summary");
    pax_hist_sample_t samples[4];
    samples[0].timestamp_ms = 0;    samples[0].value = 10.0;
    samples[0].quality = PAX_IO_QUALITY_GOOD;
    samples[1].timestamp_ms = 1000; samples[1].value = 20.0;
    samples[1].quality = PAX_IO_QUALITY_GOOD;
    samples[2].timestamp_ms = 2000; samples[2].value = 30.0;
    samples[2].quality = PAX_IO_QUALITY_GOOD;
    samples[3].timestamp_ms = 3000; samples[3].value = 15.0;
    samples[3].quality = PAX_IO_QUALITY_BAD;

    pax_hist_summary_t summary;
    int rc = pax_hist_compute_summary(samples, 4, 0, 3000, &summary);
    CHECK_EQ(rc, 0, "summary computation failed");
    CHECK(fabs(summary.average - 18.75) < 0.1, "average wrong");
    CHECK_DOUBLE_EQ(summary.minimum, 10.0, 0.01, "min wrong");
    CHECK_DOUBLE_EQ(summary.maximum, 30.0, 0.01, "max wrong");
    CHECK(fabs(summary.percent_good - 75.0) < 0.1, "percent good wrong");
    PASS();
}

static void test_ewma(void)
{
    TEST("ewma");
    /* alpha=0.5: EWMA = 0.5*100 + 0.5*50 = 75 */
    double ewma = pax_hist_ewma(100.0, 50.0, 0.5);
    CHECK_DOUBLE_EQ(ewma, 75.0, 0.001, "EWMA alpha=0.5 wrong");
    /* alpha=1.0: EWMA = current value */
    ewma = pax_hist_ewma(100.0, 50.0, 1.0);
    CHECK_DOUBLE_EQ(ewma, 100.0, 0.001, "EWMA alpha=1.0 wrong");
    /* alpha=0.0: EWMA = previous value (no update) */
    ewma = pax_hist_ewma(100.0, 50.0, 0.0);
    CHECK_DOUBLE_EQ(ewma, 50.0, 0.001, "EWMA alpha=0.0 wrong");
    PASS();
}

/* ---------------------------------------------------------------------------
 * Control Loop Tests (L1-L6)
 * ------------------------------------------------------------------------- */

static void test_pid_loop_init(void)
{
    TEST("pid_loop_init");
    pax_pid_loop_t loop;
    pax_pid_loop_init(&loop, 1, "FIC101", PAX_LOOP_TYPE_SIMPLE_PID,
                       PAX_ACTION_REVERSE, 0.0, 100.0);
    CHECK_EQ(loop.mode, PAX_LOOP_MODE_MANUAL, "initial mode should be MANUAL");
    CHECK_EQ(loop.action, PAX_ACTION_REVERSE, "action mismatch");
    CHECK_DOUBLE_EQ(loop.output_min, 0.0, 0.001, "min output wrong");
    CHECK_DOUBLE_EQ(loop.output_max, 100.0, 0.001, "max output wrong");
    PASS();
}

static void test_pid_execute_manual(void)
{
    TEST("pid_execute_manual");
    pax_pid_loop_t loop;
    pax_pid_loop_init(&loop, 1, "TIC101", PAX_LOOP_TYPE_SIMPLE_PID,
                       PAX_ACTION_REVERSE, 0.0, 100.0);
    loop.output = 50.0;
    double co = pax_pid_execute(&loop, 60.0, 70.0, 0.1);
    CHECK_DOUBLE_EQ(co, 50.0, 0.001, "manual mode should not change output");
    PASS();
}

static void test_pid_execute_reverse_action(void)
{
    TEST("pid_execute_reverse_action");
    pax_pid_loop_t loop;
    pax_pid_loop_init(&loop, 1, "TIC102", PAX_LOOP_TYPE_SIMPLE_PID,
                       PAX_ACTION_REVERSE, 0.0, 100.0);
    loop.mode = PAX_LOOP_MODE_AUTO;

    /* PV below SP: should increase output (heating) */
    loop.output = 30.0;
    loop.prev_pv = 60.0;
    double co = pax_pid_execute(&loop, 60.0, 70.0, 0.1);
    /* With Kc=1.0, Ti=60, Td=0, expect positive delta */
    CHECK(co > 30.0, "reverse action should increase output when PV < SP");

    /* PV above SP: should decrease output (cooling response) */
    co = pax_pid_execute(&loop, 80.0, 70.0, 0.1);
    CHECK(co < 100.0, "reverse action should decrease output when PV > SP");
    PASS();
}

static void test_pid_output_clamping(void)
{
    TEST("pid_output_clamping");
    pax_pid_loop_t loop;
    pax_pid_loop_init(&loop, 1, "FIC103", PAX_LOOP_TYPE_SIMPLE_PID,
                       PAX_ACTION_REVERSE, 0.0, 100.0);
    /* Use aggressive settings: high Kc, fast Ti to push output past limits */
    pax_pid_params_init(&loop.params, PAX_PID_FORM_VELOCITY,
                         10.0, 0.5, 0.0, 0.1);
    loop.mode = PAX_LOOP_MODE_AUTO;
    loop.output = 99.0;
    loop.prev_pv = 50.0;
    loop.prev_error = 0.0;

    double co = pax_pid_execute(&loop, 50.0, 100.0, 0.1);
    /* Output should be clamped to max 100.0 */
    CHECK(co <= 100.0, "output should not exceed max");
    CHECK(loop.output_clamped_high, "clamped_high flag should be set");
    PASS();
}

static void test_bumpless_transfer(void)
{
    TEST("bumpless_transfer");
    pax_pid_loop_t loop;
    pax_pid_loop_init(&loop, 1, "FIC104", PAX_LOOP_TYPE_SIMPLE_PID,
                       PAX_ACTION_REVERSE, 0.0, 100.0);
    loop.mode = PAX_LOOP_MODE_MANUAL;
    loop.output = 45.0;

    /* Switch to auto ? bumpless should keep output at 45 */
    loop.mode = PAX_LOOP_MODE_AUTO;
    pax_pid_bumpless_transfer(&loop);
    CHECK_DOUBLE_EQ(loop.output, 45.0, 0.001, "bumpless transfer failed");
    PASS();
}

static void test_ziegler_nichols_tuning(void)
{
    TEST("ziegler_nichols_tuning");
    pax_pid_params_t params;
    /* Process: K=2, T=10s, L=2s */
    pax_pid_ziegler_nichols_open_loop(2.0, 10.0, 2.0, &params);
    /* Kc = 1.2 * 10 / (2 * 2) = 3.0 */
    CHECK_DOUBLE_EQ(params.kc, 3.0, 0.01, "Z-N Kc wrong");
    /* Ti = 2.0 * 2 = 4.0 */
    CHECK_DOUBLE_EQ(params.ti_sec, 4.0, 0.01, "Z-N Ti wrong");
    /* Td = 0.5 * 2 = 1.0 */
    CHECK_DOUBLE_EQ(params.td_sec, 1.0, 0.01, "Z-N Td wrong");
    PASS();
}

static void test_feedforward(void)
{
    TEST("feedforward");
    double prev_input = 0.0, prev_output = 0.0;
    double ff = pax_feedforward_compute(10.0, 0.5,
                                         PAX_FF_STATIC_GAIN,
                                         0.0, 0.0, 0.1,
                                         &prev_input, &prev_output);
    CHECK_DOUBLE_EQ(ff, 5.0, 0.001, "static feedforward wrong");
    PASS();
}

/* ---------------------------------------------------------------------------
 * Main Test Runner
 * ------------------------------------------------------------------------- */

int main(void)
{
    printf("\n");
    printf("===========================================\n");
    printf("  PlantPAx DCS Module - Test Suite\n");
    printf("===========================================\n\n");

    /* Architecture */
    printf("[Architecture Tests]\n");
    test_system_config_init();
    test_entity_registration();
    test_procedural_state_names();
    test_system_availability();
    test_network_load();

    /* Controller */
    printf("\n[Controller Tests]\n");
    test_ctrl_spec_init();
    test_controller_add_task();
    test_controller_scan();
    test_cpu_utilization();
    test_schedulability();

    /* I/O Subsystem */
    printf("\n[I/O Subsystem Tests]\n");
    test_ai_scaling();
    test_namur_classification();
    test_alarm_checking();
    test_filter();
    test_rtd_conversion();

    /* Alarm Manager */
    printf("\n[Alarm Manager Tests]\n");
    test_alarm_init();
    test_alarm_state_machine();
    test_alarm_chatter();
    test_alarm_rationalization();

    /* Historian */
    printf("\n[Historian Tests]\n");
    test_hist_point_init();
    test_hist_add_sample();
    test_swinging_door_compression();
    test_deadband_compression();
    test_hist_summary();
    test_ewma();

    /* Control Loop */
    printf("\n[Control Loop Tests]\n");
    test_pid_loop_init();
    test_pid_execute_manual();
    test_pid_execute_reverse_action();
    test_pid_output_clamping();
    test_bumpless_transfer();
    test_ziegler_nichols_tuning();
    test_feedforward();

    printf("\n===========================================\n");
    printf("  Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("===========================================\n");

    if (tests_passed == tests_run) {
        printf("  ALL TESTS PASSED\n");
        return 0;
    } else {
        printf("  %d TEST(S) FAILED\n", tests_run - tests_passed);
        return 1;
    }
}
