/**
 * @file pointio_signal_proc.c
 * @brief Advanced signal processing for Point I/O data.
 *
 * Level: L5 Algorithms + L8 Advanced Topics
 *
 * Reference:
 *   - Oppenheim & Schafer, "Discrete-Time Signal Processing" (3rd ed.)
 *   - ISA TR77.82.01 "Data Quality for Process Measurements"
 *   - Rockwell "Logix5000 Controllers Process Control Instructions" (1756-RM006)
 *
 * Implements:
 *   - Moving average and median filtering
 *   - Rate-of-change (ROC) limiting
 *   - Deadband / hysteresis filtering
 *   - Signal quality assessment
 *   - Statistical outlier detection
 *   - Exponential smoothing variants
 *   - Linear interpolation for missing data
 *   - Signal validation (freeze detection, noise floor)
 */

#include "pointio_types.h"
#include "pointio_module.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/*===========================================================================
 * L5: Moving Average Filter
 *
 * Computes the unweighted mean of the last N samples.
 * Provides smoothing with a linear-phase (symmetric) response.
 *
 * Transfer function: H(z) = (1/N) * sum_{k=0}^{N-1} z^{-k}
 *
 * Cutoff frequency (-3dB): fc ≈ 0.443 * fs / N
 *   where fs = sampling frequency, N = window size
 *
 * Delay: (N-1)/2 samples (constant group delay)
 *
 * Applications:
 *   - Smooth noisy pressure/temperature readings
 *   - Remove 50/60 Hz line noise with N = fs / f_noise
 *   - Level sensor filtering (reduces wave-induced variation)
 *
 * Reference: Smith, "Digital Signal Processing", Ch.15
 *===========================================================================*/

/**
 * Moving average filter with ring buffer.
 *
 * @param buffer       Ring buffer of previous samples (user-managed)
 * @param buffer_size  Number of samples in ring buffer (= window size N)
 * @param write_index  Current write position in buffer (in/out)
 * @param new_sample   New input sample
 * @param average      Output: moving average
 * @param is_full      Input: 1 if buffer has been fully populated at least once
 * @return 0 on success
 */
int pointio_signal_moving_average(double *buffer, int buffer_size,
    int *write_index, double new_sample, double *average, int is_full)
{
    if (!buffer || !write_index || !average) return -1;
    if (buffer_size <= 0) return -1;

    /* Write new sample to ring buffer */
    buffer[*write_index] = new_sample;
    *write_index = (*write_index + 1) % buffer_size;

    /* Compute sum of valid samples */
    int valid_count = is_full ? buffer_size : (*write_index == 0 ? buffer_size : *write_index);
    double sum = 0.0;
    for (int i = 0; i < valid_count; i++) {
        sum += buffer[i];
    }
    *average = sum / (double)valid_count;

    return 0;
}

/*===========================================================================
 * L5: Median Filter
 *
 * Nonlinear filter that replaces each sample with the median
 * of the last N samples. Excellent for removing impulse noise
 * (spikes) while preserving edges.
 *
 * Complexity: O(N log N) per sample (via sorting the window).
 * For real-time applications with N ≤ 5-7, this is acceptable.
 *
 * Applications:
 *   - Removing electrical noise spikes from analog inputs
 *   - Filtering level sensor "glitches" from foam/waves
 *   - Preprocessing before PID control
 *===========================================================================*/

/**
 * Compare function for qsort.
 */
static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/**
 * Median filter with ring buffer.
 *
 * Computes the median of the most recent N samples.
 *
 * @param buffer       Ring buffer of previous samples
 * @param buffer_size  Window size (N). Use odd N for clean median.
 * @param write_index  Current write position (in/out)
 * @param new_sample   New input sample
 * @param median       Output: median value
 * @param is_full      1 if buffer has been fully populated
 * @return 0 on success
 */
int pointio_signal_median_filter(double *buffer, int buffer_size,
    int *write_index, double new_sample, double *median, int is_full)
{
    if (!buffer || !write_index || !median) return -1;
    if (buffer_size <= 0) return -1;

    /* Write new sample */
    buffer[*write_index] = new_sample;
    *write_index = (*write_index + 1) % buffer_size;

    int valid_count = is_full ? buffer_size : (*write_index == 0 ? buffer_size : *write_index);

    /* Copy valid samples to temporary array for sorting */
    double temp[32];
    if (valid_count > 32) valid_count = 32;  /* Safety limit */

    for (int i = 0; i < valid_count; i++) {
        temp[i] = buffer[i];
    }

    qsort(temp, (size_t)valid_count, sizeof(double), compare_double);

    /* Median: middle element for odd count, average of two middle for even */
    if (valid_count % 2 == 1) {
        *median = temp[valid_count / 2];
    } else {
        *median = (temp[valid_count / 2 - 1] + temp[valid_count / 2]) / 2.0;
    }

    return 0;
}

/*===========================================================================
 * L5: Rate-of-Change Limiter
 *
 * Limits how fast a signal can change between consecutive samples.
 * Prevents sudden jumps that could cause process upsets or
 * actuator damage.
 *
 * Algorithm:
 *   If |new - prev| > max_roc * dt, clamp to prev ± max_roc * dt
 *
 * The max_roc limit should be based on physical process constraints:
 *   - Valve slew rate: ~1-5% per second
 *   - Temperature ramp: 1-5 degC per minute
 *   - Motor speed: acceleration limit from drive parameter
 *
 * Reference: IEC 61131-3, Rate Limiter Function Block (RAMP)
 *===========================================================================*/

/**
 * Apply rate-of-change limiting to a signal.
 *
 * @param prev_value     Previous (already rate-limited) value
 * @param new_raw_value  New raw input value
 * @param max_roc_per_s  Maximum allowed rate of change (units per second)
 * @param dt_s           Time step in seconds
 * @param limited_value  Output: rate-limited value
 * @return 0 = within limits, 1 = clamped (rate limit applied)
 */
int pointio_signal_rate_limit(double prev_value, double new_raw_value,
    double max_roc_per_s, double dt_s, double *limited_value)
{
    if (!limited_value) return -1;
    if (dt_s <= 0.0) return -1;

    double max_change = max_roc_per_s * dt_s;
    double delta = new_raw_value - prev_value;

    if (fabs(delta) <= max_change) {
        *limited_value = new_raw_value;
        return 0;  /* Within limits */
    }

    /* Clamp to max rate */
    if (delta > 0) {
        *limited_value = prev_value + max_change;
    } else {
        *limited_value = prev_value - max_change;
    }

    return 1;  /* Rate limit applied */
}

/*===========================================================================
 * L5: Deadband / Hysteresis Filter
 *
 * Prevents small fluctuations around a value from generating
 * unnecessary updates (alarms, COS events, network traffic).
 *
 * The output only changes when the input deviates from the
 * last reported value by more than the deadband threshold.
 *
 * Applications:
 *   - Reduce COS network traffic from noisy analog signals
 *   - Prevent alarm chattering near trip points
 *   - Smooth display of inherently jittery measurements
 *
 * Reference: ISA TR18.2.4, "Alarm System Monitoring"
 *===========================================================================*/

/**
 * Apply deadband filtering.
 *
 * @param last_reported  Last value that passed the deadband check
 * @param new_value      New input value
 * @param deadband       Half-width of deadband zone
 * @param output_value   Output: value to report (or NaN if within deadband)
 * @param changed        Output: 0 = no change, 1 = value changed
 * @return 0 on success
 */
int pointio_signal_deadband(double last_reported, double new_value,
    double deadband, double *output_value, int *changed)
{
    if (!output_value || !changed) return -1;

    if (fabs(new_value - last_reported) > deadband) {
        *output_value = new_value;
        *changed = 1;
    } else {
        *output_value = last_reported;  /* Hold last reported value */
        *changed = 0;
    }

    return 0;
}

/*===========================================================================
 * L5: Exponential Smoothing Variants
 *
 * 1. Simple Exponential Smoothing (SES):
 *    s[t] = alpha * x[t] + (1 - alpha) * s[t-1]
 *
 * 2. Double Exponential Smoothing (DES) / Holt's method:
 *    s[t] = alpha * x[t] + (1 - alpha) * (s[t-1] + b[t-1])
 *    b[t] = beta * (s[t] - s[t-1]) + (1 - beta) * b[t-1]
 *
 *    Handles signals with trend. Provides both smoothed value
 *    and trend estimate.
 *
 * Applications:
 *   - Process variable trending with lag compensation
 *   - Demand forecasting for energy management
 *   - Sensor data with drift (temperature, pressure)
 *
 * Reference: Holt (1957), "Forecasting trends and seasonals by
 *            exponentially weighted moving averages"
 *===========================================================================*/

/**
 * Double exponential smoothing (Holt's linear method).
 *
 * @param alpha     Smoothing factor for level (0 < alpha < 1)
 * @param beta      Smoothing factor for trend (0 < beta < 1)
 * @param x         New observation
 * @param level     Level estimate (in/out)
 * @param trend     Trend estimate (in/out)
 * @param smoothed  Output: smoothed value
 * @param forecast  Output: one-step-ahead forecast
 * @return 0 on success
 */
int pointio_signal_holt_smoothing(double alpha, double beta,
    double x, double *level, double *trend,
    double *smoothed, double *forecast)
{
    if (!level || !trend || !smoothed || !forecast) return -1;
    if (alpha <= 0.0 || alpha > 1.0) return -1;
    if (beta <= 0.0 || beta > 1.0) return -1;

    /* Forecast from previous state */
    *forecast = *level + *trend;

    /* Update level */
    double new_level = alpha * x + (1.0 - alpha) * (*level + *trend);

    /* Update trend */
    double new_trend = beta * (new_level - *level) + (1.0 - beta) * (*trend);

    *smoothed = new_level;
    *level = new_level;
    *trend = new_trend;

    return 0;
}

/*===========================================================================
 * L8: Outlier Detection (Statistical)
 *
 * Detects measurement outliers using statistical methods.
 * Outliers can indicate:
 *   - Sensor malfunction (intermittent failure)
 *   - Electrical noise (EMI, switching transients)
 *   - Process events (slug flow, cavitation)
 *
 * Methods implemented:
 *   1. Z-Score: |x - mu| > z_threshold * sigma → outlier
 *   2. Modified Z-Score (MAD-based): more robust to outliers
 *      M = 0.6745 * (x - median) / MAD
 *      |M| > 3.5 → outlier (Iglewicz & Hoaglin, 1993)
 *
 * Reference: Iglewicz & Hoaglin, "How to Detect and Handle Outliers" (1993)
 *===========================================================================*/

/**
 * Detect outlier using z-score method.
 *
 * @param value      Candidate value
 * @param mean       Running mean of normal data
 * @param stddev     Running standard deviation
 * @param threshold  Z-score threshold (typical: 3.0 for 99.7% CI, 2.0 for 95%)
 * @param is_outlier Output: 1 if outlier
 * @return 0 on success
 */
int pointio_signal_detect_outlier_zscore(double value, double mean,
    double stddev, double threshold, int *is_outlier)
{
    if (!is_outlier) return -1;
    if (stddev <= 0.0) {
        *is_outlier = 0;  /* Cannot assess without stddev */
        return 0;
    }

    double z = fabs(value - mean) / stddev;
    *is_outlier = (z > threshold) ? 1 : 0;

    return 0;
}

/**
 * Update running mean and variance using Welford's online algorithm.
 *
 * Numerically stable single-pass computation of mean and variance.
 *
 * Reference: Welford (1962), "Note on a method for calculating corrected
 *            sums of squares and products", Technometrics 4(3):419-420
 *
 * @param new_value  New observation
 * @param count      Observation count (in/out)
 * @param mean       Running mean (in/out)
 * @param m2         Sum of squared differences (in/out, used for variance)
 * @param variance   Output: current variance estimate
 * @return 0 on success
 */
int pointio_signal_update_statistics(double new_value, int *count,
    double *mean, double *m2, double *variance)
{
    if (!count || !mean || !m2 || !variance) return -1;

    (*count)++;
    double delta = new_value - *mean;
    *mean += delta / (double)(*count);
    double delta2 = new_value - *mean;
    *m2 += delta * delta2;

    if (*count > 1) {
        *variance = *m2 / (double)(*count - 1);  /* Sample variance (unbiased) */
    } else {
        *variance = 0.0;
    }

    return 0;
}

/*===========================================================================
 * L8: Signal Freeze Detection
 *
 * Detects when a sensor signal has "frozen" (constant output),
 * indicating sensor failure. A frozen signal is dangerous because
 * the control system receives no indication of process changes.
 *
 * Detection criteria:
 *   1. Signal standard deviation over window < noise_floor
 *   2. Signal has not changed by more than min_delta over the window
 *   3. Duration exceeds freeze_timeout
 *
 * Common in:
 *   - Pressure transmitters with clogged impulse lines
 *   - Failed RTDs reading last valid value
 *   - Communication errors where last good value is held
 *
 * Reference: ISA TR77.82.01 "Data Quality"
 *===========================================================================*/

/**
 * Check if a signal is frozen (constant over an observation window).
 *
 * @param buffer       Ring buffer of recent samples
 * @param buffer_size  Window size for freeze detection
 * @param noise_floor  Minimum expected signal variation (based on sensor spec)
 * @param is_frozen    Output: 1 if signal appears frozen
 * @param stddev       Output: computed standard deviation over window
 * @return 0 on success
 */
int pointio_signal_detect_frozen(const double *buffer, int buffer_size,
    double noise_floor, int *is_frozen, double *stddev)
{
    if (!buffer || !is_frozen || !stddev) return -1;
    if (buffer_size < 2) return -1;

    /* Compute mean and stddev over buffer */
    double sum = 0.0, sum_sq = 0.0;
    for (int i = 0; i < buffer_size; i++) {
        sum += buffer[i];
        sum_sq += buffer[i] * buffer[i];
    }

    double mean = sum / (double)buffer_size;
    double variance = (sum_sq / (double)buffer_size) - (mean * mean);
    if (variance < 0.0) variance = 0.0;
    *stddev = sqrt(variance);

    /* Frozen if stddev < noise_floor */
    *is_frozen = (*stddev < noise_floor) ? 1 : 0;

    return 0;
}

/*===========================================================================
 * L5: Linear Interpolation for Missing Data
 *
 * Fills gaps in time series data caused by communication glitches
 * or temporary sensor outages. Uses linear interpolation between
 * the last known good value and the next known good value.
 *
 * Formula: y(t) = y0 + (y1 - y0) * (t - t0) / (t1 - t0)
 *
 * This is the simplest form of imputation. More advanced methods
 * (Kalman filtering, spline interpolation) are available for
 * critical applications.
 *===========================================================================*/

/**
 * Linearly interpolate between two known data points.
 *
 * @param x0       Independent variable at point 0 (e.g., timestamp)
 * @param y0       Dependent variable at point 0
 * @param x1       Independent variable at point 1
 * @param y1       Dependent variable at point 1
 * @param x        Interpolation point (x0 <= x <= x1)
 * @param y        Output: interpolated value
 * @return 0 on success, -1 if x0 == x1 (division by zero)
 */
int pointio_signal_linear_interpolate(double x0, double y0,
    double x1, double y1, double x, double *y)
{
    if (!y) return -1;

    if (fabs(x1 - x0) < 1e-12) return -1;

    /* Ensure x is within bounds */
    if (x < x0) x = x0;
    if (x > x1) x = x1;

    double t = (x - x0) / (x1 - x0);
    *y = y0 + t * (y1 - y0);

    return 0;
}

/*===========================================================================
 * L5: Signal Validation
 *
 * Performs comprehensive signal quality checks:
 *   1. Range check (within sensor limits)
 *   2. Rate check (not changing impossibly fast)
 *   3. Noise check (not excessively noisy)
 *
 * Returns a quality code:
 *   0 = Good
 *   1 = Suspect (one check marginal)
 *   2 = Bad (one check failed)
 *   3 = Invalid (multiple checks failed)
 *===========================================================================*/

/**
 * Validate a sensor signal against multiple quality criteria.
 *
 * @param value       Current signal value
 * @param low_limit   Low range limit (sensor minimum)
 * @param high_limit  High range limit (sensor maximum)
 * @param max_roc     Maximum rate of change (units/s)
 * @param prev_value  Previous valid value
 * @param dt_s        Time since previous sample
 * @param max_noise   Maximum acceptable noise (stddev)
 * @param noise_stddev Current noise estimate
 * @param quality     Output: 0=Good, 1=Suspect, 2=Bad, 3=Invalid
 * @return 0 on success
 */
int pointio_signal_validate(double value, double low_limit, double high_limit,
    double max_roc, double prev_value, double dt_s,
    double max_noise, double noise_stddev, int *quality)
{
    if (!quality) return -1;

    int bad_checks = 0;
    int suspect_checks = 0;

    /* Check 1: Range */
    if (value < low_limit || value > high_limit) {
        bad_checks++;
    } else if (value < low_limit * 1.1 || value > high_limit * 0.9) {
        /* Within 10% of limit */
        suspect_checks++;
    }

    /* Check 2: Rate of change */
    if (dt_s > 0 && max_roc > 0) {
        double roc = fabs(value - prev_value) / dt_s;
        if (roc > max_roc * 2.0) {
            bad_checks++;
        } else if (roc > max_roc) {
            suspect_checks++;
        }
    }

    /* Check 3: Noise level */
    if (max_noise > 0 && noise_stddev > max_noise * 2.0) {
        bad_checks++;
    } else if (noise_stddev > max_noise) {
        suspect_checks++;
    }

    if (bad_checks >= 2) {
        *quality = 3;  /* Invalid */
    } else if (bad_checks >= 1) {
        *quality = 2;  /* Bad */
    } else if (suspect_checks >= 2) {
        *quality = 1;  /* Suspect */
    } else if (suspect_checks >= 1) {
        *quality = 1;  /* Suspect */
    } else {
        *quality = 0;  /* Good */
    }

    return 0;
}

/*===========================================================================
 * L8: Signal-to-Noise Ratio Estimation
 *
 * SNR = 10 * log10(P_signal / P_noise) dB
 *
 * For industrial measurements:
 *   - SNR > 40 dB: Excellent (laboratory-grade)
 *   - SNR 20-40 dB: Good (typical process instrument)
 *   - SNR 10-20 dB: Marginal (noisy environment, long cable runs)
 *   - SNR < 10 dB: Poor (signal barely distinguishable from noise)
 *
 * Reference: Rabiner & Gold, "Theory and Application of DSP" (1975)
 *===========================================================================*/

/**
 * Estimate signal-to-noise ratio.
 *
 * Uses a simple approach: signal power = signal_mean^2 + signal_variance,
 * noise power = minimum observed variance over quiet periods.
 *
 * @param signal_mean     Mean of signal over observation window
 * @param signal_variance Variance of signal
 * @param noise_variance  Estimated noise floor variance
 * @param snr_db          Output: SNR in dB
 * @param quality_label   Output: qualitative label (0=poor, 1=marginal, 2=good, 3=excellent)
 * @return 0 on success
 */
int pointio_signal_estimate_snr(double signal_mean, double signal_variance,
    double noise_variance, double *snr_db, int *quality_label)
{
    if (!snr_db || !quality_label) return -1;

    double signal_power = signal_mean * signal_mean + signal_variance;
    double noise_power = noise_variance;

    if (noise_power <= 0.0) {
        *snr_db = 100.0;  /* Infinite SNR (no noise) */
        *quality_label = 3;
        return 0;
    }

    if (signal_power <= 0.0) {
        *snr_db = -100.0;
        *quality_label = 0;
        return 0;
    }

    *snr_db = 10.0 * log10(signal_power / noise_power);

    if (*snr_db > 40.0) {
        *quality_label = 3;  /* Excellent */
    } else if (*snr_db > 20.0) {
        *quality_label = 2;  /* Good */
    } else if (*snr_db > 10.0) {
        *quality_label = 1;  /* Marginal */
    } else {
        *quality_label = 0;  /* Poor */
    }

    return 0;
}
