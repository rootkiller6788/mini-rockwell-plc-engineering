/**
 * @file axis_state.c
 * @brief Axis state machine, homing sequences, fault handling for Kinetix.
 *
 * Level: L1-L3, L6
 * Reference:
 *   - CIP Motion Specification, Volume 2 – Motion Axis Object (Class 0x42)
 *   - Rockwell MOTION-RM002 – Motion Axis State Model
 *   - IEC 61800-7-201 (CiA 402) – Servo drive state machine
 *   - Hughes & Drury, "Electric Motors and Drives" (2013), Ch.9
 *
 * Implements:
 *   L1: Axis state enumeration validation, fault code bit definitions
 *   L2: State transition rules, safe state progression
 *   L3: Homing sequence FSM, commutation find algorithm
 *   L6: Classic servo startup sequence, safe torque off handling
 *
 * Alignment: MIT 2.171, RWTH Industrial Control, ISA/IEC 61131-3
 */

#include "axis_types.h"
#include "servo_tuning.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*===========================================================================
 * L1: Motor database catalog entries – Standard Rockwell motors
 *===========================================================================*/

/**
 * @brief Initialize a motor database entry with safe defaults.
 *
 * @param motor  Motor database entry to initialize
 */
void motor_database_init_default(motor_database_t *motor)
{
    if (!motor) return;
    memset(motor, 0, sizeof(motor_database_t));
    motor->type = MOTOR_TYPE_ROTARY_PM;
    motor->rated_torque_nm = 1.0;
    motor->peak_torque_nm  = 3.0;
    motor->rated_speed_rpm = 3000.0;
    motor->max_speed_rpm   = 6000.0;
    motor->rated_current_amps = 2.0;
    motor->peak_current_amps  = 6.0;
    motor->torque_constant_kt = 0.5;
    motor->voltage_constant_kv = 60.0;
    motor->pole_pairs = 4;
    motor->encoder_counts_per_rev = 262144;
    motor->feedback = FEEDBACK_TYPE_ABSOLUTE;
    motor->rotor_inertia_kgcm2 = 0.5;
    motor->thermal_time_const_s = 600.0;
    motor->max_winding_temp_c = 155.0;
}

/**
 * @brief Initialize a motor database with VPL-B1003F catalog data.
 *
 * VPL-B1003F is a Kinetix VP Low Inertia servo motor:
 *   240VAC class, 3000 RPM rated, 1.27 N·m continuous, 3.82 N·m peak,
 *   0.000277 kg·m² rotor inertia (2.77 kg·cm²).
 *
 * @param motor  Motor database entry to fill
 */
void motor_database_init_vpl_b1003f(motor_database_t *motor)
{
    if (!motor) return;
    memset(motor, 0, sizeof(motor_database_t));

    strcpy(motor->catalog_number, "VPL-B1003F");
    motor->type = MOTOR_TYPE_ROTARY_PM;
    motor->rated_power_kw = 0.4;
    motor->rated_torque_nm = 1.27;
    motor->peak_torque_nm  = 3.82;
    motor->rated_speed_rpm = 3000.0;
    motor->max_speed_rpm   = 5000.0;
    motor->rated_current_amps = 2.2;
    motor->peak_current_amps  = 6.8;
    motor->torque_constant_kt = 0.58;    /* N·m/A RMS = 0.82 N·m/A peak */
    motor->voltage_constant_kv = 42.0;   /* Vpeak/kRPM line-to-line */
    motor->resistance_ohms = 2.85;
    motor->inductance_mh = 8.2;
    motor->pole_pairs = 4;
    motor->rotor_inertia_kgcm2 = 2.77;
    motor->thermal_time_const_s = 1200.0;
    motor->encoder_counts_per_rev = 262144;  /**< 18-bit absolute, 1024 sin/cos */
    motor->feedback = FEEDBACK_TYPE_SINCOS;
    motor->max_winding_temp_c = 155.0;
    motor->brake_fitted = false;
}

/**
 * @brief Initialize a motor database with VPL-B1653F catalog data.
 *
 * VPL-B1653F: 165mm frame, mid-inertia, 5000 RPM.
 * Continuous: 11.9 N·m, Peak: 35.7 N·m, 4.7 kW rated.
 * Used in high-dynamic packaging and material handling.
 *
 * @param motor  Motor database entry to fill
 */
void motor_database_init_vpl_b1653f(motor_database_t *motor)
{
    if (!motor) return;
    memset(motor, 0, sizeof(motor_database_t));

    strcpy(motor->catalog_number, "VPL-B1653F");
    motor->type = MOTOR_TYPE_ROTARY_PM;
    motor->rated_power_kw = 4.7;
    motor->rated_torque_nm = 11.9;
    motor->peak_torque_nm  = 35.7;
    motor->rated_speed_rpm = 3000.0;
    motor->max_speed_rpm   = 5000.0;
    motor->rated_current_amps = 16.0;
    motor->peak_current_amps  = 48.0;
    motor->torque_constant_kt = 0.74;
    motor->voltage_constant_kv = 46.0;
    motor->resistance_ohms = 0.38;
    motor->inductance_mh = 2.4;
    motor->pole_pairs = 5;
    motor->rotor_inertia_kgcm2 = 26.0;
    motor->thermal_time_const_s = 2400.0;
    motor->encoder_counts_per_rev = 262144;
    motor->feedback = FEEDBACK_TYPE_SINCOS;
    motor->max_winding_temp_c = 155.0;
    motor->brake_fitted = true;
}

/**
 * @brief Initialize a linear motor database (LDL series).
 *
 * LDL-Txxxx: Kinetix linear motors for direct-drive linear motion.
 * No rotary inertia – uses mass (kg) instead.
 *
 * @param motor        Motor database entry to fill
 * @param force_cont   Continuous force (N)
 * @param force_peak   Peak force (N)
 * @param mass_kg      Moving mass (kg)
 * @param pole_pitch_mm Magnetic pole pitch (mm)
 */
void motor_database_init_linear(motor_database_t *motor,
                                 double force_cont, double force_peak,
                                 double mass_kg, double pole_pitch_mm)
{
    if (!motor) return;
    memset(motor, 0, sizeof(motor_database_t));

    snprintf(motor->catalog_number, sizeof(motor->catalog_number),
             "LDL-N050-%dN", (int)force_cont);
    motor->type = MOTOR_TYPE_LINEAR_PM;
    /* For linear motors, "torque" is actually force (N) */
    motor->rated_torque_nm = force_cont;
    motor->peak_torque_nm  = force_peak;
    motor->torque_constant_kt = force_cont / 10.0;  /**< N/A typical */
    motor->rotor_inertia_kgcm2 = mass_kg * 1000.0;  /**< Convert kg to kg·cm² equivalent */
    motor->rated_speed_rpm = pole_pitch_mm * 100.0; /**< mm/s equivalent */
    motor->pole_pairs = (uint32_t)(300.0 / pole_pitch_mm);
    motor->encoder_counts_per_rev = (uint32_t)(pole_pitch_mm * 1000.0);
    motor->feedback = FEEDBACK_TYPE_ABSOLUTE;
    motor->rated_current_amps = 10.0;
    motor->peak_current_amps  = 30.0;
}

/*===========================================================================
 * L1-L2: Axis state machine transitions
 *===========================================================================*/

/**
 * @brief Validate that a requested axis state transition is legal.
 *
 * CIP Motion defines a strict state machine with these legal transitions:
 *
 *   DISABLED   → STARTING    (MSO command)
 *   STARTING   → RUNNING     (startup complete)
 *   STARTING   → ERROR_STOP  (startup fails)
 *   RUNNING    → STOPPING    (MSF or MAS with disable)
 *   STOPPING   → DISABLED    (stopped, bus voltage removed)
 *   RUNNING    → ERROR_STOP  (fault detected)
 *   STOPPING   → ERROR_STOP  (fault during stop)
 *   ERROR_STOP → DISABLED    (fault cleared via MAFR)
 *   RUNNING    → ABORTING    (MAS immediate abort)
 *   ABORTING   → DISABLED    (abort complete)
 *   RUNNING    → HOMING      (MAH command)
 *   HOMING     → RUNNING     (home complete)
 *   HOMING     → ERROR_STOP  (home fails)
 *   Any        → OFFLINE     (communication loss)
 *   OFFLINE    → DISABLED    (communication restored)
 *
 * @param current   Current axis state
 * @param requested Requested new axis state
 * @return          true if transition is legal
 */
bool axis_state_transition_valid(axis_state_t current, axis_state_t requested)
{
    /* Communication loss always allowed */
    if (requested == AXIS_STATE_AXIS_OFFLINE) return true;

    /* Recovery from offline always allowed */
    if (current == AXIS_STATE_AXIS_OFFLINE && requested == AXIS_STATE_DISABLED)
        return true;

    switch (current) {
    case AXIS_STATE_DISABLED:
        return (requested == AXIS_STATE_STARTING ||
                requested == AXIS_STATE_TESTING);

    case AXIS_STATE_STARTING:
        return (requested == AXIS_STATE_RUNNING ||
                requested == AXIS_STATE_ERROR_STOP ||
                requested == AXIS_STATE_DISABLED);

    case AXIS_STATE_RUNNING:
        return (requested == AXIS_STATE_STOPPING ||
                requested == AXIS_STATE_ERROR_STOP ||
                requested == AXIS_STATE_ABORTING ||
                requested == AXIS_STATE_HOMING);

    case AXIS_STATE_STOPPING:
        return (requested == AXIS_STATE_DISABLED ||
                requested == AXIS_STATE_ERROR_STOP);

    case AXIS_STATE_ERROR_STOP:
        return (requested == AXIS_STATE_DISABLED);

    case AXIS_STATE_ABORTING:
        return (requested == AXIS_STATE_DISABLED ||
                requested == AXIS_STATE_ERROR_STOP);

    case AXIS_STATE_HOMING:
        return (requested == AXIS_STATE_RUNNING ||
                requested == AXIS_STATE_ERROR_STOP);

    case AXIS_STATE_TESTING:
        return (requested == AXIS_STATE_DISABLED ||
                requested == AXIS_STATE_ERROR_STOP);

    default:
        return false;
    }
}

/**
 * @brief Execute an axis state transition.
 *
 * Updates the axis runtime state, returning the actual resulting state.
 * If the transition is illegal, returns the current state unchanged.
 *
 * @param axis              Axis runtime state
 * @param requested         Requested new state
 * @return                  Actual resulting state
 */
axis_state_t axis_state_transition(axis_runtime_t *axis, axis_state_t requested)
{
    if (!axis) return AXIS_STATE_ERROR_STOP;

    if (!axis_state_transition_valid(axis->state, requested)) {
        return axis->state; /* Illegal transition – no change */
    }

    axis_state_t prev_state = axis->state;
    axis->state = requested;

    /* Side effects of state transitions */
    switch (requested) {
    case AXIS_STATE_STARTING:
        axis->servo_active = false;
        axis->at_home = false;
        break;

    case AXIS_STATE_RUNNING:
        axis->servo_active = true;
        axis->move_complete = true;  /* No active move on entry */
        axis->active_motion_instr = -1;
        break;

    case AXIS_STATE_STOPPING:
        axis->cmd_velocity = 0.0;
        axis->cmd_acceleration = 0.0;
        break;

    case AXIS_STATE_DISABLED:
        axis->servo_active = false;
        axis->cmd_velocity = 0.0;
        axis->cmd_torque_ff = 0.0;
        axis->iq_ref = 0.0;
        axis->id_ref = 0.0;
        break;

    case AXIS_STATE_ERROR_STOP:
        axis->cmd_velocity = 0.0;
        axis->cmd_torque_ff = 0.0;
        axis->iq_ref = 0.0;
        axis->id_ref = 0.0;
        /* Keep servo_active true during error stop for controlled deceleration */
        break;

    case AXIS_STATE_AXIS_OFFLINE:
        axis->servo_active = false;
        break;

    default:
        break;
    }

    (void)prev_state; /* Suppress unused variable warning */
    return axis->state;
}

/*===========================================================================
 * L1-L3: Fault handling
 *===========================================================================*/

/**
 * @brief Set a fault on an axis and transition to error state.
 *
 * Sets the fault code bits and forces the axis into ERROR_STOP state
 * if running, or stays in current state if already disabled.
 *
 * @param axis       Axis runtime state
 * @param fault_code Fault code (bitmask)
 * @return           Actual resulting axis state
 */
axis_state_t axis_set_fault(axis_runtime_t *axis, uint16_t fault_code)
{
    if (!axis) return AXIS_STATE_ERROR_STOP;

    axis->fault_code |= fault_code;

    if (axis->state == AXIS_STATE_RUNNING ||
        axis->state == AXIS_STATE_HOMING ||
        axis->state == AXIS_STATE_ABORTING) {
        return axis_state_transition(axis, AXIS_STATE_ERROR_STOP);
    }

    if (axis->state == AXIS_STATE_STARTING) {
        return axis_state_transition(axis, AXIS_STATE_ERROR_STOP);
    }

    return axis->state;
}

/**
 * @brief Clear all faults on an axis (MAFR instruction logic).
 *
 * Resets the fault code to zero and transitions to DISABLED state.
 * Follows the CIP Motion rule: faults can only be cleared from
 * ERROR_STOP or DISABLED state.
 *
 * @param axis  Axis runtime state
 * @return      0 on success, -1 if axis not in a fault-clearable state
 */
int axis_clear_faults(axis_runtime_t *axis)
{
    if (!axis) return -1;

    if (axis->state != AXIS_STATE_ERROR_STOP &&
        axis->state != AXIS_STATE_DISABLED) {
        return -1; /* Cannot clear faults while running */
    }

    axis->fault_code = 0;
    axis_state_transition(axis, AXIS_STATE_DISABLED);
    return 0;
}

/**
 * @brief Check for following error fault.
 *
 * Theorem (Following Error): |cmd_pos - actual_pos| > pos_error_tolerance
 * triggers AXIS_FAULT_FOLLOWING_ERROR when axis is active.
 *
 * @param axis      Axis runtime state
 * @param tolerance Following error tolerance (eng units)
 * @return          true if following error fault
 */
bool axis_check_following_error(axis_runtime_t *axis, double tolerance)
{
    if (!axis) return false;

    if (!axis->servo_active) return false;

    axis->position_error = axis->cmd_position - axis->actual_position;

    if (fabs(axis->position_error) > tolerance) {
        axis_set_fault(axis, AXIS_FAULT_FOLLOWING_ERROR);
        return true;
    }
    return false;
}

/**
 * @brief Check for software overtravel.
 *
 * @param axis    Axis runtime state
 * @param pos_max Positive soft limit
 * @param pos_min Negative soft limit
 * @return        true if overtravel detected
 */
bool axis_check_soft_overtravel(axis_runtime_t *axis, double pos_max, double pos_min)
{
    if (!axis) return false;

    if (axis->actual_position > pos_max) {
        axis_set_fault(axis, AXIS_FAULT_SOFTWARE_OVERTRAVEL);
        return true;
    }
    if (axis->actual_position < pos_min) {
        axis_set_fault(axis, AXIS_FAULT_SOFTWARE_OVERTRAVEL);
        return true;
    }
    return false;
}

/*===========================================================================
 * L3: Homing sequence FSM
 * Reference: CIP Motion, CiA 402 homing methods
 *===========================================================================*/

/**
 * @brief Homing sequence state context.
 */
typedef struct {
    home_phase_t phase;
    double       fast_speed;         /**< Approach speed */
    double       slow_speed;         /**< Creep speed */
    double       offset;             /**< Home offset */
    home_method_t method;
    double       initial_position;   /**< Position at start of homing */
    bool         switch_found;
    bool         index_found;
    double       index_position;     /**< Position where index was found */
    double       elapsed_time;       /**< Duration tracking */
} homing_context_t;

static homing_context_t g_homing_ctx;

/**
 * @brief Initialize the homing sequence.
 *
 * Sets up the homing context with the specified method and speeds.
 * Called before executing an MAH instruction.
 *
 * @param method        Homing method (see home_method_t)
 * @param fast_speed    Approach speed to switch (eng units/sec)
 * @param slow_speed    Creep speed to index pulse (eng units/sec)
 * @param offset        Final offset from home position
 * @return              0 on success
 */
int homing_init(home_method_t method, double fast_speed,
                double slow_speed, double offset)
{
    /* Validate parameters */
    if (fast_speed <= 0.0 || slow_speed <= 0.0) return -1;
    if (slow_speed >= fast_speed) return -1; /* Creep must be slower */

    memset(&g_homing_ctx, 0, sizeof(g_homing_ctx));
    g_homing_ctx.method = method;
    g_homing_ctx.fast_speed = fast_speed;
    g_homing_ctx.slow_speed = slow_speed;
    g_homing_ctx.offset = offset;
    g_homing_ctx.phase = HOME_PHASE_FAST_APPROACH;

    return 0;
}

/**
 * @brief Process one cycle of the homing sequence.
 *
 * Advances the homing state machine based on position, switch state,
 * and index pulse detection. Computes the homing velocity command.
 *
 * @param current_position  Current axis position (eng units)
 * @param switch_active     Home limit switch active (true = depressed)
 * @param index_pulse       Encoder index pulse detected this cycle
 * @param[out] cmd_velocity Homing velocity command output
 * @return                  Current home phase
 */
home_phase_t homing_step(double current_position, bool switch_active,
                         bool index_pulse, double *cmd_velocity)
{
    if (!cmd_velocity) return HOME_PHASE_FAILED;
    *cmd_velocity = 0.0;

    switch (g_homing_ctx.phase) {
    case HOME_PHASE_IDLE:
        break;

    case HOME_PHASE_FAST_APPROACH:
        /* Move toward home switch at fast speed */
        if (g_homing_ctx.method == HOME_METHOD_SWITCH_NEG) {
            *cmd_velocity = -g_homing_ctx.fast_speed;
        } else {
            *cmd_velocity = g_homing_ctx.fast_speed;
        }

        if (switch_active) {
            g_homing_ctx.switch_found = true;
            g_homing_ctx.phase = HOME_PHASE_SWITCH_DETECT;
            g_homing_ctx.initial_position = current_position;
        }
        break;

    case HOME_PHASE_SWITCH_DETECT:
        /* Reverse direction off the switch, then go to creep */
        if (g_homing_ctx.method == HOME_METHOD_SWITCH_NEG) {
            *cmd_velocity = g_homing_ctx.slow_speed; /* Reverse */
        } else {
            *cmd_velocity = -g_homing_ctx.slow_speed;
        }

        if (!switch_active) {
            /* Off the switch, now looking for index */
            g_homing_ctx.phase = HOME_PHASE_CREEP_TO_INDEX;
        }
        break;

    case HOME_PHASE_CREEP_TO_INDEX:
        /* Continue at creep speed until index pulse */
        if (g_homing_ctx.method == HOME_METHOD_SWITCH_NEG) {
            *cmd_velocity = g_homing_ctx.slow_speed;
        } else {
            *cmd_velocity = -g_homing_ctx.slow_speed;
        }

        if (index_pulse) {
            g_homing_ctx.index_found = true;
            g_homing_ctx.index_position = current_position;
            g_homing_ctx.phase = HOME_PHASE_INDEX_FOUND;
        }
        break;

    case HOME_PHASE_INDEX_FOUND:
        /* Hold position at index, then move to offset */
        *cmd_velocity = 0.0;
        g_homing_ctx.phase = HOME_PHASE_OFFSET_MOVE;
        break;

    case HOME_PHASE_OFFSET_MOVE:
        /* Move to final home offset from index position */
        {
            double target = g_homing_ctx.index_position + g_homing_ctx.offset;
            double error = target - current_position;
            /* Simple P controller for positioning */
            *cmd_velocity = error * 2.0;  /* Kp = 2 1/s */
            if (*cmd_velocity > g_homing_ctx.slow_speed)
                *cmd_velocity = g_homing_ctx.slow_speed;
            if (*cmd_velocity < -g_homing_ctx.slow_speed)
                *cmd_velocity = -g_homing_ctx.slow_speed;

            if (fabs(error) < 0.001) {
                g_homing_ctx.phase = HOME_PHASE_COMPLETE;
                *cmd_velocity = 0.0;
            }
        }
        break;

    case HOME_PHASE_COMPLETE:
    case HOME_PHASE_FAILED:
        *cmd_velocity = 0.0;
        break;
    }

    return g_homing_ctx.phase;
}

/**
 * @brief Check if the homing sequence is complete.
 *
 * @return true if homing has completed
 */
bool homing_is_complete(void)
{
    return g_homing_ctx.phase == HOME_PHASE_COMPLETE;
}

/**
 * @brief Abort an in-progress homing sequence.
 *
 * Resets the homing state machine to idle.
 */
void homing_abort(void)
{
    g_homing_ctx.phase = HOME_PHASE_IDLE;
    g_homing_ctx.switch_found = false;
    g_homing_ctx.index_found = false;
}

/*===========================================================================
 * L6: Classic Problem – Servo Start-Up Sequence
 *
 * The Kinetix servo startup sequence (MSO instruction) involves:
 *   1. DC bus pre-charge check
 *   2. Safe Torque Off verification
 *   3. Motor phase resistance/inductance check (if auto-tune)
 *   4. Commutation alignment (absolute encoder → electrical angle)
 *   5. Current loop enable
 *   6. Velocity/position loop enable
 *===========================================================================*/

/**
 * @brief Simulation of the commutation alignment step.
 *
 * For PM motors with absolute encoders, the electrical angle offset
 * between the encoder zero and the rotor d-axis must be known.
 * This is stored in the drive's non-volatile memory after Motor ID.
 *
 * Given:
 *   encoder_angle_rad: raw mechanical encoder angle
 *   pole_pairs:        number of motor pole pairs
 *   commutation_offset: d-axis offset from encoder zero (rad mechanical)
 *
 * Returns: electrical angle (rad) for FOC.
 *
 * @param encoder_angle_rad    Encoder mechanical angle (rad)
 * @param pole_pairs           Motor pole pairs
 * @param commutation_offset   Calibrated offset (rad mechanical)
 * @return                     Electrical angle (rad, 0 to 2π)
 */
double commutation_compute_electrical_angle(double encoder_angle_rad,
                                             uint32_t pole_pairs,
                                             double commutation_offset)
{
    double mech_angle = encoder_angle_rad + commutation_offset;
    double elec_angle = mech_angle * (double)pole_pairs;
    /* Wrap to [0, 2π) */
    elec_angle = fmod(elec_angle, 2.0 * M_PI);
    if (elec_angle < 0.0) elec_angle += 2.0 * M_PI;
    return elec_angle;
}

/**
 * @brief Safe Torque Off (STO) state machine.
 *
 * Kinetix drives have dual-channel STO inputs. Both channels must
 * be energized (24V) for the drive to produce torque.
 *
 * SIL 3 / PL e per ISO 13849-1 when both channels are used.
 *
 * @param sto_channel1  STO input 1 state (true = 24V present = safe to run)
 * @param sto_channel2  STO input 2 state
 * @param reset_needed  true to attempt reset after STO event
 * @param[out] sto_active true if STO is currently active (torque inhibited)
 * @param[out] fault     Set to true if channel mismatch detected
 */
void sto_state_machine(bool sto_channel1, bool sto_channel2,
                       bool reset_needed,
                       bool *sto_active, bool *fault)
{
    static bool sto_latched = false;
    static bool channel_mismatch_fault = false;

    *fault = false;

    /* Both channels high = safe to enable torque */
    if (sto_channel1 && sto_channel2) {
        if (reset_needed) {
            sto_latched = false;
            channel_mismatch_fault = false;
        }
    }
    /* Either channel low = STO active */
    else {
        sto_latched = true;
    }

    /* Channel mismatch detection (single-channel fault) */
    if (sto_channel1 != sto_channel2) {
        channel_mismatch_fault = true;
        *fault = true;
    }

    *sto_active = sto_latched || channel_mismatch_fault;
}

/*===========================================================================
 * L1-L2: Axis configuration initialization
 *===========================================================================*/

/**
 * @brief Initialize an axis configuration with safe defaults.
 *
 * Uses conservative limits, standard feedback scaling, and
 * safe stopping parameters.
 *
 * @param config  Axis configuration to initialize
 */
void axis_config_init(axis_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(axis_config_t));

    strcpy(config->axis_name, "Axis_Default");
    config->axis_instance = 1;
    config->is_virtual = false;
    config->feedback_type = FEEDBACK_TYPE_ABSOLUTE;
    config->feedback_counts_per_rev = 262144; /* 18-bit absolute */
    config->feedback_noise_filter = 2000.0;   /* 2 kHz LPF */
    config->feedback_loss_window_ms = 5;

    config->position_unit = POSITION_UNIT_DEGREE;
    config->counts_per_unit = 262144.0 / 360.0;
    config->position_unwind = 360.0;
    config->position_rotary = true;

    /* Conservative dynamic limits */
    config->max_velocity = 1000.0;       /* deg/s */
    config->max_acceleration = 5000.0;   /* deg/s² */
    config->max_deceleration = 5000.0;
    config->max_jerk = 0.0;              /* Jerk-limited disabled */
    config->max_torque_pct = 100.0;
    config->max_current_pct = 100.0;

    config->soft_limits_enabled = true;
    config->soft_limit_positive = 1e6;
    config->soft_limit_negative = -1e6;

    config->home_method = HOME_METHOD_SWITCH_NEG;
    config->home_speed_fast = 50.0;      /* deg/s */
    config->home_speed_slow = 5.0;
    config->home_offset = 0.0;

    config->max_stop_deceleration = 10000.0;
    config->max_abort_deceleration = 50000.0;
    config->disable_delay_s = 0.2;
    config->brake_engage_on_disable = false;

    config->pos_error_tolerance = 1.0;
    config->pos_error_fault_action = 2; /* Stop Drive */

    /* Communication defaults */
    config->rpi_us = 2000;              /* 2 ms RPI */
    config->unicast_enabled = false;

    /* Drive defaults – Kinetix 5300 with 480V */
    config->drive_catalog = KINETIX_5300;
    config->bus_voltage_dc = 650.0;     /* 480VAC → ~650VDC */
    config->pwm_frequency_khz = 8.0;
    config->current_loop_bw_hz = 2000.0;

    config->sto_active = false;
    config->sil_level = 3;
}

/**
 * @brief Initialize an axis runtime structure.
 *
 * Sets initial state to DISABLED with zero commands and feedback.
 *
 * @param axis   Axis runtime to initialize
 * @param config Axis configuration (used for scaling, limits)
 */
void axis_runtime_init(axis_runtime_t *axis, const axis_config_t *config)
{
    if (!axis) return;
    memset(axis, 0, sizeof(axis_runtime_t));

    axis->state = AXIS_STATE_DISABLED;
    axis->fault_code = 0;
    axis->servo_active = false;
    axis->at_home = false;
    axis->move_complete = true;
    axis->active_motion_instr = -1;

    /* Apply max values from config */
    if (config) {
        axis->cycle_time_us = config->rpi_us;
    }
}

/**
 * @brief Get a human-readable string for an axis state.
 *
 * @param state  Axis state
 * @return       String representation
 */
const char *axis_state_to_string(axis_state_t state)
{
    switch (state) {
    case AXIS_STATE_DISABLED:     return "DISABLED";
    case AXIS_STATE_STARTING:     return "STARTING";
    case AXIS_STATE_RUNNING:      return "RUNNING";
    case AXIS_STATE_STOPPING:     return "STOPPING";
    case AXIS_STATE_ERROR_STOP:   return "ERROR_STOP";
    case AXIS_STATE_ABORTING:     return "ABORTING";
    case AXIS_STATE_HOMING:       return "HOMING";
    case AXIS_STATE_TESTING:      return "TESTING";
    case AXIS_STATE_AXIS_OFFLINE: return "OFFLINE";
    default:                      return "UNKNOWN";
    }
}
