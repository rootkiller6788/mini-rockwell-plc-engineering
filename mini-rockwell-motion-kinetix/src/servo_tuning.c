/**
 * @file servo_tuning.c
 * @brief Servo loop tuning: cascaded PID, notch filters, auto-tuning, load observer.
 *
 * Level: L4-L6, L8
 * Reference:
 *   - Ellis, "Control System Design Guide" (4th Ed, 2012), Ch.3-6, 13-15
 *   - Åström & Hägglund, "PID Controllers" (1995), Ch.6 (relay auto-tuning)
 *   - Garnado & Qu, "Industrial Servo Control Systems" (2002)
 *   - Hughes & Drury, "Electric Motors and Drives" (2013), Ch.9
 *   - Ziegler & Nichols (1942), "Optimum Settings for Automatic Controllers"
 *
 * Cascaded loop bandwidth hierarchy (Ellis Ch.5-6):
 *
 *   f_current_loop >> f_velocity_loop >> f_position_loop
 *
 *   f_current:  typically 1000-4000 Hz
 *   f_velocity: f_current / (4..10) = 100-500 Hz
 *   f_position: f_velocity / (4..10) = 10-50 Hz
 *
 * Rule: Inner loop BW ≥ 4× outer loop BW for minimal interaction.
 *
 * Alignment: MIT 2.171, Stanford ENGR205, Berkeley ME233, Tsinghua Motion Control
 */

#include "servo_tuning.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*===========================================================================
 * L4: Servo Tuning Initialization
 *===========================================================================*/

/**
 * @brief Default current loop gains based on motor parameters.
 *
 * Using pole-zero cancellation (Ellis Ch.4):
 *   Kp = L * ω_c   where ω_c = 2π * f_c (current loop bandwidth)
 *   Ki = R * ω_c
 *
 * This gives a first-order closed-loop response with time constant 1/ω_c.
 *
 * @param motor     Motor parameters
 * @param[out] curr Current loop gains
 */
static void compute_default_current_gains(const motor_database_t *motor,
                                           current_loop_gains_t *curr)
{
    double bw_rad_s = 2.0 * M_PI * 2000.0;  /* Default: 2 kHz bandwidth */

    double L = motor->inductance_mh * 1e-3;   /* Convert mH → H */
    double R = motor->resistance_ohms;

    if (L < 1e-9) L = 1e-3;  /* Avoid division issues */
    if (R < 1e-9) R = 1.0;

    curr->kp_v_per_a    = L * bw_rad_s;
    curr->ki_v_per_as   = R * bw_rad_s;
    curr->integral_limit = 400.0;   /* ±400V max */
    curr->output_limit   = 400.0;
    curr->deadtime_comp_ns = 500.0; /* 500 ns typical */
    curr->decoupling_gain  = 0.0;
    curr->bus_voltage_comp_factor = 1.0;
}

/**
 * @brief Default velocity loop gains from inertia and target bandwidth.
 *
 * For a plant G(s) = 1 / (J·s + B) ≈ 1 / (J·s) at mid-high frequencies:
 *
 * PI controller C(s) = Kp + Ki/s
 *
 * Open-loop: C(s)·G(s) = (Kp·s + Ki) / (J·s²)
 *
 * Bandwidth ω_bw = Kp / J  (approx, ignoring B)
 * Phase margin ≈ 63° when Ki = Kp · ω_bw / tan(PM_target)
 *
 * Symmetric optimum (for integrator plant):
 *   Kp = J · ω_bw
 *   Ki = J · ω_bw² / a   where a ≈ 2 for PM ≈ 45°
 *
 * Reference: Ellis (2012), Eq. 5.47-5.50
 *
 * @param inertia       Total system inertia (kg·cm²)
 * @param bandwidth_hz  Target velocity loop bandwidth (Hz)
 * @param Kt            Motor torque constant (N·m/A RMS)
 * @param[out] Kp       Velocity proportional gain
 * @param[out] Ki       Velocity integral gain
 */
void servo_compute_velocity_gains(double inertia, double bandwidth_hz,
                                   double Kt, double *Kp, double *Ki)
{
    if (!Kp || !Ki) return;
    if (inertia <= 0.0 || bandwidth_hz <= 0.0 || Kt <= 0.0) {
        *Kp = 0.0;
        *Ki = 0.0;
        return;
    }

    /* Convert inertia from kg·cm² to kg·m² */
    double J_kgm2 = inertia * 1e-4;

    double bw_rad_s = 2.0 * M_PI * bandwidth_hz;

    /* Velocity PI: output is torque (N·m) per velocity error (rad/s) */
    /* Scale by 1/Kt if output is current (A) instead of torque */
    double Kp_torque = J_kgm2 * bw_rad_s;           /* N·m / (rad/s) */
    double Ki_torque = J_kgm2 * bw_rad_s * bw_rad_s / 3.0;  /* N·m / rad */

    *Kp = Kp_torque / Kt;  /* A / (rad/s) — current per velocity error */
    *Ki = Ki_torque / Kt;  /* A / rad — current integral */
}

void servo_tuning_init(servo_tuning_t *tuning, const motor_database_t *motor)
{
    if (!tuning) return;
    memset(tuning, 0, sizeof(servo_tuning_t));

    if (motor) {
        compute_default_current_gains(motor, &tuning->current);
    } else {
        motor_database_t default_motor;
        motor_database_init_default(&default_motor);
        compute_default_current_gains(&default_motor, &tuning->current);
    }

    /* Conservative velocity loop defaults */
    tuning->velocity.kp_vel = 0.1;
    tuning->velocity.ki_vel = 1.0;
    tuning->velocity.integral_limit = 100.0;
    tuning->velocity.output_limit   = 100.0;
    tuning->velocity.velocity_feedback_filter = 500.0; /* 500 Hz LPF */
    tuning->velocity.observer_gain = 0.0;

    /* Conservative position loop defaults */
    tuning->position.kp_pos = 10.0;       /* 10 1/s */
    tuning->position.kd_pos = 0.0;
    tuning->position.velocity_ff_gain = 0.0;
    tuning->position.acceleration_ff_gain = 0.0;
    tuning->position.following_error_limit = 1.0;
    tuning->position.integral_gain = 0.0;
    tuning->position.integral_active_only_in_window = false;
}

/*===========================================================================
 * L4: Notch Filter Design (Bi-Quad)
 *
 * Continuous transfer function:
 *   H(s) = (s² + 2·ζz·ωn·s + ωn²) / (s² + 2·ζp·ωn·s + ωn²)
 *
 * Bilinear (Tustin) discretization without pre-warping:
 *   s ← (2/Ts)·(z-1)/(z+1)
 *
 * Pre-warped: ωn ← (2/Ts)·tan(ωn·Ts/2)
 *
 * Resulting discrete transfer function:
 *   H(z) = (b0 + b1·z⁻¹ + b2·z⁻²) / (1 + a1·z⁻¹ + a2·z⁻²)
 *
 * Reference: Ellis (2012), Section 13.3, Eq. 13.8
 *===========================================================================*/

int notch_filter_design(notch_filter_t *filter,
                        double center_freq,
                        double num_damping,
                        double den_damping,
                        double Ts)
{
    if (!filter) return -1;
    if (center_freq <= 0.0 || Ts <= 0.0) return -1;
    if (center_freq >= 0.48 / Ts) return -1; /* Near Nyquist */

    /* Nyquist check */
    double nyquist = 0.5 / Ts;
    if (center_freq >= nyquist) return -1;

    filter->active = true;
    filter->center_freq_hz = center_freq;
    filter->numerator_damping = num_damping;
    filter->denominator_damping = den_damping;

    /* Pre-warp the notch frequency for Tustin transform */
    double w = 2.0 * M_PI * center_freq;
    double w_warped = (2.0 / Ts) * tan(w * Ts / 2.0);

    double w2 = w_warped * w_warped;

    /* Continuous coefficients normalized: */
    /* numerator: s² + 2*ζz*ωn*s + ωn² */
    /* denominator: s² + 2*ζp*ωn*s + ωn² */
    double num_0 = 1.0;
    double num_1 = 2.0 * num_damping * w_warped;
    double num_2 = w2;

    double den_0 = 1.0;
    double den_1 = 2.0 * den_damping * w_warped;
    double den_2 = w2;

    /* Apply bilinear transform:
       H(z) = H(s) with s = (2/Ts) * (z-1)/(z+1)
       Let K = 2/Ts
       b0*K² + b1*K + b2 → numerator coefficients in z-domain
    */
    double K = 2.0 / Ts;
    double K2 = K * K;

    /* Numerator: K²*(z²-2z+1)/Ts² contribution... 
       Actually, the standard bilinear substitution is:
       Replace s^n with K^n * (z-1)^n / (z+1)^n and multiply through.

       For biquad: H(s) = (b0_s*s² + b1_s*s + b2_s) / (a0_s*s² + a1_s*s + a2_s)
       
       After bilinear: 
       H(z) = (B0 + B1*z⁻¹ + B2*z⁻²) / (A0 + A1*z⁻¹ + A2*z⁻²)
       
       where (with a0_s=1, normalized):
    */
    double B0 = num_0 * K2 + num_1 * K + num_2;
    double B1 = -2.0 * num_0 * K2 + 2.0 * num_2;
    double B2 = num_0 * K2 - num_1 * K + num_2;

    double A0 = den_0 * K2 + den_1 * K + den_2;
    double A1 = -2.0 * den_0 * K2 + 2.0 * den_2;
    double A2 = den_0 * K2 - den_1 * K + den_2;

    /* Normalize so a0 = 1 */
    if (fabs(A0) < 1e-15) return -1;

    filter->b0 = B0 / A0;
    filter->b1 = B1 / A0;
    filter->b2 = B2 / A0;
    filter->a1 = A1 / A0;
    filter->a2 = A2 / A0;

    /* Clear filter state */
    filter->x1 = 0.0;
    filter->x2 = 0.0;
    filter->y1 = 0.0;
    filter->y2 = 0.0;

    /* Compute derived metrics */
    if (num_damping > 0.0) {
        filter->depth_db = 20.0 * log10(num_damping / den_damping);
    } else {
        filter->depth_db = -60.0; /* Deep notch */
    }
    filter->width_hz = center_freq * (den_damping - num_damping);

    return 0;
}

/**
 * @brief Apply the bi-quad notch filter to an input sample.
 *
 * Direct Form II transposed implementation:
 *
 *   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 *
 * @param filter  Notch filter
 * @param input   Current input sample
 * @return        Filtered output
 */
double notch_filter_apply(notch_filter_t *filter, double input)
{
    if (!filter || !filter->active) return input;

    double output = filter->b0 * input
                  + filter->b1 * filter->x1
                  + filter->b2 * filter->x2
                  - filter->a1 * filter->y1
                  - filter->a2 * filter->y2;

    /* Shift state */
    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = output;

    return output;
}

/*===========================================================================
 * L5: Low-Pass Filter
 *===========================================================================*/

void lpf_init(low_pass_filter_t *filter, double cutoff_hz, double Ts)
{
    if (!filter) return;
    filter->cutoff_freq_hz = cutoff_hz;
    filter->prev_output = 0.0;

    if (cutoff_hz > 0.0 && Ts > 0.0) {
        /* Backward Euler: alpha = 2π·fc·Ts, y(k) = (1-α)·y(k-1) + α·x(k) */
        double alpha = 2.0 * M_PI * cutoff_hz * Ts;
        if (alpha > 1.0) alpha = 1.0;  /* Saturation at Nyquist */
        filter->alpha = alpha;
    } else {
        filter->alpha = 1.0;  /* No filtering */
    }
}

double lpf_apply(low_pass_filter_t *filter, double input)
{
    if (!filter) return input;
    filter->prev_output = (1.0 - filter->alpha) * filter->prev_output
                        + filter->alpha * input;
    return filter->prev_output;
}

/*===========================================================================
 * L5: Luenberger Load Observer
 *
 * Physical model:
 *   dω/dt = (Te - Tl - B·ω) / J
 *
 * where Te = electromagnetic torque, Tl = load torque, B = viscous friction.
 *
 * Observer (estimates ω̂ and disturbance d̂):
 *   dω̂/dt = (Te - d̂ - B·ω̂) / J + L1 · (ω - ω̂)
 *   dd̂/dt = L2 · (ω - ω̂)
 *
 * where L1, L2 are observer gains.
 *
 * For pole placement at ω_obs (observer bandwidth):
 *   L1 = 2·ω_obs
 *   L2 = J · ω_obs²
 *
 * Reference: Ellis (2012), Section 14.2
 *===========================================================================*/

void load_observer_init(load_observer_t *obs, double inertia,
                        double friction, double observer_gain, double Ts)
{
    if (!obs) return;
    memset(obs, 0, sizeof(load_observer_t));

    obs->inertia_kgcm2 = inertia;
    obs->viscous_friction = friction;
    obs->observer_gain_L = observer_gain;
    obs->active = (observer_gain > 0.0);
    (void)Ts; /* Reserved for discrete-time observer design */
}

double load_observer_update(load_observer_t *obs, double torque,
                            double measured_vel, double Ts)
{
    if (!obs || !obs->active) return measured_vel;

    /* Convert inertia to kg·m² */
    double J = obs->inertia_kgcm2 * 1e-4;
    double B = obs->viscous_friction;

    if (J < 1e-9) return measured_vel;

    /* Observer error */
    double vel_error = measured_vel - obs->estimated_velocity;

    /* Observer dynamics (Euler integration) */
    double L1 = 2.0 * obs->observer_gain_L;
    double L2 = J * obs->observer_gain_L * obs->observer_gain_L;

    /* dω̂/dt */
    double dw_dt = (torque - obs->estimated_disturbance - B * obs->estimated_velocity) / J
                   + L1 * vel_error;

    /* d(d̂)/dt */
    double dd_dt = L2 * vel_error;

    /* Integrate */
    obs->estimated_velocity     += dw_dt * Ts;
    obs->estimated_disturbance  += dd_dt * Ts;

    /* Convergence detection */
    if (fabs(vel_error) < 0.01 && fabs(dw_dt) < 1.0) {
        obs->converged = true;
    }

    return obs->estimated_velocity;
}

/*===========================================================================
 * L4-L5: Åström-Hägglund Relay Feedback Auto-Tuning
 *
 * The relay method identifies the ultimate gain (Ku) and ultimate period (Pu)
 * of the velocity loop by inducing a limit cycle oscillation.
 *
 * Process:
 *   1. Replace PID with relay (bang-bang) controller
 *   2. Relay output = ±d when error crosses zero
 *   3. System oscillates at ω_180 (frequency where phase = -180°)
 *   4. Measure oscillation period Pu and amplitude a
 *   5. Compute ultimate gain: Ku = 4d / (πa)
 *
 * Then Ziegler-Nichols PI tuning from Ku, Pu:
 *   Kp = 0.45 * Ku
 *   Ki = Kp / (Pu / 1.2) = 0.54 * Ku / Pu
 *
 * Reference: Åström & Hägglund (1984), "Automatic Tuning of Simple Regulators"
 *            IEEE Control Systems Magazine, 8(4), 50-55
 *===========================================================================*/

/**
 * @brief Relay auto-tuning internal state.
 */
typedef struct {
    bool     measuring;              /**< True when oscillation detected */
    double   prev_error;             /**< Previous error for zero-crossing */
    double   prev_output;            /**< Previous relay output */
    int      half_cycle_count;       /**< Count of half-cycles */
    double   cycle_start_time;       /**< Time of most recent zero-crossing */
    double   last_half_period;       /**< Duration of last half-cycle */
    double   peak_amplitude;         /**< Peak oscillation amplitude */
    double   trough_amplitude;       /**< Trough oscillation amplitude */
    double   period_accumulator;     /**< Sum of periods for averaging */
    double   amplitude_accumulator;  /**< Sum of amplitudes for averaging */
    int      cycle_count;            /**< Number of full cycles measured */
    /* Averaged results */
    double   measured_Pu;            /**< Measured ultimate period */
    double   measured_Ku;            /**< Measured ultimate gain */
} relay_autotune_state_t;

static relay_autotune_state_t g_relay_state;

double autotune_relay_step(autotune_state_t *state, double vel_error,
                           double relay_amplitude, double Ts)
{
    if (!state || relay_amplitude <= 0.0) return 0.0;

    double output;

    /* Relay with hysteresis */
    double hysteresis = relay_amplitude * 0.05; /* 5% hysteresis */

    /* Detect zero crossing */
    bool zero_crossing = false;

    if (vel_error > hysteresis && g_relay_state.prev_error <= hysteresis) {
        zero_crossing = true; /* Positive crossing */
    } else if (vel_error < -hysteresis && g_relay_state.prev_error >= -hysteresis) {
        zero_crossing = true; /* Negative crossing */
    }

    /* Relay output */
    if (vel_error > 0.0) {
        output = relay_amplitude;
    } else {
        output = -relay_amplitude;
    }

    /* On zero crossing, measure half-period */
    if (zero_crossing && g_relay_state.cycle_start_time > 0.0) {
        double current_time = g_relay_state.cycle_start_time + Ts; /* Approx */
        g_relay_state.last_half_period = current_time - g_relay_state.cycle_start_time;

        if (g_relay_state.half_cycle_count > 1) {
            /* Skip first half-cycle (transient) */
            g_relay_state.period_accumulator += g_relay_state.last_half_period * 2.0;
            g_relay_state.cycle_count++;

            double amplitude = (g_relay_state.peak_amplitude - g_relay_state.trough_amplitude) / 2.0;
            g_relay_state.amplitude_accumulator += amplitude;
        }

        g_relay_state.cycle_start_time = current_time;
        g_relay_state.half_cycle_count++;
        g_relay_state.peak_amplitude = vel_error;
        g_relay_state.trough_amplitude = vel_error;
    }

    /* Track peak/trough */
    if (vel_error > g_relay_state.peak_amplitude)
        g_relay_state.peak_amplitude = vel_error;
    if (vel_error < g_relay_state.trough_amplitude)
        g_relay_state.trough_amplitude = vel_error;

    g_relay_state.prev_error = vel_error;
    g_relay_state.prev_output = output;

    /* After sufficient cycles, compute Ku and Pu */
    if (g_relay_state.cycle_count >= 5) {
        double avg_Pu = g_relay_state.period_accumulator / g_relay_state.cycle_count;
        double avg_amplitude = g_relay_state.amplitude_accumulator / g_relay_state.cycle_count;

        if (avg_amplitude > 1e-9) {
            state->ultimate_period_Pu = avg_Pu;
            /* Ku = 4d / (π·a) — Åström-Hägglund formula */
            state->ultimate_gain_Ku = (4.0 * relay_amplitude) / (M_PI * avg_amplitude);

            state->tuning_in_progress = false;
            state->tuning_success = true;
            state->progress_pct = 100.0;
        }
    }

    return output;
}

/*===========================================================================
 * L5: Load Inertia Identification
 *
 * Inertia identified from velocity ramp:
 *
 * J = ΔT / Δω  where ΔT is torque change, Δω is acceleration
 *
 * Using least-squares fit over the acceleration phase:
 *   minimize Σ [T(k) - J·α(k) - Tf·sign(ω(k))]²
 *
 * where α(k) = (ω(k) - ω(k-1)) / Ts
 *
 * This assumes constant acceleration phase (known timing from motion planner).
 *
 * Reference: Ellis (2012), Section 13.1
 *===========================================================================*/

double autotune_identify_inertia(autotune_state_t *state,
                                  const double *torque_samples,
                                  const double *velocity_samples,
                                  size_t num_samples, double Ts)
{
    if (!torque_samples || !velocity_samples || num_samples < 3 || Ts <= 0.0) {
        return -1.0;
    }

    /* Compute acceleration from velocity differences */
    double sum_T = 0.0, sum_alpha = 0.0, sum_T_alpha = 0.0, sum_alpha2 = 0.0;
    size_t n = 0;

    for (size_t k = 1; k < num_samples; k++) {
        double alpha = (velocity_samples[k] - velocity_samples[k - 1]) / Ts;

        /* Only use samples where we're actually accelerating */
        if (fabs(alpha) < 0.01) continue;

        double T = torque_samples[k];

        sum_T      += T;
        sum_alpha  += alpha;
        sum_T_alpha += T * alpha;
        sum_alpha2 += alpha * alpha;
        n++;
    }

    if (n < 2) return -1.0;

    /* Least squares: J = Σ(T·α) / Σ(α²)  (assuming zero intercept) */
    double J_kgm2 = sum_T_alpha / sum_alpha2;
    if (J_kgm2 < 1e-9) return -1.0;

    /* Convert to kg·cm² */
    double J_kgcm2 = J_kgm2 * 1e4;

    state->identified_J_kgcm2 = J_kgcm2;
    state->identified_B = 0.0;  /* Simplified – neglect viscous friction in estimate */

    return J_kgcm2;
}

/*===========================================================================
 * L5: Compute Recommended Servo Gains from Identified Parameters
 *
 * Given identified inertia and motor parameters, compute gains for
 * all three cascaded loops using bandwidth hierarchy rules.
 *===========================================================================*/

int autotune_compute_gains(autotune_state_t *state,
                           const motor_database_t *motor)
{
    if (!state || !motor) return -1;

    double J_total = state->identified_J_kgcm2;
    if (J_total <= 0.0) J_total = motor->rotor_inertia_kgcm2;

    /* Target bandwidths based on identified inertia ratio */
    double inertia_ratio = J_total / motor->rotor_inertia_kgcm2;

    /* Higher inertia → lower bandwidth for stability */
    double vel_bw_hz, pos_bw_hz;

    if (inertia_ratio <= 3.0) {
        vel_bw_hz = 300.0;  /* High-dynamic: 300 Hz velocity loop */
    } else if (inertia_ratio <= 10.0) {
        vel_bw_hz = 150.0;  /* Medium inertia */
    } else {
        vel_bw_hz = 50.0;   /* High inertia – conservative */
    }

    pos_bw_hz = vel_bw_hz / 8.0;  /* Position loop BW ≈ Vel BW / 8 */

    state->estimated_vel_bw_hz = vel_bw_hz;
    state->estimated_pos_bw_hz = pos_bw_hz;

    /* Current loop: use motor parameters */
    servo_tuning_init(&state->recommended_gains, motor);

    /* Velocity loop: use identified inertia */
    servo_compute_velocity_gains(J_total, vel_bw_hz, motor->torque_constant_kt,
                                  &state->recommended_gains.velocity.kp_vel,
                                  &state->recommended_gains.velocity.ki_vel);

    /* Position loop */
    state->recommended_gains.position.kp_pos = pos_bw_hz * 2.0 * M_PI;
    state->recommended_gains.position.velocity_ff_gain = 0.7;

    /* Phase margin estimate (simplified) */
    state->estimated_phase_margin = 45.0 + 20.0 * (1.0 / (1.0 + inertia_ratio));
    state->estimated_gain_margin = 10.0;  /* Conservative default */

    return 0;
}

/*===========================================================================
 * L5: Friction Compensation
 *
 * Model: T_friction = Tc · tanh(ω/ε) + B · ω
 *   where tanh(ω/ε) provides smooth signum approximation
 *   ε = velocity_smooth_width (small → sharper transition)
 *
 * Stiction (breakaway friction) is handled separately:
 *   If |ω| < threshold and |cmd_torque| > Ts → add Ts boost
 *
 * Reference: Armstrong-Hélouvry, Dupont, Canudas de Wit (1994),
 *            "A Survey of Models, Analysis Tools and Compensation Methods
 *             for the Control of Machines with Friction", Automatica, 30(7)
 *===========================================================================*/

double friction_compensation_compute(const friction_comp_t *comp, double velocity)
{
    if (!comp || !comp->enabled) return 0.0;

    double Tf = 0.0;

    /* Viscous friction */
    Tf += comp->viscous_nm_per_krpm * (velocity / 1000.0);

    /* Coulomb friction with smooth sign */
    double eps = comp->smooth_width;
    if (eps < 1e-9) eps = 1e-3;

    Tf += comp->coulomb_nm * tanh(velocity / eps);

    return Tf;
}

/*===========================================================================
 * L8: Adaptive Gain Scheduling
 *
 * Adjusts servo gains based on:
 *   1. Inertia ratio change (e.g., robot picks up a payload)
 *   2. Position-dependent inertia (e.g., SCARA robot arm extension)
 *
 * Gain scaling law:
 *   Kp_scaled = Kp_nominal · inertia_ratio^γ
 *   where γ ≈ 0.5 for velocity loop (bandwidth ∝ √Kp)
 *
 * Reference: Ellis (2012), Ch.15 – Adaptive Control for Motion Systems
 *===========================================================================*/

void adaptive_gain_init(adaptive_gain_t *adaptive)
{
    if (!adaptive) return;
    memset(adaptive, 0, sizeof(adaptive_gain_t));

    adaptive->enabled = false;
    adaptive->inertia_ratio = 1.0;
    adaptive->scheduled_kp_scale = 1.0;
    adaptive->scheduled_kv_scale = 1.0;
    adaptive->inertia_filter_tau = 0.05;  /* 50 ms filter */
    adaptive->max_gain_scale = 3.0;
    adaptive->min_gain_scale = 0.2;
}

double adaptive_gain_compute(const adaptive_gain_t *adaptive,
                             double inertia_ratio, double position)
{
    if (!adaptive || !adaptive->enabled) return 1.0;

    double scale;

    /* Inertia-based scaling */
    if (inertia_ratio > 0.0) {
        scale = sqrt(inertia_ratio);
    } else {
        scale = 1.0;
    }

    /* Position-dependent scaling (linear interpolation in table) */
    for (int i = 0; i < 9; i++) {
        if (position >= adaptive->pos_dependent_positions[i] &&
            position < adaptive->pos_dependent_positions[i + 1]) {
            double frac = (position - adaptive->pos_dependent_positions[i])
                        / (adaptive->pos_dependent_positions[i + 1]
                         - adaptive->pos_dependent_positions[i]);
            double pos_scale = adaptive->pos_dependent_table[i] * (1.0 - frac)
                             + adaptive->pos_dependent_table[i + 1] * frac;
            scale *= pos_scale;
            break;
        }
    }

    /* Clamp to allowed range */
    if (scale > adaptive->max_gain_scale) scale = adaptive->max_gain_scale;
    if (scale < adaptive->min_gain_scale) scale = adaptive->min_gain_scale;

    return scale;
}
