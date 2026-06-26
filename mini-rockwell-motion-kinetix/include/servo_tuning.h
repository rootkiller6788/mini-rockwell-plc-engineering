/**
 * @file servo_tuning.h
 * @brief Servo loop tuning, auto-tuning, filter design, and load observer.
 *
 * Level: L4 Engineering Laws + L5 Algorithms
 * Reference:
 *   - Ellis, "Control System Design Guide" (4th Ed, 2012), Ch.3-6, 13-15
 *   - Astrom & Hagglund, "PID Controllers" (1995), Ch.6 (auto-tuning)
 *   - Hughes & Drury, "Electric Motors and Drives" (2013), Ch.9 (servo drives)
 *   - Garnado & Qu, "Industrial Servo Control Systems" (2002)
 *   - Rockwell Automation, "Kinetix Motion Control Selection Guide" (KNX-SG001)
 *
 * Kinetix cascaded control loop structure:
 *
 *   Position Cmd → [Pos P] → Vel Cmd → [Vel PI] → Torque Cmd → [Iq PI] → Vq
 *                       ↑                      ↑                    ↑
 *                   Pos FB                Vel FB               Current FB
 *                                     (diff pos)             (phase sense)
 *
 * Implements:
 *   L4: Cascaded servo loop model, bandwidth hierarchy rules
 *   L4: Notch filter design (bi-quad), low-pass filter
 *   L4: Ziegler-Nichols relay feedback auto-tuning (Åström-Hägglund method)
 *   L5: Load inertia identification, friction compensation
 *   L5: Adaptive gain scheduling, anti-resonance filters
 *
 * Alignment: MIT 2.171, Berkeley ME233, Stanford ENGR205, Tsinghua Motion Control
 */

#ifndef SERVO_TUNING_H
#define SERVO_TUNING_H

#include "axis_types.h"
#include <stdbool.h>

/*===========================================================================
 * L1: Definitions – Cascaded Servo Loop Gains
 *===========================================================================*/

/**
 * @brief Current loop PI controller parameters.
 *
 * The innermost loop – regulates motor phase currents.
 * Typically the fastest loop: bandwidth = 1-4 kHz for Kinetix drives.
 *
 * id_ref is normally zero (no field weakening for PM motors).
 * iq_ref is the torque-producing current.
 */
typedef struct {
    double   kp_v_per_a;              /**< Proportional gain (V/A) */
    double   ki_v_per_as;             /**< Integral gain (V/A·s) */
    double   integral_limit;          /**< Integrator anti-windup limit (V) */
    double   output_limit;            /**< Output voltage limit (V) */
    double   deadtime_comp_ns;        /**< Deadtime compensation (ns) */
    double   decoupling_gain;         /**< Cross-coupling decoupling gain */
    double   bus_voltage_comp_factor; /**< Bus voltage compensation factor */
} current_loop_gains_t;

/**
 * @brief Velocity loop PI controller parameters.
 *
 * The middle loop – regulates motor shaft velocity.
 * Velocity feedback is derived from position feedback (difference + filter).
 * Typical bandwidth: 100-500 Hz.
 *
 * Theorem (Ellis Ch.5): For minimal settling time, velocity loop bandwidth
 * should be 4-10× lower than current loop bandwidth (cascaded rule).
 */
typedef struct {
    double   kp_vel;                  /**< Velocity proportional gain (N·m / rad/s or A / rad/s) */
    double   ki_vel;                  /**< Velocity integral gain (N·m / rad or A / rad) */
    double   integral_limit;          /**< Integral anti-windup limit */
    double   output_limit;            /**< Torque/current output limit */
    double   velocity_feedback_filter;/**< Velocity measurement low-pass filter (Hz) */
    double   observer_gain;           /**< Luenberger observer gain (0 = disabled) */
} velocity_loop_gains_t;

/**
 * @brief Position loop P controller parameters.
 *
 * The outermost loop – regulates motor shaft position.
 * Typically P-only (no integral, to prevent overshoot).
 * Integral action for steady-state error comes from the velocity loop integrator.
 *
 * Theorem (Ellis Ch.6): Position loop bandwidth should be 4-10× lower
 * than velocity loop bandwidth.
 *
 * Bandwidth hierarchy:
 *   f_current > f_velocity × (4..10) > f_position × (4..10)
 *
 * Typical values for Kinetix:
 *   f_current = 2000 Hz, f_velocity = 200 Hz, f_position = 50 Hz
 */
typedef struct {
    double   kp_pos;                  /**< Position proportional gain (rad/s per rad or 1/s) */
    double   kd_pos;                  /**< Position derivative gain (velocity FF weight) */
    double   velocity_ff_gain;        /**< Velocity feedforward gain (0-1) */
    double   acceleration_ff_gain;    /**< Acceleration feedforward gain (0-1) */
    double   following_error_limit;   /**< Following error fault threshold (eng units) */
    double   integral_gain;           /**< Position integral gain (optional, 1/s²) */
    double   integral_saturation;     /**< Integral saturation limit */
    bool     integral_active_only_in_window; /**< Only integrate when error < window */
    double   integral_window;         /**< Integration window (eng units) */
} position_loop_gains_t;

/**
 * @brief Complete cascaded servo loop tuning parameters.
 *
 * This structure holds all three loop gains for a Kinetix axis.
 */
typedef struct {
    current_loop_gains_t   current;
    velocity_loop_gains_t  velocity;
    position_loop_gains_t  position;
} servo_tuning_t;

/*===========================================================================
 * L4: Engineering Laws – Notch Filters and Observers
 *===========================================================================*/

/**
 * @brief Bi-quadratic IIR notch filter for mechanical resonance suppression.
 *
 * Kinetix drives support up to 4 notch filters in series to suppress
 * mechanical resonances in the load/motor system.
 *
 * Transfer function (continuous):
 *   H(s) = (s² + 2·ζz·ω0·s + ω0²) / (s² + 2·ζp·ω0·s + ω0²)
 *
 * Discretized via Tustin (bilinear) transform for servo-loop execution.
 *
 * Reference: Ellis (2012), Section 13.3 – Notch Filters
 */
typedef struct {
    bool     active;                  /**< Filter enabled */
    double   center_freq_hz;          /**< Center/notch frequency (Hz) */
    double   numerator_damping;       /**< Damping ratio ζz for zeros (depth of notch) */
    double   denominator_damping;     /**< Damping ratio ζp for poles (width of notch) */
    double   depth_db;                /**< Notch depth (dB, read-only computed from ζz) */
    double   width_hz;                /**< -3dB bandwidth (Hz, read-only computed from ζp) */
    /* Discrete filter coefficients (pre-computed for efficiency) */
    double   b0, b1, b2;              /**< Numerator coefficients */
    double   a1, a2;                  /**< Denominator coefficients (a0=1) */
    /* Filter state */
    double   x1, x2;                  /**< Input history */
    double   y1, y2;                  /**< Output history */
} notch_filter_t;

/**
 * @brief First-order low-pass filter for velocity/acceleration estimation.
 *
 * Used to smooth noisy velocity feedback from differentiated position.
 *
 * Discrete transfer function (Backward Euler):
 *   y(k) = (1 - α)·y(k-1) + α·x(k)
 *   α = 2π·f3db·Ts
 */
typedef struct {
    double   cutoff_freq_hz;          /**< -3dB cutoff frequency (Hz) */
    double   alpha;                   /**< Filter coefficient */
    double   prev_output;             /**< Previous output y(k-1) */
} low_pass_filter_t;

/**
 * @brief Luenberger load observer for velocity estimation with noise rejection.
 *
 * The observer uses the plant model (J, B) and measured torque to produce
 * a filtered velocity estimate superior to simple differentiation.
 *
 * State-space model:
 *   dω/dt = (Te - B·ω) / J
 *
 * Observer:
 *   dω̂/dt = (Te - B·ω̂) / J + L·(ω_meas - ω̂)
 *
 * Reference: Ellis (2012), Section 14.2 – Luenberger Observer for Motion Systems
 */
typedef struct {
    bool     active;
    double   observer_gain_L;         /**< Observer gain L (rad/s per rad/s) */
    double   inertia_kgcm2;           /**< Total system inertia (motor + load) */
    double   viscous_friction;        /**< Viscous friction coefficient */
    double   estimated_velocity;      /**< Observer velocity output */
    double   estimated_disturbance;   /**< Estimated load torque disturbance (N·m) */
    double   prev_torque;             /**< Previous torque input */
    bool     converged;               /**< True when observer has converged */
} load_observer_t;

/*===========================================================================
 * L5: Algorithms – Auto-Tuning Methods
 *===========================================================================*/

/**
 * @brief Auto-tuning excitation mode for motor identification.
 */
typedef enum {
    TUNE_MODE_NONE        = 0,  /**< No active tuning */
    TUNE_MODE_CURRENT     = 1,  /**< Identify R, L (inject voltage pulses, measure current) */
    TUNE_MODE_INERTIA     = 2,  /**< Identify J, B (velocity ramp, measure torque) */
    TUNE_MODE_VELOCITY    = 3,  /**< Tune velocity loop (relay or step response) */
    TUNE_MODE_POSITION    = 4,  /**< Tune position loop (from velocity loop model) */
    TUNE_MODE_COMPLETE    = 5   /**< All tuning steps complete */
} tune_mode_t;

/**
 * @brief Auto-tuning state machine.
 *
 * Kinetix auto-tuning (also called "Load Observer Auto-Tuning"
 * or "Autotune" in Studio 5000) identifies motor/load parameters
 * and computes optimal servo gains.
 */
typedef struct {
    tune_mode_t mode;                /**< Current tuning step */
    bool        tuning_in_progress;  /**< True during active excitation */
    bool        tuning_success;      /**< Tuning completed successfully */
    uint16_t    tuning_fault;        /**< Tuning fault code (0 = none) */
    double      progress_pct;        /**< Tuning progress (0-100%) */

    /* Identified motor electrical parameters */
    double      identified_R_ohm;    /**< Phase resistance (Ω) */
    double      identified_L_mh;     /**< Phase inductance (mH) */
    double      identified_Kt;       /**< Torque constant Kt (N·m/A RMS) */
    double      identified_Kv;       /**< Back-EMF constant Kv (Vpeak/kRPM) */
    double      identified_flux_Wb;  /**< Permanent magnet flux linkage (Wb) */

    /* Identified mechanical parameters */
    double      identified_J_kgcm2;  /**< Total inertia (motor + load) */
    double      identified_B;        /**< Viscous friction coefficient */
    double      identified_Tc_Nm;    /**< Coulomb friction torque (N·m) */
    double      identified_Ts_Nm;    /**+ Stiction torque (static friction, N·m) */

    /* Identified load dynamics */
    double      identified_compliance;     /**< Load compliance 1/k (rad/N·m) */
    double      identified_resonance_hz;   /**< Primary resonance frequency (Hz) */
    double      identified_backlash_deg;   /**< Estimated backlash (degrees) */

    /* Computed gain recommendations */
    servo_tuning_t recommended_gains; /**< Complete recommended servo gains */

    /* Relay auto-tuning data (Åström-Hägglund method) */
    double      relay_amplitude;      /**< Relay excitation amplitude */
    double      ultimate_gain_Ku;     /**< Ultimate gain from relay test */
    double      ultimate_period_Pu;   /**< Ultimate period (seconds) */

    /* Performance estimate */
    double      estimated_vel_bw_hz;  /**< Estimated velocity loop bandwidth (Hz) */
    double      estimated_pos_bw_hz;  /**< Estimated position loop bandwidth (Hz) */
    double      estimated_phase_margin; /**< Estimated phase margin (degrees) */
    double      estimated_gain_margin; /**< Estimated gain margin (dB) */
} autotune_state_t;

/*===========================================================================
 * L5: Algorithms – Friction Compensation
 *===========================================================================*/

/**
 * @brief Friction compensation model.
 *
 * Friction in servo systems causes quadrant glitches (reversal errors)
 * and steady-state errors. Kinetix drives apply a friction feedforward
 * based on velocity direction and magnitude.
 *
 * Model: T_friction = Tc * sign(ω) + B * ω + Ts(if |ω| ≈ 0)
 *   where Tc = Coulomb friction, B = viscous friction, Ts = stiction
 */
typedef struct {
    bool     enabled;                 /**< Friction compensation active */
    double   coulomb_nm;              /**< Coulomb friction torque */
    double   viscous_nm_per_krpm;     /**< Viscous friction coefficient */
    double   stiction_nm;             /**< Stiction (breakaway) torque */
    double   velocity_threshold;      /**< Threshold for stiction detection */
    double   smooth_width;            /**< tanh smoothing width for sign function */
} friction_comp_t;

/*===========================================================================
 * L5: Algorithms – Adaptive Gain Scheduling
 *===========================================================================*/

/**
 * @brief Adaptive gain scheduling parameters.
 *
 * Adjusts servo gains based on load inertia changes or position
 * to maintain consistent performance across the operating envelope.
 */
typedef struct {
    bool     enabled;
    double   inertia_ratio;           /**< Current estimated inertia ratio J/J_motor */
    double   scheduled_kp_scale;      /**< Kp scaling factor from inertia ratio */
    double   scheduled_kv_scale;      /**< Kv scaling factor from inertia ratio */
    double   inertia_filter_tau;      /**< Inertia estimation low-pass time constant */
    double   max_gain_scale;          /**< Maximum allowed gain scale */
    double   min_gain_scale;          /**< Minimum allowed gain scale */
    double   pos_dependent_table[10]; /**< Position-dependent gain map (sampled) */
    double   pos_dependent_positions[10]; /**< Position values for gain map */
} adaptive_gain_t;

/*===========================================================================
 * L5: Algorithms – Servo Tuning API
 *===========================================================================*/

/**
 * @brief Initialize servo tuning to safe defaults.
 *
 * Sets conservative gains safe for initial motor start-up.
 * Current loop: Kp = L·fc·2π, Ki = R·fc·2π  (pole-zero cancellation)
 * Velocity loop: Kp based on J and target bandwidth
 * Position loop: Kp = ω_vel_bw / 4  (rule of thumb)
 *
 * @param tuning    Servo tuning struct to initialize
 * @param motor     Motor database entry for default gain computation
 */
void servo_tuning_init(servo_tuning_t *tuning, const motor_database_t *motor);

/**
 * @brief Design a bi-quad notch filter.
 *
 * Computes the digital filter coefficients for a notch filter at the
 * specified center frequency, using Tustin (bilinear) discretization
 * with pre-warping at the notch frequency.
 *
 * Reference: Ellis (2012), Eq. 13.6-13.8
 *
 * @param filter        Notch filter to configure
 * @param center_freq   Center (notch) frequency (Hz)
 * @param num_damping   Numerator damping ratio ζz (0 < ζz << 1 = deep notch)
 * @param den_damping   Denominator damping ratio ζp (ζp > ζz for wider notch)
 * @param Ts            Sample time (seconds)
 * @return              0 on success, -1 if parameters invalid
 */
int notch_filter_design(notch_filter_t *filter,
                        double center_freq,
                        double num_damping,
                        double den_damping,
                        double Ts);

/**
 * @brief Process a sample through the notch filter.
 *
 * @param filter    Notch filter
 * @param input     Current input sample
 * @return          Filtered output
 */
double notch_filter_apply(notch_filter_t *filter, double input);

/**
 * @brief Initialize a low-pass filter.
 *
 * @param filter    Filter to initialize
 * @param cutoff_hz -3dB cutoff frequency (Hz)
 * @param Ts        Sample time (seconds)
 */
void lpf_init(low_pass_filter_t *filter, double cutoff_hz, double Ts);

/**
 * @brief Apply the low-pass filter to a sample.
 *
 * @param filter    Filter
 * @param input     Current sample value
 * @return          Filtered value
 */
double lpf_apply(low_pass_filter_t *filter, double input);

/**
 * @brief Initialize the Luenberger load observer.
 *
 * @param obs           Observer to initialize
 * @param inertia       Total system inertia (kg·cm²)
 * @param friction      Viscous friction coefficient
 * @param observer_gain Observer gain L
 * @param Ts            Sample time (s)
 */
void load_observer_init(load_observer_t *obs, double inertia,
                        double friction, double observer_gain, double Ts);

/**
 * @brief Update the load observer with new measurements.
 *
 * @param obs           Observer state
 * @param torque        Applied motor torque (N·m)
 * @param measured_vel  Measured velocity from feedback
 * @param Ts            Sample time (s)
 * @return              Estimated (filtered) velocity
 */
double load_observer_update(load_observer_t *obs, double torque,
                            double measured_vel, double Ts);

/**
 * @brief Run Åström-Hägglund relay feedback auto-tuning for velocity loop.
 *
 * Injects a relay (bang-bang) signal at the velocity error to identify
 * the ultimate gain Ku and ultimate period Pu of the velocity loop.
 * From these, PI gains are computed using Ziegler-Nichols rules.
 *
 * Reference: Åström & Hägglund (1984), "Automatic Tuning of Simple Regulators"
 *
 * @param state         Autotune state machine
 * @param vel_error     Current velocity error (eng units/sec)
 * @param relay_amplitude Amplitude of relay output
 * @param Ts            Sample time (s)
 * @return              Relay output value (±relay_amplitude)
 */
double autotune_relay_step(autotune_state_t *state, double vel_error,
                           double relay_amplitude, double Ts);

/**
 * @brief Identify load inertia from a trapezoidal velocity profile.
 *
 * Runs a velocity ramp and measures the torque required to accelerate
 * the load. Inertia is computed as J = ΔT / Δω.
 *
 * @param state             Autotune state
 * @param torque_samples    Array of torque measurements during ramp
 * @param velocity_samples  Array of velocity measurements
 * @param num_samples       Number of samples
 * @param Ts                Sample time (s)
 * @return                  Identified inertia in kg·cm², negative on error
 */
double autotune_identify_inertia(autotune_state_t *state,
                                  const double *torque_samples,
                                  const double *velocity_samples,
                                  size_t num_samples, double Ts);

/**
 * @brief Compute recommended servo gains from identified parameters.
 *
 * Based on the identified inertia and motor parameters, computes
 * optimal gains for all three cascaded loops.
 *
 * @param state   Autotune state with identified parameters
 * @param motor   Motor database entry
 * @return        0 on success
 */
int autotune_compute_gains(autotune_state_t *state,
                           const motor_database_t *motor);

/**
 * @brief Compute friction compensation torque.
 *
 * @param comp      Friction compensation parameters
 * @param velocity  Current velocity (eng units/sec)
 * @return          Friction compensation torque (N·m)
 */
double friction_compensation_compute(const friction_comp_t *comp, double velocity);

/**
 * @brief Compute adaptive gain scale based on current inertia estimate.
 *
 * @param adaptive      Adaptive gain parameters
 * @param inertia_ratio Current estimated inertia ratio
 * @param position      Current axis position (for position-dependent gain)
 * @return              Gain scale factor (1.0 = nominal)
 */
double adaptive_gain_compute(const adaptive_gain_t *adaptive,
                             double inertia_ratio, double position);

/**
 * @brief Initialize adaptive gain structure.
 *
 * @param adaptive   Adaptive gain struct to initialize
 */
void adaptive_gain_init(adaptive_gain_t *adaptive);

/**
 * @brief Compute velocity PI gains from desired bandwidth.
 *
 * Uses the symmetric optimum method for a PI controller on a
 * first-order system (J·s + B).
 *
 *   Kp = J*ω_bw   (or J*ω_bw/Kt if current loop output = current)
 *   Ki = Kp * ω_bw / 4  (for 45° phase margin)
 *
 * @param inertia       Total inertia (kg·cm²)
 * @param bandwidth_hz  Desired velocity loop bandwidth (Hz)
 * @param Kt            Motor torque constant (N·m/A RMS)
 * @param[out] Kp       Computed proportional gain
 * @param[out] Ki       Computed integral gain
 */
void servo_compute_velocity_gains(double inertia, double bandwidth_hz,
                                   double Kt, double *Kp, double *Ki);

#endif /* SERVO_TUNING_H */
