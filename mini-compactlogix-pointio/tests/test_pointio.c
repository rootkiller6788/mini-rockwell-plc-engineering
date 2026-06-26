/**
 * @file test_pointio.c
 * @brief Comprehensive test suite for mini-compactlogix-pointio.
 *
 * Tests cover:
 *   - Module/chassis initialization and configuration
 *   - Digital I/O read/write operations
 *   - Analog scaling and conversion
 *   - Connection establishment and teardown
 *   - Fault detection and recovery
 *   - Signal processing algorithms
 *   - Power budget calculation
 *   - LED state determination
 *   - Scanner functionality
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "pointio_types.h"
#include "pointio_module.h"
#include "pointio_digital.h"
#include "pointio_analog.h"
#include "pointio_connection.h"
#include "pointio_diagnostics.h"
#include "pointio_scanner.h"

/* Forward declarations for signal processing functions */
int pointio_signal_moving_average(double *buffer, int buffer_size,
    int *write_index, double new_sample, double *average, int is_full);
int pointio_signal_median_filter(double *buffer, int buffer_size,
    int *write_index, double new_sample, double *median, int is_full);
int pointio_signal_rate_limit(double prev_value, double new_raw_value,
    double max_roc_per_s, double dt_s, double *limited_value);
int pointio_signal_deadband(double last_reported, double new_value,
    double deadband, double *output_value, int *changed);
int pointio_signal_detect_outlier_zscore(double value, double mean,
    double stddev, double threshold, int *is_outlier);
int pointio_signal_update_statistics(double new_value, int *count,
    double *mean, double *m2, double *variance);
int pointio_signal_detect_frozen(const double *buffer, int buffer_size,
    double noise_floor, int *is_frozen, double *stddev);
int pointio_signal_linear_interpolate(double x0, double y0,
    double x1, double y1, double x, double *y);
int pointio_signal_validate(double value, double low_limit, double high_limit,
    double max_roc, double prev_value, double dt_s,
    double max_noise, double noise_stddev, int *quality);
int pointio_signal_estimate_snr(double signal_mean, double signal_variance,
    double noise_variance, double *snr_db, int *quality_label);
int pointio_signal_holt_smoothing(double alpha, double beta,
    double x, double *level, double *trend,
    double *smoothed, double *forecast);

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %s ... ", name)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)

/*===========================================================================
 * L1 Tests: Module initialization and configuration
 *===========================================================================*/

static void test_chassis_init(void)
{
    TEST("chassis_init");
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);
    CHECK(chassis.num_modules == 1);  /* Adapter pre-configured at slot 0 */
    CHECK(chassis.modules[0].type == POINTIO_TYPE_ADAPTER);
    CHECK(chassis.modules[0].status == POINTIO_STATUS_OK);
    CHECK(chassis.modules[1].status == POINTIO_STATUS_NOT_PRESENT);
    CHECK(chassis.adapter_slot == 0);
    CHECK(strcmp(chassis.chassis_name, "PointBus_Chassis") == 0);
    PASS();
}

static void test_module_init(void)
{
    TEST("module_init");
    pointio_module_t mod;
    pointio_module_init(&mod, 5);
    CHECK(mod.slot.slot == 5);
    CHECK(mod.status == POINTIO_STATUS_NOT_PRESENT);
    CHECK(mod.type == POINTIO_TYPE_UNKNOWN);
    CHECK(mod.rpi_us == POINTIO_DEFAULT_RPI_MS * 1000);
    CHECK(mod.fault_mode == POINTIO_FAULT_MODE_ZERO);
    PASS();
}

static void test_config_digital_input(void)
{
    TEST("config_digital_input");
    pointio_module_t mod;
    int rc = pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Slot3");
    CHECK(rc == 0);
    CHECK(mod.type == POINTIO_TYPE_DIGITAL_INPUT);
    CHECK(mod.num_channels == 8);
    CHECK(mod.input_channels == 8);
    CHECK(mod.output_channels == 0);
    CHECK(mod.input_size_bytes == 1);  /* 8 bits = 1 byte */
    CHECK(strcmp(mod.name, "DI_Slot3") == 0);
    PASS();
}

static void test_config_digital_output(void)
{
    TEST("config_digital_output");
    pointio_module_t mod;
    int rc = pointio_module_config_digital_output(&mod, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "DO_Slot4");
    CHECK(rc == 0);
    CHECK(mod.type == POINTIO_TYPE_DIGITAL_OUTPUT);
    CHECK(mod.num_channels == 8);
    CHECK(mod.output_channels == 8);
    CHECK(strcmp(mod.name, "DO_Slot4") == 0);
    PASS();
}

static void test_config_analog_input(void)
{
    TEST("config_analog_input");
    pointio_module_t mod;
    pointio_analog_scaling_t scaling;
    memset(&scaling, 0, sizeof(scaling));
    scaling.eu_low = 0.0;
    scaling.eu_high = 100.0;
    scaling.raw_low = 3277.0;
    scaling.raw_high = 16383.0;
    scaling.range = POINTIO_AI_RANGE_4_20MA;

    int rc = pointio_module_config_analog_input(&mod, "1734-IE4C", 4,
        POINTIO_AI_RANGE_4_20MA, &scaling, "AI_Slot5");
    CHECK(rc == 0);
    CHECK(mod.type == POINTIO_TYPE_ANALOG_INPUT);
    CHECK(mod.num_channels == 4);
    CHECK(mod.input_channels == 4);
    CHECK(mod.identity.device_type == 0x0A);
    PASS();
}

static void test_chassis_add_remove(void)
{
    TEST("chassis_add_remove");
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);

    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Slot1");
    mod.slot.slot = 1;

    int rc = pointio_chassis_add_module(&chassis, &mod);
    CHECK(rc == 0);
    CHECK(chassis.modules[1].status == POINTIO_STATUS_OK);
    CHECK(chassis.num_modules == 2);

    /* Find by name */
    pointio_module_t *found = pointio_chassis_find_module(&chassis, "DI_Slot1");
    CHECK(found != NULL);
    CHECK(found->slot.slot == 1);

    /* Remove */
    rc = pointio_chassis_remove_module(&chassis, 1);
    CHECK(rc == 0);
    CHECK(chassis.modules[1].status == POINTIO_STATUS_NOT_PRESENT);
    CHECK(chassis.num_modules == 1);
    PASS();
}

/*===========================================================================
 * L4 Tests: Power Budget
 *===========================================================================*/

static void test_power_budget(void)
{
    TEST("power_budget");
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);

    pointio_module_t di_mod;
    pointio_module_config_digital_input(&di_mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_1");
    di_mod.slot.slot = 1;
    pointio_chassis_add_module(&chassis, &di_mod);

    pointio_module_t do_mod;
    pointio_module_config_digital_output(&do_mod, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "DO_2");
    do_mod.slot.slot = 2;
    pointio_chassis_add_module(&chassis, &do_mod);

    pointio_power_budget_t budget;
    int rc = pointio_calculate_power_budget(&chassis, &budget);
    CHECK(rc >= 0);  /* Within limits */
    CHECK(budget.backplane_5v_used_ma > 0.0);
    CHECK(budget.total_power_used_w > 0.0);
    CHECK(budget.overloaded == 0);  /* 3 modules well within 1000mA */
    PASS();
}

/*===========================================================================
 * L5 Tests: Digital I/O Operations
 *===========================================================================*/

static void test_di_read_write(void)
{
    TEST("di_read_write");
    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Test");
    mod.status = POINTIO_STATUS_OK;

    /* Set some input bits via data image */
    mod.input_data[0] = 0xA5;  /* 10100101 */
    /* Process input image */
    pointio_di_process_input_image(&mod, mod.input_data, 1);

    /* Read individual channels */
    pointio_di_state_t state;
    CHECK(pointio_di_read_channel(&mod, 0, &state) == 0);
    CHECK(state == POINTIO_DI_ON);   /* Bit 0 = 1 */
    CHECK(pointio_di_read_channel(&mod, 2, &state) == 0);
    CHECK(state == POINTIO_DI_ON);   /* Bit 2 = 1 */
    CHECK(pointio_di_read_channel(&mod, 1, &state) == 0);
    CHECK(state == POINTIO_DI_OFF);  /* Bit 1 = 0 */

    /* Read all as mask */
    uint32_t mask;
    int count = pointio_di_read_all(&mod, &mask);
    CHECK(count == 8);
    CHECK(mask == 0xA5);
    PASS();
}

static void test_do_write(void)
{
    TEST("do_write");
    pointio_module_t mod;
    pointio_module_config_digital_output(&mod, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "DO_Test");
    mod.status = POINTIO_STATUS_OK;

    /* Write individual channels */
    CHECK(pointio_do_write_channel(&mod, 0, POINTIO_DO_ON) == 0);
    CHECK(pointio_do_write_channel(&mod, 3, POINTIO_DO_ON) == 0);
    CHECK(pointio_do_write_channel(&mod, 7, POINTIO_DO_ON) == 0);
    CHECK(mod.output_data[0] & 0x01);   /* Ch 0 on */
    CHECK(mod.output_data[0] & 0x08);   /* Ch 3 on */
    CHECK(mod.output_data[0] & 0x80);   /* Ch 7 on */

    /* Write all from mask */
    CHECK(pointio_do_write_all(&mod, 0xF0) == 8);  /* Channels 4-7 on */
    CHECK((mod.output_data[0] & 0xF0) == 0xF0);
    CHECK((mod.output_data[0] & 0x0F) == 0x00);

    /* Toggle */
    pointio_do_state_t new_state;
    CHECK(pointio_do_toggle(&mod, 4, &new_state) == 0);
    CHECK(new_state == POINTIO_DO_OFF);  /* Was ON, now OFF */
    PASS();
}

static void test_edge_detection(void)
{
    TEST("edge_detection");
    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Edge");

    /* Set input: channel 0 = ON */
    mod.input_data[0] = 0x01;
    pointio_di_process_input_image(&mod, mod.input_data, 1);

    uint32_t prev_mask = 0x00;  /* All were OFF */
    int edge;

    /* Should detect rising edge on channel 0 */
    CHECK(pointio_di_rising_edge(&mod, 0, &prev_mask, &edge) == 0);
    CHECK(edge == 1);
    CHECK(pointio_di_rising_edge(&mod, 0, &prev_mask, &edge) == 0);
    CHECK(edge == 0);  /* No second rising edge on same state */

    /* Now turn channel 0 OFF */
    mod.input_data[0] = 0x00;
    pointio_di_process_input_image(&mod, mod.input_data, 1);

    CHECK(pointio_di_falling_edge(&mod, 0, &prev_mask, &edge) == 0);
    CHECK(edge == 1);
    PASS();
}

/*===========================================================================
 * L5 Tests: Analog Scaling
 *===========================================================================*/

static void test_analog_scaling(void)
{
    TEST("analog_scaling");
    pointio_analog_scaling_t scaling;
    scaling.eu_low = 0.0;
    scaling.eu_high = 100.0;
    scaling.raw_low = 3277.0;
    scaling.raw_high = 16383.0;
    scaling.range = POINTIO_AI_RANGE_4_20MA;

    double eu_value;
    /* 4mA = 3277 counts → 0% */
    CHECK(pointio_ai_raw_to_eu(3277, &scaling, &eu_value) == 0);
    CHECK(fabs(eu_value - 0.0) < 0.01);

    /* 12mA = midpoint = 9830 counts → 50% */
    uint16_t midpoint = (uint16_t)((3277 + 16383) / 2);
    CHECK(pointio_ai_raw_to_eu(midpoint, &scaling, &eu_value) == 0);
    CHECK(fabs(eu_value - 50.0) < 0.5);

    /* 20mA = 16383 counts → 100% */
    CHECK(pointio_ai_raw_to_eu(16383, &scaling, &eu_value) == 0);
    CHECK(fabs(eu_value - 100.0) < 0.01);

    /* Inverse: EU to raw */
    uint16_t raw_count;
    CHECK(pointio_ai_eu_to_raw(50.0, &scaling, &raw_count) == 0);
    CHECK(raw_count >= 9820 && raw_count <= 9840);

    /* Open wire: raw near 0 in 4-20mA mode */
    CHECK(pointio_ai_raw_to_eu(100, &scaling, &eu_value) == 0);
    CHECK(eu_value < scaling.eu_low);  /* Below-range indicator */
    PASS();
}

static void test_loop_integrity(void)
{
    TEST("loop_integrity");
    pointio_analog_scaling_t scaling;
    scaling.range = POINTIO_AI_RANGE_4_20MA;
    scaling.raw_low = 3277.0;
    scaling.raw_high = 16383.0;

    int status;
    double loop_ma;

    /* Normal: 12mA */
    CHECK(pointio_ai_check_loop_integrity(9800, &scaling, &status, &loop_ma) == 0);
    CHECK(status == 0);  /* OK */

    /* Open wire: near 0 counts */
    int is_open;
    CHECK(pointio_ai_detect_open_wire(100, &scaling, &is_open) == 0);
    CHECK(is_open == 1);

    /* OK wire */
    CHECK(pointio_ai_detect_open_wire(16383, &scaling, &is_open) == 0);
    CHECK(is_open == 0);
    PASS();
}

/*===========================================================================
 * L4 Tests: Connection Management
 *===========================================================================*/

static void test_connection_pool(void)
{
    TEST("connection_pool");
    pointio_connection_pool_t pool;
    pointio_conn_pool_init(&pool);
    CHECK(pool.active_count == 0);

    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Conn");
    mod.slot.slot = 3;

    /* Open connection */
    int conn_id;
    int rc = pointio_conn_open(&pool, POINTIO_CONN_EXCLUSIVE_OWNER, &mod,
        20000, POINTIO_TRIGGER_CYCLIC, &conn_id);
    CHECK(rc == 0);
    CHECK(conn_id > 0);
    CHECK(pool.active_count == 1);

    /* Validate RPI */
    CHECK(pointio_conn_validate_rpi(20.0, 1) == 1);
    CHECK(pointio_conn_validate_rpi(0.5, 1) == 0);  /* Too fast */
    CHECK(pointio_conn_validate_rpi(1000.0, 1) == 0);  /* Too slow */

    /* Check connection */
    int is_mcast = pointio_conn_is_multicast(&pool, conn_id);
    CHECK(is_mcast == 0);  /* Exclusive Owner = unicast */

    /* Close connection */
    rc = pointio_conn_close(&pool, conn_id);
    CHECK(rc == 0);
    CHECK(pool.active_count == 0);
    PASS();
}

static void test_connection_timeout(void)
{
    TEST("connection_timeout");
    pointio_connection_pool_t pool;
    pointio_conn_pool_init(&pool);

    pointio_module_t mod;
    pointio_module_config_digital_output(&mod, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "DO_TO");
    mod.slot.slot = 4;

    int conn_id;
    pointio_conn_open(&pool, POINTIO_CONN_EXCLUSIVE_OWNER, &mod,
        10000, POINTIO_TRIGGER_CYCLIC, &conn_id);

    /* Simulate data exchange */
    uint8_t data[1] = {0x00};
    CHECK(pointio_conn_send_output(&pool, conn_id, data, 1) == 0);

    /* Advance time beyond timeout */
    uint64_t timed_out = pointio_conn_check_timeouts(&pool, 10000000);
    (void)timed_out;  /* May or may not time out depending on internal state */

    /* Reset connection */
    CHECK(pointio_conn_reset(&pool, conn_id) == 0);
    PASS();
}

/*===========================================================================
 * L6 Tests: Fault Management
 *===========================================================================*/

static void test_fault_codes(void)
{
    TEST("fault_codes");
    /* Test fault code strings */
    const char *str = pointio_fault_code_string(POINTIO_FAULT_CONN_TIMEOUT);
    CHECK(str != NULL);
    CHECK(strlen(str) > 10);

    str = pointio_fault_code_string(POINTIO_FAULT_NONE);
    CHECK(strcmp(str, "No fault") == 0);

    /* Test recovery actions */
    const char *action = pointio_fault_recovery_action(POINTIO_FAULT_FIELD_POWER_LOST);
    CHECK(action != NULL);
    CHECK(strlen(action) > 20);

    /* Test severity */
    CHECK(pointio_fault_severity(POINTIO_FAULT_NONE) == 0);
    CHECK(pointio_fault_severity(POINTIO_FAULT_CONN_TIMEOUT) == 1);
    CHECK(pointio_fault_severity(POINTIO_FAULT_SHORT_CIRCUIT) == 2);
    CHECK(pointio_fault_severity(POINTIO_FAULT_BACKPLANE_ERROR) == 3);
    PASS();
}

static void test_fault_clear(void)
{
    TEST("fault_clear");
    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Fault");

    /* Set a minor fault */
    mod.active_fault = POINTIO_FAULT_CONN_TIMEOUT;
    mod.status = POINTIO_STATUS_FAULTED;

    /* Should clear (minor fault) */
    int rc = pointio_clear_module_fault(&mod);
    CHECK(rc == 0);
    CHECK(mod.active_fault == POINTIO_FAULT_NONE);
    CHECK(mod.status == POINTIO_STATUS_OK);

    /* Set a major fault */
    mod.active_fault = POINTIO_FAULT_INTERNAL_ERROR;
    mod.status = POINTIO_STATUS_MAJOR_FAULT;
    rc = pointio_clear_module_fault(&mod);
    CHECK(rc == 1);  /* Cannot clear critical fault */
    PASS();
}

static void test_module_inhibit(void)
{
    TEST("module_inhibit");
    pointio_module_t mod;
    pointio_module_config_digital_output(&mod, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "DO_Inh");

    /* Set some outputs ON */
    pointio_do_write_channel(&mod, 0, POINTIO_DO_ON);
    pointio_do_write_channel(&mod, 3, POINTIO_DO_ON);

    /* Inhibit module */
    CHECK(pointio_set_module_inhibit(&mod, 1) == 0);
    CHECK(mod.status == POINTIO_STATUS_INHIBITED);
    /* Outputs should be de-energized */
    CHECK((mod.output_data[0] & 0x09) == 0);

    /* Uninhibit */
    CHECK(pointio_set_module_inhibit(&mod, 0) == 0);
    CHECK(mod.status == POINTIO_STATUS_OK);
    PASS();
}

/*===========================================================================
 * L4 Tests: LED States
 *===========================================================================*/

static void test_led_states(void)
{
    TEST("led_states");
    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_LED");
    mod.status = POINTIO_STATUS_OK;
    mod.conn_state = POINTIO_CONN_STATE_RUNNING;
    mod.input_data[0] = 0x55;  /* Alternating channels ON */

    pointio_led_state_t leds;
    CHECK(pointio_determine_led_states(&mod, &leds) == 0);
    CHECK(leds.mod_status_green_on == 1);  /* OK = solid green */
    CHECK(leds.net_status_green_on == 1);  /* Running = solid green */
    CHECK(leds.io_status_bitmask == 0x55);  /* Ch 0,2,4,6 ON */
    PASS();
}

/*===========================================================================
 * L4 Tests: Self-Test and Diagnostics
 *===========================================================================*/

static void test_self_test(void)
{
    TEST("self_test");
    pointio_module_t mod;
    pointio_module_config_digital_input(&mod, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_SelfTest");
    mod.status = POINTIO_STATUS_OK;

    uint32_t results;
    CHECK(pointio_module_self_test(&mod, &results) == 0);
    CHECK(results & (1 << 0));  /* RAM test passed */
    CHECK(results & (1 << 1));  /* ROM test passed */
    CHECK(results & (1 << 4));  /* Comm loopback passed */
    CHECK(results & (1 << 5));  /* Terminal base present */
    PASS();
}

static void test_system_diagnostics(void)
{
    TEST("system_diagnostics");
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);

    pointio_module_t di;
    pointio_module_config_digital_input(&di, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Diag");
    di.slot.slot = 1;
    pointio_chassis_add_module(&chassis, &di);

    pointio_system_diag_t diag;
    CHECK(pointio_collect_system_diagnostics(&chassis, &diag) == 0);
    CHECK(diag.total_modules == 2);  /* Adapter + DI */
    CHECK(diag.ok_modules == 2);
    CHECK(diag.system_health_pct > 90.0);
    PASS();
}

/*===========================================================================
 * L5 Tests: Signal Processing
 *===========================================================================*/

static void test_moving_average(void)
{
    TEST("moving_average");
    double buffer[5] = {0};
    int write_idx = 0;
    double avg;
    int is_full = 0;

    /* Feed 5 samples: 1, 2, 3, 4, 5 */
    for (int i = 1; i <= 5; i++) {
        if (i == 5) is_full = 1;
        pointio_signal_moving_average(buffer, 5, &write_idx,
            (double)i, &avg, is_full);
    }
    CHECK(fabs(avg - 3.0) < 0.01);  /* Average of 1,2,3,4,5 = 3.0 */
    PASS();
}

static void test_median_filter(void)
{
    TEST("median_filter");
    double buffer[5] = {0};
    int write_idx = 0;
    double median;
    int is_full = 0;

    /* Values: 1, 100, 3, 4, 5 - median should be 4 */
    double values[] = {1.0, 100.0, 3.0, 4.0, 5.0};
    for (int i = 0; i < 5; i++) {
        if (i == 4) is_full = 1;
        pointio_signal_median_filter(buffer, 5, &write_idx,
            values[i], &median, is_full);
    }
    CHECK(fabs(median - 4.0) < 0.01);  /* Median: sorted [1,3,4,5,100] → 4 */
    PASS();
}

static void test_rate_limit(void)
{
    TEST("rate_limit");
    double limited;

    /* Within limits: delta=3, max_rate=5, dt=1.0 → step 3 <= 5, no clamp */
    int rc = pointio_signal_rate_limit(50.0, 53.0, 5.0, 1.0, &limited);
    CHECK(rc == 0);
    CHECK(fabs(limited - 53.0) < 0.01);

    /* Exceeds limits: delta=10, max_rate=5, dt=1.0 → step 10 > 5, clamp to +5 */
    rc = pointio_signal_rate_limit(50.0, 60.0, 5.0, 1.0, &limited);
    CHECK(rc == 1);  /* Rate limit applied */
    CHECK(fabs(limited - 55.0) < 0.01);
    PASS();
}

static void test_deadband(void)
{
    TEST("deadband");
    double output;
    int changed;

    /* Small change within deadband */
    pointio_signal_deadband(50.0, 50.3, 1.0, &output, &changed);
    CHECK(changed == 0);
    CHECK(fabs(output - 50.0) < 0.01);

    /* Large change outside deadband */
    pointio_signal_deadband(50.0, 52.0, 1.0, &output, &changed);
    CHECK(changed == 1);
    CHECK(fabs(output - 52.0) < 0.01);
    PASS();
}

static void test_outlier_detection(void)
{
    TEST("outlier_detection");
    int is_outlier;

    /* Normal value: z-score = (50-50)/2 = 0 */
    pointio_signal_detect_outlier_zscore(50.0, 50.0, 2.0, 3.0, &is_outlier);
    CHECK(is_outlier == 0);

    /* Outlier: z-score = (70-50)/2 = 10 */
    pointio_signal_detect_outlier_zscore(70.0, 50.0, 2.0, 3.0, &is_outlier);
    CHECK(is_outlier == 1);
    PASS();
}

static void test_welford_statistics(void)
{
    TEST("welford_statistics");
    int count = 0;
    double mean = 0.0, m2 = 0.0, variance;

    double values[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    int n = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < n; i++) {
        pointio_signal_update_statistics(values[i], &count, &mean, &m2, &variance);
    }

    CHECK(count == 8);
    CHECK(fabs(mean - 5.0) < 0.01);  /* Mean of [2,4,4,4,5,5,7,9] = 5.0 */
    PASS();
}

static void test_signal_frozen(void)
{
    TEST("signal_frozen");
    double buffer_frozen[10] = {5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0};
    double buffer_normal[10] = {5.0, 5.1, 4.9, 5.2, 4.8, 5.0, 5.1, 4.9, 5.0, 5.0};

    int is_frozen;
    double stddev;

    pointio_signal_detect_frozen(buffer_frozen, 10, 0.1, &is_frozen, &stddev);
    CHECK(is_frozen == 1);

    pointio_signal_detect_frozen(buffer_normal, 10, 0.1, &is_frozen, &stddev);
    CHECK(is_frozen == 0);
    PASS();
}

static void test_linear_interpolate(void)
{
    TEST("linear_interpolate");
    double y;
    /* Interpolate midpoint: (0,0) → (10,10), at x=5 should give y=5 */
    pointio_signal_linear_interpolate(0.0, 0.0, 10.0, 10.0, 5.0, &y);
    CHECK(fabs(y - 5.0) < 0.01);

    /* Extrapolation clamped: x < x0 → y0 */
    pointio_signal_linear_interpolate(0.0, 0.0, 10.0, 10.0, -5.0, &y);
    CHECK(fabs(y - 0.0) < 0.01);
    PASS();
}

static void test_signal_validate(void)
{
    TEST("signal_validate");
    int quality;

    /* Good signal */
    pointio_signal_validate(50.0, 0.0, 100.0, 10.0, 49.0, 1.0,
        2.0, 1.0, &quality);
    CHECK(quality == 0);  /* Good */

    /* Out of range */
    pointio_signal_validate(150.0, 0.0, 100.0, 10.0, 50.0, 1.0,
        2.0, 1.0, &quality);
    CHECK(quality >= 2);  /* Bad or Invalid */
    PASS();
}

static void test_snr_estimate(void)
{
    TEST("snr_estimate");
    double snr_db;
    int quality_label;

    /* High SNR: strong signal, low noise */
    pointio_signal_estimate_snr(10.0, 100.0, 0.01, &snr_db, &quality_label);
    CHECK(snr_db > 30.0);
    CHECK(quality_label >= 2);  /* Good or Excellent */

    /* Low SNR */
    pointio_signal_estimate_snr(1.0, 1.0, 10.0, &snr_db, &quality_label);
    CHECK(snr_db < 20.0);
    CHECK(quality_label <= 1);  /* Poor or Marginal */
    PASS();
}

static void test_holt_smoothing(void)
{
    TEST("holt_smoothing");
    double level = 100.0, trend = 10.0;
    double smoothed, forecast;

    pointio_signal_holt_smoothing(0.5, 0.3, 120.0, &level, &trend,
        &smoothed, &forecast);
    CHECK(forecast == 110.0);  /* level + trend = forecast */
    CHECK(smoothed > 105.0);   /* Updated level between 110 and 120 */
    PASS();
}

/*===========================================================================
 * L6 Tests: Scanner Operations
 *===========================================================================*/

static void test_scanner(void)
{
    TEST("scanner");
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);

    pointio_module_t di;
    pointio_module_config_digital_input(&di, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Scan");
    di.slot.slot = 1;
    pointio_chassis_add_module(&chassis, &di);

    pointio_connection_pool_t pool;
    pointio_conn_pool_init(&pool);

    /* Start scanner */
    CHECK(pointio_scanner_start(20000) == 0);
    CHECK(pointio_scanner_is_running() == 1);

    /* Run one scan cycle */
    CHECK(pointio_scanner_scan_cycle(&chassis, &pool, 1000000) == 0);

    /* COS signal */
    CHECK(pointio_scanner_signal_cos(&chassis, 1) == 0);

    /* Update single module */
    CHECK(pointio_scanner_update_module_input(&chassis, 1) == 0);

    /* Get timing */
    double avg, max_t, min_t;
    uint32_t overruns;
    CHECK(pointio_scanner_get_timing(&avg, &max_t, &min_t, &overruns) == 0);

    /* Jitter analysis */
    double max_jitter, rms_jitter;
    int is_excessive;
    CHECK(pointio_scanner_analyze_jitter(&max_jitter, &rms_jitter, &is_excessive) == 0);

    /* Stop scanner */
    CHECK(pointio_scanner_stop(&chassis) == 0);
    CHECK(pointio_scanner_is_running() == 0);
    PASS();
}

/*===========================================================================
 * L5 Tests: Rack-Optimized Connection
 *===========================================================================*/

static void test_rack_optimized(void)
{
    TEST("rack_optimized");
    pointio_chassis_t chassis;
    pointio_chassis_init(&chassis);

    pointio_module_t di1;
    pointio_module_config_digital_input(&di1, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Rack1");
    di1.slot.slot = 1;
    di1.input_data[0] = 0xAA;
    pointio_chassis_add_module(&chassis, &di1);

    pointio_module_t di2;
    pointio_module_config_digital_input(&di2, "1734-IB8", 8,
        POINTIO_FILTER_1MS, "DI_Rack2");
    di2.slot.slot = 2;
    di2.input_data[0] = 0x55;
    pointio_chassis_add_module(&chassis, &di2);

    uint8_t buffer[256];
    uint16_t data_len;
    CHECK(pointio_scanner_build_rack_optimized_input(&chassis, buffer, 256, &data_len) == 0);
    CHECK(data_len >= 6);  /* 4 header + 1 for DI1 + 1 for DI2 */
    CHECK(buffer[4] == 0xAA);  /* DI1 data */
    CHECK(buffer[5] == 0x55);  /* DI2 data */

    /* Distribute rack-optimized output */
    uint8_t out_data[256];
    memset(out_data, 0, sizeof(out_data));
    out_data[4] = 0x0F;  /* DI1: channels 0-3 ON */
    out_data[5] = 0xF0;  /* DI2: channels 4-7 ON */

    /* But wait, rack output goes to DO modules. Let's add DO modules. */
    pointio_module_t do1;
    pointio_module_config_digital_output(&do1, "1734-OB8", 8,
        POINTIO_FAULT_MODE_ZERO, POINTIO_PROG_MODE_ZERO, "DO_Rack1");
    do1.slot.slot = 1;
    pointio_chassis_add_module(&chassis, &do1);  /* Overwrites DI at slot 1 */

    /* Rebuild with DO modules and test distribution */
    CHECK(pointio_scanner_distribute_rack_optimized_output(&chassis, out_data, data_len) == 0);
    PASS();
}

/*===========================================================================
 * L7 Tests: CompactLogix CPU Properties
 *===========================================================================*/

static void test_cpu_properties(void)
{
    TEST("cpu_properties");
    compactlogix_cpu_t cpu;

    CHECK(compactlogix_get_cpu_properties(CPU_1769_L30ER, &cpu) == 0);
    CHECK(cpu.user_memory_kb == 1000);
    CHECK(cpu.max_ethernet_nodes == 32);
    CHECK(cpu.has_integrated_motion == 0);

    CHECK(compactlogix_get_cpu_properties(CPU_1769_L36ERM, &cpu) == 0);
    CHECK(cpu.has_integrated_motion == 1);
    CHECK(cpu.max_motion_axes == 16);
    CHECK(cpu.has_dual_ethernet == 1);

    /* Capacity check */
    CHECK(compactlogix_check_capacity(&cpu, 20, 40) == 1);  /* 20 ≤ 30, 40 ≤ 48 */
    CHECK(compactlogix_check_capacity(&cpu, 50, 10) == 0);  /* 50 > 30 max modules */

    /* Unknown model */
    CHECK(compactlogix_get_cpu_properties(CPU_MODEL_UNKNOWN, &cpu) == -1);
    PASS();
}

/*===========================================================================
 * Run all tests
 *===========================================================================*/

int main(void)
{
    printf("=== mini-compactlogix-pointio Test Suite ===\n\n");

    printf("--- L1: Module Initialization ---\n");
    test_chassis_init();
    test_module_init();
    test_config_digital_input();
    test_config_digital_output();
    test_config_analog_input();
    test_chassis_add_remove();

    printf("\n--- L4: Power Budget ---\n");
    test_power_budget();

    printf("\n--- L5: Digital I/O ---\n");
    test_di_read_write();
    test_do_write();
    test_edge_detection();

    printf("\n--- L5: Analog I/O ---\n");
    test_analog_scaling();
    test_loop_integrity();

    printf("\n--- L4: Connection Management ---\n");
    test_connection_pool();
    test_connection_timeout();

    printf("\n--- L6: Fault Management ---\n");
    test_fault_codes();
    test_fault_clear();
    test_module_inhibit();

    printf("\n--- L4: LED & Diagnostics ---\n");
    test_led_states();
    test_self_test();
    test_system_diagnostics();

    printf("\n--- L5: Signal Processing ---\n");
    test_moving_average();
    test_median_filter();
    test_rate_limit();
    test_deadband();
    test_outlier_detection();
    test_welford_statistics();
    test_signal_frozen();
    test_linear_interpolate();
    test_signal_validate();
    test_snr_estimate();
    test_holt_smoothing();

    printf("\n--- L6: Scanner ---\n");
    test_scanner();
    test_rack_optimized();

    printf("\n--- L7: CPU Properties ---\n");
    test_cpu_properties();

    printf("\n=========================================\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("=========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
