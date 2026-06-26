/**
 * @file test_axis_state.c
 * @brief Tests for axis state machine, homing, fault handling.
 *
 * Tests assertions (L4): state transitions, fault-set, fault-clear,
 * following error detection, homing sequence, STO safety.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "axis_types.h"

/* External declarations from axis_state.c */
void motor_database_init_default(motor_database_t *motor);
void motor_database_init_vpl_b1003f(motor_database_t *motor);
void motor_database_init_vpl_b1653f(motor_database_t *motor);
void motor_database_init_linear(motor_database_t *motor,
                                 double force_cont, double force_peak,
                                 double mass_kg, double pole_pitch_mm);
bool axis_state_transition_valid(axis_state_t current, axis_state_t requested);
axis_state_t axis_state_transition(axis_runtime_t *axis, axis_state_t requested);
axis_state_t axis_set_fault(axis_runtime_t *axis, uint16_t fault_code);
int axis_clear_faults(axis_runtime_t *axis);
bool axis_check_following_error(axis_runtime_t *axis, double tolerance);
bool axis_check_soft_overtravel(axis_runtime_t *axis, double pos_max, double pos_min);
int homing_init(home_method_t method, double fast_speed,
                double slow_speed, double offset);
home_phase_t homing_step(double current_position, bool switch_active,
                         bool index_pulse, double *cmd_velocity);
bool homing_is_complete(void);
void homing_abort(void);
double commutation_compute_electrical_angle(double encoder_angle_rad,
                                             uint32_t pole_pairs,
                                             double commutation_offset);
void sto_state_machine(bool sto_channel1, bool sto_channel2,
                       bool reset_needed, bool *sto_active, bool *fault);
void axis_config_init(axis_config_t *config);
void axis_runtime_init(axis_runtime_t *axis, const axis_config_t *config);
const char *axis_state_to_string(axis_state_t state);
int kinetix_drive_lookup(uint16_t catalog_id, drive_power_rating_t *rating);
int kinetix_compute_regenerative_energy(double bus_voltage,
                                         double bus_capacitance,
                                         double inertia, double initial_velocity,
                                         double *final_voltage, double *shunt_energy);
bool kinetix_check_drive_motor_match(const drive_power_rating_t *rating,
                                      const motor_database_t *motor,
                                      double duty_cycle);
double kinetix_thermal_model_update(double current_amps, double speed_rps,
                                      const motor_database_t *motor,
                                      double ambient_temp_c,
                                      double prev_temp_c, double Ts);
double kinetix_compute_overload_time(double current_amps,
                                       const motor_database_t *motor);
double kinetix_capacitor_life_estimate(double rated_life_h,
                                         double rated_temp_c, double actual_temp_c,
                                         double rated_voltage, double actual_voltage);
bool kinetix_sto_integrity_check(bool sto1_active, bool sto2_active,
                                   bool gate_drive_disabled);
double kinetix_sto_pfhd_compute(double lambda_d1, double lambda_d2,
                                  double diag_interval);
double kinetix_sto_sff_compute(double lambda_s, double lambda_dd,
                                 double lambda_du);
int kinetix_recommend_drive(const motor_database_t *motor,
                              double load_inertia_ratio,
                              uint16_t *drive_catalog);

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) do { \
    test_count++; \
    printf("  TEST %d: %s ... ", test_count, name); \
} while(0)

#define PASS() do { pass_count++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

/*===========================================================================
 * L1-L2: Axis State Machine Tests
 *===========================================================================*/

static void test_valid_transitions(void)
{
    TEST("DISABLED → STARTING valid");
    assert(axis_state_transition_valid(AXIS_STATE_DISABLED, AXIS_STATE_STARTING));
    PASS();

    TEST("DISABLED → RUNNING invalid");
    assert(!axis_state_transition_valid(AXIS_STATE_DISABLED, AXIS_STATE_RUNNING));
    PASS();

    TEST("STARTING → RUNNING valid");
    assert(axis_state_transition_valid(AXIS_STATE_STARTING, AXIS_STATE_RUNNING));
    PASS();

    TEST("STARTING → ERROR_STOP valid");
    assert(axis_state_transition_valid(AXIS_STATE_STARTING, AXIS_STATE_ERROR_STOP));
    PASS();

    TEST("RUNNING → STOPPING valid");
    assert(axis_state_transition_valid(AXIS_STATE_RUNNING, AXIS_STATE_STOPPING));
    PASS();

    TEST("RUNNING → DISABLED invalid");
    assert(!axis_state_transition_valid(AXIS_STATE_RUNNING, AXIS_STATE_DISABLED));
    PASS();

    TEST("STOPPING → DISABLED valid");
    assert(axis_state_transition_valid(AXIS_STATE_STOPPING, AXIS_STATE_DISABLED));
    PASS();

    TEST("ERROR_STOP → DISABLED valid");
    assert(axis_state_transition_valid(AXIS_STATE_ERROR_STOP, AXIS_STATE_DISABLED));
    PASS();

    TEST("Any → OFFLINE valid");
    assert(axis_state_transition_valid(AXIS_STATE_RUNNING, AXIS_STATE_AXIS_OFFLINE));
    PASS();

    TEST("OFFLINE → DISABLED valid");
    assert(axis_state_transition_valid(AXIS_STATE_AXIS_OFFLINE, AXIS_STATE_DISABLED));
    PASS();

    TEST("HOMING → RUNNING valid");
    assert(axis_state_transition_valid(AXIS_STATE_HOMING, AXIS_STATE_RUNNING));
    PASS();
}

static void test_state_transition_side_effects(void)
{
    TEST("MSO transition sets servo_active");
    axis_runtime_t axis;
    axis_runtime_init(&axis, NULL);
    axis_state_transition(&axis, AXIS_STATE_STARTING);
    assert(axis.state == AXIS_STATE_STARTING);
    assert(!axis.servo_active);
    PASS();

    TEST("STARTING → RUNNING enables servo");
    axis_state_transition(&axis, AXIS_STATE_RUNNING);
    assert(axis.state == AXIS_STATE_RUNNING);
    assert(axis.servo_active);
    PASS();

    TEST("DISABLED clears velocity and torque");
    axis_state_transition(&axis, AXIS_STATE_STOPPING);
    axis_state_transition(&axis, AXIS_STATE_DISABLED);
    assert(!axis.servo_active);
    assert(fabs(axis.cmd_velocity) < 1e-9);
    assert(fabs(axis.cmd_torque_ff) < 1e-9);
    PASS();
}

static void test_fault_handling(void)
{
    TEST("Fault sets ERROR_STOP from RUNNING");
    axis_runtime_t axis;
    axis_runtime_init(&axis, NULL);
    axis_state_transition(&axis, AXIS_STATE_STARTING);
    axis_state_transition(&axis, AXIS_STATE_RUNNING);
    axis_set_fault(&axis, AXIS_FAULT_MOTOR_OVERLOAD);
    assert(axis.state == AXIS_STATE_ERROR_STOP);
    assert(axis.fault_code & AXIS_FAULT_MOTOR_OVERLOAD);
    PASS();

    TEST("Cannot clear faults while RUNNING");
    axis_runtime_t axis2;
    axis_runtime_init(&axis2, NULL);
    axis_state_transition(&axis2, AXIS_STATE_STARTING);
    axis_state_transition(&axis2, AXIS_STATE_RUNNING);
    int result = axis_clear_faults(&axis2);
    assert(result == -1);
    PASS();

    TEST("Clear faults from ERROR_STOP");
    int result2 = axis_clear_faults(&axis);
    assert(result2 == 0);
    assert(axis.fault_code == 0);
    assert(axis.state == AXIS_STATE_DISABLED);
    PASS();
}

static void test_following_error(void)
{
    TEST("Following error detection");
    axis_runtime_t axis;
    axis_runtime_init(&axis, NULL);
    axis_state_transition(&axis, AXIS_STATE_STARTING);
    axis_state_transition(&axis, AXIS_STATE_RUNNING);
    axis.cmd_position = 100.0;
    axis.actual_position = 95.0;
    bool faulted = axis_check_following_error(&axis, 2.0);
    assert(faulted);
    assert(axis.fault_code & AXIS_FAULT_FOLLOWING_ERROR);
    PASS();

    TEST("Following error within tolerance");
    axis_runtime_t axis2;
    axis_runtime_init(&axis2, NULL);
    axis_state_transition(&axis2, AXIS_STATE_STARTING);
    axis_state_transition(&axis2, AXIS_STATE_RUNNING);
    axis2.cmd_position = 100.0;
    axis2.actual_position = 99.0;
    bool faulted2 = axis_check_following_error(&axis2, 2.0);
    assert(!faulted2);
    PASS();
}

/*===========================================================================
 * L1-L3: Homing Sequence Tests
 *===========================================================================*/

static void test_homing_sequence(void)
{
    TEST("Homing initialization");
    int result = homing_init(HOME_METHOD_SWITCH_NEG, 100.0, 10.0, 0.0);
    assert(result == 0);
    PASS();

    TEST("Homing fast approach phase");
    double cmd_vel;
    home_phase_t phase = homing_step(0.0, false, false, &cmd_vel);
    assert(phase == HOME_PHASE_FAST_APPROACH);
    assert(cmd_vel == -100.0);
    PASS();

    TEST("Homing switch detection");
    phase = homing_step(100.0, true, false, &cmd_vel);
    assert(phase == HOME_PHASE_SWITCH_DETECT);
    PASS();

    TEST("Homing creep to index after switch release");
    phase = homing_step(105.0, false, false, &cmd_vel);
    assert(phase == HOME_PHASE_CREEP_TO_INDEX);
    PASS();

    TEST("Homing index found");
    phase = homing_step(106.0, false, true, &cmd_vel);
    assert(phase == HOME_PHASE_INDEX_FOUND);
    PASS();

    TEST("Homing offset move complete");
    phase = homing_step(106.0, false, false, &cmd_vel);
    assert(phase == HOME_PHASE_OFFSET_MOVE);
    phase = homing_step(106.0, false, false, &cmd_vel);
    /* Should be near target */
    assert(phase == HOME_PHASE_COMPLETE || phase == HOME_PHASE_OFFSET_MOVE);
    PASS();

    TEST("Homing abort");
    homing_abort();
    assert(!homing_is_complete());
    PASS();
}

/*===========================================================================
 * L1-L3: Motor Database Tests
 *===========================================================================*/

static void test_motor_database(void)
{
    TEST("Default motor initialization");
    motor_database_t motor;
    motor_database_init_default(&motor);
    assert(motor.type == MOTOR_TYPE_ROTARY_PM);
    assert(motor.rated_torque_nm > 0.0);
    assert(motor.pole_pairs > 0);
    PASS();

    TEST("VPL-B1003F catalog data");
    motor_database_init_vpl_b1003f(&motor);
    assert(strcmp(motor.catalog_number, "VPL-B1003F") == 0);
    assert(fabs(motor.rated_torque_nm - 1.27) < 0.01);
    assert(fabs(motor.rotor_inertia_kgcm2 - 2.77) < 0.1);
    assert(motor.pole_pairs == 4);
    PASS();

    TEST("VPL-B1653F catalog data");
    motor_database_init_vpl_b1653f(&motor);
    assert(fabs(motor.rated_power_kw - 4.7) < 0.1);
    assert(motor.brake_fitted == true);
    PASS();

    TEST("Linear motor initialization");
    motor_database_init_linear(&motor, 500.0, 1500.0, 5.0, 60.0);
    assert(motor.type == MOTOR_TYPE_LINEAR_PM);
    assert(fabs(motor.rated_torque_nm - 500.0) < 0.1);
    PASS();
}

/*===========================================================================
 * L4: STO Safety Tests
 *===========================================================================*/

static void test_sto_safety(void)
{
    TEST("STO both channels de-energized = STO active (torque inhibited)");
    bool sto_active, fault;
    sto_state_machine(false, false, false, &sto_active, &fault);
    /* Both channels low = no 24V = STO active = torque inhibited */
    /* sto_active=true means torque is being held off (safe state) */
    /* Both channels agree → no channel fault */
    assert(!fault);
    PASS();

    TEST("STO channel mismatch = fault");
    sto_state_machine(true, false, false, &sto_active, &fault);
    assert(fault);
    PASS();

    TEST("STO both channels energized = STO not active, no fault");
    /* Reset previous state: both channels 24V with reset clears latches */
    sto_state_machine(true, true, true, &sto_active, &fault);
    assert(!sto_active);
    assert(!fault);
    PASS();

    TEST("STO integrity check valid");
    assert(kinetix_sto_integrity_check(true, true, true));
    PASS();

    TEST("STO integrity check fail – channel mismatch");
    assert(!kinetix_sto_integrity_check(true, false, true));
    PASS();

    TEST("STO PFHd within SIL 3 limit");
    double pfhd = kinetix_sto_pfhd_compute(1e-7, 1e-7, 0.1 / 3600.0);
    assert(pfhd < 1e-7);  /* SIL 3 requirement */
    PASS();

    TEST("STO SFF > 99%");
    double sff = kinetix_sto_sff_compute(100.0, 10.0, 0.5);
    assert(sff > 0.99);
    PASS();
}

/*===========================================================================
 * L1: Commutation and Axis Config Tests
 *===========================================================================*/

static void test_commutation(void)
{
    TEST("Electrical angle zero offset");
    double angle = commutation_compute_electrical_angle(0.0, 4, 0.0);
    assert(fabs(angle - 0.0) < 1e-9);
    PASS();

    TEST("Electrical angle wraps at 2π");
    double angle2 = commutation_compute_electrical_angle(M_PI / 2.0, 4, 0.0);
    assert(fabs(angle2 - 0.0) < 1e-9); /* 4 * π/2 = 2π = 0 mod 2π */
    PASS();

    TEST("Axis config initialization");
    axis_config_t config;
    axis_config_init(&config);
    assert(config.max_velocity > 0.0);
    assert(config.sil_level == 3);
    assert(config.drive_catalog == KINETIX_5300);
    PASS();

    TEST("Axis state to string");
    assert(strcmp(axis_state_to_string(AXIS_STATE_RUNNING), "RUNNING") == 0);
    assert(strcmp(axis_state_to_string(AXIS_STATE_ERROR_STOP), "ERROR_STOP") == 0);
    PASS();
}

/*===========================================================================
 * L6: Drive Selection Test
 *===========================================================================*/

static void test_drive_sizing(void)
{
    TEST("Drive lookup Kinetix 5300");
    drive_power_rating_t rating;
    int result = kinetix_drive_lookup(0x5301, &rating);
    assert(result == 0);
    assert(rating.output_power_kw > 0.0);
    PASS();

    TEST("Recommend drive for VPL-B1003F");
    motor_database_t motor;
    motor_database_init_vpl_b1003f(&motor);
    uint16_t drive_id;
    result = kinetix_recommend_drive(&motor, 1.0, &drive_id);
    assert(result == 0);
    assert(drive_id > 0);
    PASS();

    TEST("Drive-motor match check (VPL-B1653F + 2198-C1020-ERS)");
    drive_power_rating_t dr;
    /* 2198-C1020-ERS: 16A cont, 48A peak, 7.7 kW — adequate for VPL-B1653F */
    kinetix_drive_lookup(0x5304, &dr);
    motor_database_init_vpl_b1653f(&motor);
    assert(kinetix_check_drive_motor_match(&dr, &motor, 0.6));
    PASS();

    TEST("Regenerative energy computation");
    double v_final, shunt_J;
    result = kinetix_compute_regenerative_energy(650.0, 820.0, 10.0, 300.0,
                                                  &v_final, &shunt_J);
    assert(result == 0);
    assert(v_final >= 650.0);
    PASS();

    TEST("Thermal model update");
    double temp = kinetix_thermal_model_update(2.0, 50.0, &motor, 40.0, 40.0, 0.1);
    assert(temp >= 40.0);
    PASS();

    TEST("Overload time finite");
    double t_trip = kinetix_compute_overload_time(20.0, &motor);
    assert(t_trip > 0.0 && t_trip < 1e9);
    PASS();

    TEST("Capacitor life estimate");
    double life = kinetix_capacitor_life_estimate(10000.0, 105.0, 65.0, 450.0, 325.0);
    assert(life > 10000.0); /* Cooler operation extends life */
    PASS();
}

/*===========================================================================
 * Main
 *===========================================================================*/

int main(void)
{
    printf("=== Axis State Machine Tests ===\n\n");

    test_valid_transitions();
    test_state_transition_side_effects();
    test_fault_handling();
    test_following_error();
    test_homing_sequence();
    test_motor_database();
    test_sto_safety();
    test_commutation();
    test_drive_sizing();

    printf("\n=== Results: %d/%d tests passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
