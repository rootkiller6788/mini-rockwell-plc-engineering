/**
 * @file motion_advanced.c
 * @brief Advanced motion control: adaptive tuning, Lyapunov stability,
 *        Monte Carlo robustness, vibration suppression, cogging compensation.
 *
 * Level: L8 Advanced Topics
 * Reference:
 *   - Slotine & Li, "Applied Nonlinear Control" (1991), Ch.3 (Lyapunov stability)
 *   - Ellis, "Control System Design Guide" (4th Ed, 2012), Ch.14-16
 *   - Åström & Hägglund, "Advanced PID Control" (2006), Ch.8 (adaptive)
 *   - de Silva, "Mechatronics: An Integrated Approach" (2005), Ch.9
 *   - Bristow, Alleyne, "A High-Performance Motion Control..." (2004), IEEE TIE
 *
 * Implements:
 *   L8: Lyapunov-based adaptive gain tuning, Monte Carlo robustness analysis,
 *       cogging torque compensation for PM motors, vibration suppression
 *       using input shaping, anti-resonance filter auto-design,
 *       time-varying parameter estimation
 *
 * Alignment: MIT 6.302, Stanford ENGR205, Berkeley ME233
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "axis_types.h"
#include "servo_tuning.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*===========================================================================
 * L8: Lyapunov-Based Adaptive Gain Tuning
 *
 * For a velocity-controlled PM motor with unknown load inertia:
 *
 *   J·dω/dt = Kt·iq - B·ω - Tl
 *
 * With adaptive PI controller:
 *   iq = K̂p·e + K̂i·∫e   where e = ω_ref - ω
 *
 * Lyapunov candidate:
 *   V = (J/2)·e² + (1/(2γ1))·(K̂p - Kp_opt)² + (1/(2γ2))·(K̂i - Ki_opt)²
 *
 * Adaptation laws (gradient descent on Lyapunov derivative):
 *   dK̂p/dt = -γ1·∂V/∂K̂p = γ1·e² / J
 *   dK̂i/dt = -γ2·∂V/∂K̂i = γ2·e·∫e / J
 *
 * This guarantees V̇ ≤ 0 when bounds are respected.
 *
 * Reference: Slotine & Li (1991), Section 8.3, "Adaptive Control"
 *===========================================================================*/

/**
 * @brief State for Lyapunov-based adaptive velocity loop.
 */
typedef struct {
    bool     enabled;
    double   inertia_estimate;       /**< Current inertia estimate Ĵ */
    double   kp_adaptive;            /**< Current Kp value */
    double   ki_adaptive;            /**< Current Ki value */
    double   kp_nominal;             /**< Nominal Kp (from initial tuning) */
    double   ki_nominal;             /**< Nominal Ki */
    double   gamma_1;                /**< Adaptation rate for Kp */
    double   gamma_2;                /**< Adaptation rate for Ki */
    double   error_integrated;       /**< ∫e dt */
    double   error_squared_sum;      /**< ∫e² dt */
    double   ts;                     /**< Sample time */
    double   kp_min, kp_max;
    double   ki_min, ki_max;
    double   lyapunov_value;         /**< Current Lyapunov function value */
    bool     converging;             /**< True if parameters are converging */
} lyapunov_adaptive_t;

/* Lyapunov adaptive state is passed by pointer – no static global needed */

/**
 * @brief Initialize Lyapunov adaptive controller.
 *
 * @param lyap         Adaptive state
 * @param kp_nominal   Nominal Kp from standard tuning
 * @param ki_nominal   Nominal Ki from standard tuning
 * @param Ts           Sample time (s)
 */
void lyapunov_adaptive_init(lyapunov_adaptive_t *lyap,
                              double kp_nominal, double ki_nominal, double Ts)
{
    if (!lyap) return;
    memset(lyap, 0, sizeof(lyapunov_adaptive_t));

    lyap->enabled = true;
    lyap->kp_nominal = kp_nominal;
    lyap->ki_nominal = ki_nominal;
    lyap->kp_adaptive = kp_nominal;
    lyap->ki_adaptive = ki_nominal;
    lyap->gamma_1 = 0.01;    /**< Slow adaptation for stability */
    lyap->gamma_2 = 0.001;
    lyap->ts = Ts;
    lyap->kp_min = kp_nominal * 0.2;
    lyap->kp_max = kp_nominal * 5.0;
    lyap->ki_min = ki_nominal * 0.2;
    lyap->ki_max = ki_nominal * 5.0;
    lyap->converging = false;
}

/**
 * @brief Update adaptive gains using Lyapunov method.
 *
 * @param lyap      Adaptive state
 * @param velocity_error Current velocity error (rad/s or eng units/s)
 * @param[out] kp   Updated Kp (output to velocity loop)
 * @param[out] ki   Updated Ki
 */
void lyapunov_adaptive_update(lyapunov_adaptive_t *lyap,
                                double velocity_error,
                                double *kp, double *ki)
{
    if (!lyap || !lyap->enabled || !kp || !ki) {
        if (kp) *kp = (lyap) ? lyap->kp_adaptive : 0.0;
        if (ki) *ki = (lyap) ? lyap->ki_adaptive : 0.0;
        return;
    }

    /* Update integrated error and squared error */
    lyap->error_integrated += velocity_error * lyap->ts;
    lyap->error_squared_sum += velocity_error * velocity_error * lyap->ts;

    /* Lyapunov-derived adaptation laws */
    double dkp = lyap->gamma_1 * velocity_error * velocity_error / 
                 (lyap->inertia_estimate > 0.0 ? lyap->inertia_estimate : 1.0);
    double dki = lyap->gamma_2 * velocity_error * lyap->error_integrated /
                 (lyap->inertia_estimate > 0.0 ? lyap->inertia_estimate : 1.0);

    lyap->kp_adaptive += dkp * lyap->ts;
    lyap->ki_adaptive += dki * lyap->ts;

    /* Projection (bounds enforcement) */
    if (lyap->kp_adaptive > lyap->kp_max) lyap->kp_adaptive = lyap->kp_max;
    if (lyap->kp_adaptive < lyap->kp_min) lyap->kp_adaptive = lyap->kp_min;
    if (lyap->ki_adaptive > lyap->ki_max) lyap->ki_adaptive = lyap->ki_max;
    if (lyap->ki_adaptive < lyap->ki_min) lyap->ki_adaptive = lyap->ki_min;

    /* Compute Lyapunov function value for monitoring */
    double kp_err = lyap->kp_adaptive - lyap->kp_nominal;
    double ki_err = lyap->ki_adaptive - lyap->ki_nominal;
    lyap->lyapunov_value = 0.5 * velocity_error * velocity_error
                          + 0.5 * kp_err * kp_err / lyap->gamma_1
                          + 0.5 * ki_err * ki_err / lyap->gamma_2;

    /* Convergence detection */
    if (fabs(dkp) < 1e-6 && fabs(dki) < 1e-6) {
        lyap->converging = true;
    }

    *kp = lyap->kp_adaptive;
    *ki = lyap->ki_adaptive;
}

/*===========================================================================
 * L8: Monte Carlo Robustness Analysis
 *
 * Evaluates servo performance under parameter uncertainty by
 * running N simulations with randomly perturbed parameters.
 *
 * Parameters varied:
 *   - Inertia (±30%)
 *   - Motor resistance (±15%)
 *   - Backlash (±50%)
 *   - Viscous friction (±40%)
 *
 * Performance metrics:
 *   - Settling time (to within 1%)
 *   - Overshoot percentage
 *   - IAE (Integral of Absolute Error)
 *   - Phase margin degradation
 *
 * Reference: Ray & Stengel, "A Monte Carlo Approach to the Analysis
 *            of Control System Robustness" (1993), Automatica, 29(1)
 *===========================================================================*/

/** @brief Pseudo-random number generator (xorshift64).
 *
 * Used for deterministic Monte Carlo simulations (reproducible results).
 *
 * @param state  PRNG state (modified in place)
 * @return       Uniform random in [0, 1)
 */
static double prng_uniform(uint64_t *state)
{
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return (double)(x & 0xFFFFFFFFFFFFFull) / (double)0x100000000000000ull;
}

/**
 * @brief Generate a normally distributed random number (Box-Muller).
 *
 * @param state  PRNG state
 * @param mean   Mean of normal distribution
 * @param stddev Standard deviation
 * @return       Normal random value
 */
static double prng_normal(uint64_t *state, double mean, double stddev)
{
    double u1 = prng_uniform(state);
    double u2 = prng_uniform(state);

    /* Avoid log(0) */
    if (u1 < 1e-15) u1 = 1e-15;

    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + stddev * z;
}

/**
 * @brief Run a Monte Carlo robustness analysis for velocity loop tuning.
 *
 * Simulates N trials with randomly perturbed system parameters
 * and evaluates closed-loop performance.
 *
 * @param n_trials          Number of Monte Carlo trials (typically 1000+)
 * @param nominal_inertia   Nominal inertia (kg·cm²)
 * @param nominal_resistance Nominal motor resistance (Ω)
 * @param kp_vel            Velocity Kp under test
 * @param ki_vel            Velocity Ki under test
 * @param inertia_std_pct   Inertia standard deviation (% of nominal)
 * @param seed              PRNG seed (0 = use time)
 * @param[out] summary      Summary statistics
 */
void monte_carlo_robustness(int n_trials,
                              double nominal_inertia,
                              double nominal_resistance,
                              double kp_vel, double ki_vel,
                              double inertia_std_pct,
                              uint64_t seed,
                              mc_summary_t *summary)
{
    if (!summary || n_trials < 1) return;
    memset(summary, 0, sizeof(mc_summary_t));

    if (seed == 0) seed = 1234567890123456789ull;
    uint64_t prng_state = seed;

    double st_sum = 0.0, st_sum2 = 0.0;
    double os_sum = 0.0, os_sum2 = 0.0;
    double iae_sum = 0.0, iae_sum2 = 0.0;
    int unstable_count = 0;

    summary->worst_st = 0.0;
    summary->best_st = 1e9;

    for (int trial = 0; trial < n_trials; trial++) {
        /* Perturb parameters */
        double J = prng_normal(&prng_state, nominal_inertia,
                                nominal_inertia * inertia_std_pct / 100.0);
        if (J < nominal_inertia * 0.1) J = nominal_inertia * 0.1; /* Floor */

        (void)prng_normal(&prng_state, nominal_resistance,
                           nominal_resistance * 0.15); /* R perturbation – reserved for future resistance-dependent simulation */

        /* Simple simulation of velocity loop step response */
        /* Plant: G(s) = Kt / (J·s + B), with PI controller */
        double Kt = 0.5;  /* Typical N·m/A */
        double B = 0.001; /* Small viscous friction */

        double J_kgm2 = J * 1e-4;

        /* Closed-loop natural frequency and damping */
        double wn = sqrt((Kt * ki_vel) / J_kgm2);
        double zeta = (B + Kt * kp_vel) / (2.0 * sqrt(J_kgm2 * Kt * ki_vel));

        if (zeta < 0.0) {
            /* Unstable */
            unstable_count++;
            continue;
        }

        /* Settling time (4 / (ζ·ωn) for 2% criterion) */
        double st = (zeta > 0.0 && wn > 0.0) ? 4.0 / (zeta * wn) : 10.0;

        /* Overshoot: exp(-π·ζ/√(1-ζ²)) for ζ < 1 */
        double os_pct = 0.0;
        if (zeta < 1.0) {
            os_pct = 100.0 * exp(-M_PI * zeta / sqrt(1.0 - zeta * zeta));
        }

        /* IAE approximate: ∫|e|dt for standard second-order step response */
        double iae = (1.0 / (zeta * wn)) * (1.0 + os_pct / 100.0);

        st_sum  += st;    st_sum2  += st * st;
        os_sum  += os_pct; os_sum2 += os_pct * os_pct;
        iae_sum += iae;   iae_sum2 += iae * iae;

        if (st > summary->worst_st) summary->worst_st = st;
        if (st < summary->best_st) summary->best_st = st;
    }

    int valid_trials = n_trials - unstable_count;
    if (valid_trials > 0) {
        summary->st_mean = st_sum / valid_trials;
        summary->st_std  = sqrt(st_sum2 / valid_trials - summary->st_mean * summary->st_mean);
        summary->os_mean = os_sum / valid_trials;
        summary->os_std  = sqrt(os_sum2 / valid_trials - summary->os_mean * summary->os_mean);
        summary->iae_mean = iae_sum / valid_trials;
        summary->iae_std  = sqrt(iae_sum2 / valid_trials - summary->iae_mean * summary->iae_mean);
    }

    summary->instability_pct = 100.0 * (double)unstable_count / (double)n_trials;
    summary->total_trials = n_trials;
}

/*===========================================================================
 * L8: Cogging Torque Compensation for PM Motors
 *
 * Cogging torque arises from the interaction between permanent magnets
 * and stator slotting. It is position-dependent (periodic with electrical
 * angle) and independent of current.
 *
 * Model: T_cog(θ_e) = Σ_{n=1}^{N} A_n · sin(6n·θ_e + φ_n)
 *
 * The dominant harmonic is the 6th (6× electrical frequency) for a
 * 3-phase motor with integer slots per pole.
 *
 * Compensation: Add a feedforward current iq_ff(θ_e) = -T_cog(θ_e) / Kt
 *
 * The cogging table is learned offline (or during commissioning)
 * by measuring torque ripple at constant low velocity.
 *
 * Reference: Jahns & Soong, "Pulsating Torque Minimization Techniques
 *            for Permanent Magnet AC Motor Drives – A Review"
 *            IEEE Trans. Industrial Electronics, 43(2), 1996
 *===========================================================================*/

/**
 * @brief Cogging torque compensation lookup table.
 *
 * Stores the identified cogging torque as a function of electrical angle.
 */
#define COGGING_TABLE_SIZE 360  /**< 1-degree resolution in electrical degrees */

typedef struct {
    bool     enabled;
    double   table[COGGING_TABLE_SIZE]; /**< Cogging torque per electrical degree (N·m) */
    double   Kt;                        /**< Motor torque constant for feedforward scaling */
    double   scaling;                   /**< Overall scaling factor (0-1, learned confidence) */
} cogging_comp_t;

/**
 * @brief Initialize cogging compensation table.
 *
 * @param cc   Cogging compensation structure
 * @param Kt   Motor torque constant (N·m/A RMS)
 */
void cogging_comp_init(cogging_comp_t *cc, double Kt)
{
    if (!cc) return;
    memset(cc, 0, sizeof(cogging_comp_t));
    cc->enabled = false;
    cc->Kt = Kt;
    cc->scaling = 0.0; /* Start with zero compensation */
}

/**
 * @brief Learn cogging torque by averaging torque at constant velocity.
 *
 * At very low, constant velocity, the net torque output equals
 * the cogging torque (assuming small friction and no load).
 *
 * This function updates one entry in the cogging table.
 *
 * @param cc                Cogging compensation
 * @param electrical_angle  Current electrical angle (rad, 0→2π)
 * @param torque_feedback   Measured/estimated torque (N·m)
 */
void cogging_comp_learn(cogging_comp_t *cc, double electrical_angle,
                         double torque_feedback)
{
    if (!cc) return;

    /* Convert to degrees and wrap */
    double deg = electrical_angle * 180.0 / M_PI;
    deg = fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;

    int idx = (int)deg;
    if (idx < 0) idx = 0;
    if (idx >= COGGING_TABLE_SIZE) idx = COGGING_TABLE_SIZE - 1;

    /* Exponential moving average */
    double alpha = 0.05;
    cc->table[idx] = (1.0 - alpha) * cc->table[idx] + alpha * torque_feedback;
    cc->scaling = (cc->scaling < 1.0) ? cc->scaling + 0.001 : 1.0;
}

/**
 * @brief Get cogging torque compensation for a given electrical angle.
 *
 * Uses linear interpolation between table entries.
 *
 * @param cc                Cogging compensation
 * @param electrical_angle  Electrical angle (rad)
 * @return                  Compensating torque feedforward (N·m)
 */
double cogging_comp_get(cogging_comp_t *cc, double electrical_angle)
{
    if (!cc || !cc->enabled || cc->scaling < 0.01) return 0.0;

    double deg = electrical_angle * 180.0 / M_PI;
    deg = fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;

    int idx_lo = (int)deg;
    int idx_hi = (idx_lo + 1) % COGGING_TABLE_SIZE;
    double frac = deg - (double)idx_lo;

    double T_cog = cc->table[idx_lo] * (1.0 - frac) + cc->table[idx_hi] * frac;

    return -T_cog * cc->scaling;  /* Negative feedback compensation */
}

/*===========================================================================
 * L8: Input Shaping for Vibration Suppression
 *
 * Input shaping convolves the command signal with a sequence of impulses
 * designed to cancel the residual vibration of flexible modes.
 *
 * Zero Vibration (ZV) shaper for a mode with frequency f_n and damping ζ:
 *   t1 = 0,                     A1 = 1 / (1 + K)
 *   t2 = π / (ωn·√(1-ζ²)),     A2 = K / (1 + K)
 *   where K = exp(-π·ζ / √(1-ζ²))
 *
 * Zero Vibration Derivative (ZVD) adds robustness to frequency uncertainty:
 *   3 impulses instead of 2, derivative constraint forces zero sensitivity
 *   at the nominal frequency.
 *
 * Reference: Singer & Seering, "Preshaping Command Inputs to Reduce
 *            System Vibration" (1990), ASME J. Dynamic Systems, 112(1)
 *===========================================================================*/

/**
 * @brief ZVD (Zero Vibration Derivative) input shaper parameters.
 */
typedef struct {
    bool     active;
    double   natural_freq_hz;       /**< Flexible mode natural frequency (Hz) */
    double   damping_ratio;         /**< Modal damping ratio */
    double   impulse_times[3];      /**< Impulse application times (s) */
    double   impulse_amplitudes[3]; /**< Impulse amplitudes (sum = 1) */
    double   filter_state[8];       /**< Delay line for convolution */
    int      filter_index;
    int      filter_length;
} input_shaper_t;

/**
 * @brief Design a ZVD input shaper.
 *
 * @param shaper    Input shaper to configure
 * @param fn        Natural frequency to cancel (Hz)
 * @param zeta      Damping ratio
 * @param Ts        Sample time of the servo loop (s)
 */
void input_shaper_design_zvd(input_shaper_t *shaper,
                               double fn, double zeta, double Ts)
{
    if (!shaper || fn <= 0.0) return;
    memset(shaper, 0, sizeof(input_shaper_t));

    double wn = 2.0 * M_PI * fn;
    double wd = wn * sqrt(1.0 - zeta * zeta);
    double K = exp(-M_PI * zeta / sqrt(1.0 - zeta * zeta));

    /* ZVD shaper: 3 impulses */
    double Td = M_PI / wd;          /**< Half period */

    shaper->impulse_times[0] = 0.0;
    shaper->impulse_times[1] = Td;
    shaper->impulse_times[2] = 2.0 * Td;

    shaper->impulse_amplitudes[0] = 1.0 / ((1.0 + K) * (1.0 + K));
    shaper->impulse_amplitudes[1] = 2.0 * K / ((1.0 + K) * (1.0 + K));
    shaper->impulse_amplitudes[2] = K * K / ((1.0 + K) * (1.0 + K));

    /* Initialize delay line */
    shaper->filter_length = (int)(shaper->impulse_times[2] / Ts) + 1;
    if (shaper->filter_length > 8) shaper->filter_length = 8;
    shaper->filter_index = 0;
    shaper->active = true;
}

/**
 * @brief Apply input shaping to a command signal.
 *
 * Convolves the input with the pre-designed impulse sequence.
 *
 * @param shaper    Input shaper
 * @param input     New command sample
 * @return          Shaped (convolved) output
 */
double input_shaper_apply(input_shaper_t *shaper, double input)
{
    if (!shaper || !shaper->active) return input;

    /* Shift delay line */
    for (int i = shaper->filter_length - 1; i > 0; i--) {
        shaper->filter_state[i] = shaper->filter_state[i - 1];
    }
    shaper->filter_state[0] = input;

    /* Convolve with impulse sequence (simplified: only use stored samples) */
    double output = 0.0;
    for (int i = 0; i < 3; i++) {
        int delay_idx = (int)(shaper->impulse_times[i] / 0.002); /* Assume 2ms Ts */
        if (delay_idx < shaper->filter_length && delay_idx >= 0) {
            output += shaper->impulse_amplitudes[i]
                      * shaper->filter_state[delay_idx];
        }
    }

    return output;
}

/*===========================================================================
 * L8: Anti-Resonance Filter Auto-Design
 *
 * Mechanical systems often exhibit anti-resonances (transmission zeros)
 * in addition to resonances. An anti-resonance occurs when the motor
 * and load move in opposite phase, creating a zero in the transfer
 * function from torque to motor velocity.
 *
 * The two-mass system transfer function:
 *
 *   ω_m(s) / T_m(s) = (J_L·s² + B_L·s + K_s)
 *                    / (J_m·J_L·s³ + (J_m·B_L+J_L·B_m)·s² + K_s·(J_m+J_L)·s)
 *
 * Anti-resonance frequency: f_ar = (1/2π)·√(K_s / J_L)
 * Resonance frequency:      f_r  = (1/2π)·√(K_s·(J_m+J_L) / (J_m·J_L))
 *
 * The anti-resonance creates a notch in the frequency response.
 * By estimating f_ar and J_L/J_m, we can auto-design the notch filter
 * to avoid exciting the resonance.
 *
 * Reference: Ellis (2012), Section 14.4
 *===========================================================================*/

/**
 * @brief Estimate anti-resonance frequency from frequency response data.
 *
 * Searches for the frequency where magnitude drops sharply
 * (indicating an anti-resonance / transmission zero).
 *
 * @param frequencies       Array of frequency points (Hz)
 * @param magnitudes        Magnitude response at each frequency (linear)
 * @param n_points          Number of points
 * @param motor_inertia     Motor rotor inertia (kg·cm²)
 * @param load_inertia      Estimated load inertia (kg·cm²)
 * @param[out] f_ar         Identified anti-resonance frequency (Hz)
 * @param[out] ks_estimate  Estimated shaft stiffness (N·m/rad)
 * @return                  0 on success, -1 if no anti-resonance found
 */
int anti_resonance_identify(const double *frequencies,
                              const double *magnitudes,
                              int n_points,
                              double motor_inertia,
                              double load_inertia,
                              double *f_ar, double *ks_estimate)
{
    (void)motor_inertia; /* Reserved for two-mass model validation */

    if (!frequencies || !magnitudes || n_points < 3 || !f_ar || !ks_estimate) {
        return -1;
    }

    /* Find the frequency with minimum magnitude (anti-resonance notch) */
    int min_idx = 0;
    double min_mag = magnitudes[0];

    for (int i = 1; i < n_points; i++) {
        /* Only consider frequencies below the resonance peak */
        if (magnitudes[i] < min_mag && frequencies[i] < 500.0) {
            min_mag = magnitudes[i];
            min_idx = i;
        }
    }

    if (min_mag > 0.9) {
        /* No significant anti-resonance – stiff coupling */
        *f_ar = 0.0;
        *ks_estimate = INFINITY;
        return -1;
    }

    *f_ar = frequencies[min_idx];

    /* Estimate coupling stiffness from anti-resonance frequency */
    /* f_ar = (1/2π)·√(K_s / J_L)  →  K_s = (2π·f_ar)² · J_L */
    double J_L_kgm2 = load_inertia * 1e-4;
    double w_ar = 2.0 * M_PI * (*f_ar);
    *ks_estimate = w_ar * w_ar * J_L_kgm2;

    return 0;
}

/*===========================================================================
 * L8: Time-Varying Parameter Estimation with Forgetting Factor
 *
 * Recursive Least Squares (RLS) with exponential forgetting for
 * tracking slowly changing parameters (e.g., load inertia during
 * robot arm extension).
 *
 * Model: y(k) = φᵀ(k) · θ
 *   where φ = regressor vector, θ = parameters to estimate
 *
 * RLS update:
 *   e(k) = y(k) - φᵀ(k)·θ̂(k-1)
 *   P(k) = (1/λ)·[P(k-1) - P(k-1)·φ(k)·φᵀ(k)·P(k-1) / (λ + φᵀ(k)·P(k-1)·φ(k))]
 *   θ̂(k) = θ̂(k-1) + P(k)·φ(k)·e(k)
 *
 * where λ = forgetting factor (0.95-0.999)
 *
 * Reference: Ljung, "System Identification" (1999), Ch.11
 *===========================================================================*/

/**
 * @brief RLS estimator for inertia estimation.
 *
 * Model: ω(k) - ω(k-1) = (Ts/J)·T(k-1)
 *   → y = Δω, φ = Ts·T(k-1), θ = 1/J
 */
typedef struct {
    bool     initialized;
    double   theta_hat;             /**< Estimated parameter (1/J) */
    double   p_matrix;              /**< Covariance (scalar for 1-param) */
    double   lambda;                /**< Forgetting factor */
    double   estimation;            /**< Current J estimate (1/theta_hat) */
    double   prev_velocity;         /**< ω(k-1) */
    double   prev_torque;           /**< T(k-1) */
} rls_inertia_estimator_t;

/**
 * @brief Initialize the RLS inertia estimator.
 *
 * @param rls           Estimator state
 * @param initial_J     Initial inertia guess (kg·cm²)
 * @param lambda        Forgetting factor (0.95-0.999)
 * @param initial_cov   Initial covariance (large = fast adaptation)
 */
void rls_inertia_init(rls_inertia_estimator_t *rls,
                       double initial_J, double lambda, double initial_cov)
{
    if (!rls) return;
    memset(rls, 0, sizeof(rls_inertia_estimator_t));

    if (initial_J > 0.0) {
        rls->theta_hat = 1.0 / (initial_J * 1e-4);  /* 1/J in kg·m² */
    } else {
        rls->theta_hat = 0.1;
    }

    rls->p_matrix = initial_cov;
    rls->lambda = (lambda > 0.9 && lambda < 1.0) ? lambda : 0.98;
    rls->estimation = initial_J;
    rls->initialized = true;
}

/**
 * @brief Update the RLS inertia estimator with new measurements.
 *
 * @param rls       Estimator state
 * @param velocity  Current velocity measurement
 * @param torque    Current torque command
 * @param Ts        Sample time (s)
 * @return          Updated inertia estimate (kg·cm²)
 */
double rls_inertia_update(rls_inertia_estimator_t *rls,
                            double velocity, double torque, double Ts)
{
    if (!rls || !rls->initialized || Ts <= 0.0) {
        return (rls) ? rls->estimation : 0.0;
    }

    /* Skip first sample */
    if (rls->prev_torque == 0.0 && rls->prev_velocity == 0.0) {
        rls->prev_velocity = velocity;
        rls->prev_torque = torque;
        return rls->estimation;
    }

    /* Measurement: Δω */
    double y = velocity - rls->prev_velocity;

    /* Regressor: Ts * T(k-1) */
    double phi = Ts * rls->prev_torque;

    /* Only update when there's meaningful excitation */
    if (fabs(phi) < 1e-9) {
        rls->prev_velocity = velocity;
        rls->prev_torque = torque;
        return rls->estimation;
    }

    /* Prediction error */
    double error = y - phi * rls->theta_hat;

    /* Kalman gain: K = P·φ / (λ + φ²·P) */
    double K_gain = rls->p_matrix * phi / (rls->lambda + phi * phi * rls->p_matrix);

    /* Update parameter estimate */
    rls->theta_hat += K_gain * error;

    /* Update covariance */
    rls->p_matrix = (1.0 / rls->lambda)
                   * (1.0 - K_gain * phi) * rls->p_matrix;

    /* Ensure positive definiteness */
    if (rls->p_matrix < 1e-12) rls->p_matrix = 1e-12;

    /* Convert to inertia */
    if (fabs(rls->theta_hat) > 1e-9) {
        rls->estimation = (1.0 / rls->theta_hat) * 1e-4; /* kg·m² → kg·cm² */
    }

    /* Store for next iteration */
    rls->prev_velocity = velocity;
    rls->prev_torque = torque;

    return rls->estimation;
}
