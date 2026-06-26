/**
 * @file plantpax_historian.c
 * @brief PlantPAx Historian Implementation ? Swinging Door Compression, Retrieval, Statistics
 *
 * Implements Bristol's swinging door compression algorithm, deadband
 * compression, interpolated data retrieval, summary statistics, EWMA,
 * and step change detection for time-series process data.
 */
#include "plantpax_historian.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ---------------------------------------------------------------------------
 * L1: Historian Point Initialization
 * ------------------------------------------------------------------------- */

void pax_hist_point_init(pax_hist_point_t *point, uint32_t id,
                          const char *tag, const char *description,
                          pax_hist_data_type_t data_type,
                          const char *unit, double range_min, double range_max)
{
    if (point == NULL) return;

    memset(point, 0, sizeof(pax_hist_point_t));

    point->point_id = id;
    if (tag != NULL) {
        strncpy(point->tag_name, tag, sizeof(point->tag_name) - 1);
    }
    if (description != NULL) {
        strncpy(point->description, description,
                sizeof(point->description) - 1);
    }
    point->data_type = data_type;
    if (unit != NULL) {
        strncpy(point->engineering_unit, unit,
                sizeof(point->engineering_unit) - 1);
    }
    point->range_min = range_min;
    point->range_max = range_max;
    point->compression = PAX_HIST_COMPRESS_SWINGING_DOOR;
    point->compression_deviation = PAX_HIST_DEFAULT_DEVIATION;
    point->exception_deviation = PAX_HIST_DEFAULT_DEVIATION;
    point->exc_deviation_pct = PAX_HIST_DEFAULT_DEVIATION_PCT;
    point->scan_period_ms = PAX_HIST_DEFAULT_SCAN_MS;
    point->archive_period_ms = 60000;
    point->is_active = false;
    point->total_samples = 0;
    point->archived_samples = 0;
    point->last_value = 0.0;
    point->last_timestamp_ms = 0;
    point->last_quality = PAX_IO_QUALITY_GOOD;
}

/* ---------------------------------------------------------------------------
 * L3: Add Sample with Exception Reporting
 *
 * Exception reporting only stores a value if it has changed
 * significantly from the previous stored value. This reduces
 * storage by omitting redundant data while preserving fidelity.
 *
 * Exception criteria (OR logic):
 *   1. Absolute value change > exception_deviation
 *   2. Percentage change > exc_deviation_pct of span
 *   3. Quality changed from last stored quality
 *   4. Time since last stored > max_time_without_sample
 * ------------------------------------------------------------------------- */

int pax_hist_add_sample(pax_hist_point_t *point,
                         double value, uint64_t timestamp_ms,
                         pax_io_quality_t quality)
{
    if (point == NULL) return 0;

    point->total_samples++;

    /* Always store if quality changed or this is first sample */
    if (point->total_samples == 1 || quality != point->last_quality) {
        point->last_value = value;
        point->last_timestamp_ms = timestamp_ms;
        point->last_quality = quality;
        point->archived_samples++;
        return 1;
    }

    /* Check exception deviation */
    double delta = fabs(value - point->last_value);
    double span = point->range_max - point->range_min;
    double pct_delta = (span > 1e-9) ? (delta / span) * 100.0 : 0.0;

    bool store_sample = false;

    if (delta >= point->exception_deviation) {
        store_sample = true;
    }
    if (pct_delta >= point->exc_deviation_pct) {
        store_sample = true;
    }

    /* Force store after 10x scan period to ensure data freshness */
    if (timestamp_ms - point->last_timestamp_ms >
        10 * (uint64_t)point->scan_period_ms) {
        store_sample = true;
    }

    if (store_sample) {
        point->last_value = value;
        point->last_timestamp_ms = timestamp_ms;
        point->last_quality = quality;
        point->archived_samples++;
        return 1;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * L5: Swinging Door Compression (Bristol 1990)
 *
 * Algorithm:
 *   Given compression deviation E, for each point (t_i, v_i):
 *
 *   1. For the first point after an archive event, record it.
 *   2. Compute "upper door" slope: (v_i - (v_prev + E)) / (t_i - t_prev)
 *   3. Compute "lower door" slope: (v_i - (v_prev - E)) / (t_i - t_prev)
 *   4. Track the most restrictive upper and lower slopes.
 *   5. When upper_slope_min < lower_slope_max, the doors have crossed:
 *      archive the previous point and restart.
 *
 * This preserves the fidelity of data while compressing
 * straight-line or slowly-varying segments.
 * ------------------------------------------------------------------------- */

uint32_t pax_hist_compress_swinging_door(pax_hist_sample_t *samples,
                                          uint32_t num_samples,
                                          double compression_deviation)
{
    if (samples == NULL || num_samples < 3) return num_samples;

    pax_hist_sample_t *output = samples;  /* In-place compression */
    uint32_t output_count = 0;

    if (num_samples == 0) return 0;

    /* Always keep the first point */
    output[output_count++] = samples[0];

    if (num_samples == 1) return 1;

    uint32_t i;
    double upper_slope_min = 1e308;
    double lower_slope_max = -1e308;
    uint64_t t_prev = samples[0].timestamp_ms;
    double v_prev = samples[0].value;

    for (i = 1; i < num_samples; i++) {
        uint64_t t_i = samples[i].timestamp_ms;
        double v_i = samples[i].value;

        /* Avoid division by zero for samples with same timestamp */
        if (t_i == t_prev) continue;

        double dt_ms = (double)(t_i - t_prev);
        double upper_slope = (v_i - (v_prev + compression_deviation)) / dt_ms;
        double lower_slope = (v_i - (v_prev - compression_deviation)) / dt_ms;

        /* Track the most restrictive slopes */
        if (upper_slope < upper_slope_min) upper_slope_min = upper_slope;
        if (lower_slope > lower_slope_max) lower_slope_max = lower_slope;

        /* Check if doors have crossed (or will cross) */
        if (upper_slope_min < lower_slope_max) {
            /* Archive the previous point (with bounds check) */
            if (output_count < num_samples) {
                output[output_count++] = samples[i - 1];
            }

            /* Reset for next segment: capture data BEFORE potential overwrite */
            uint64_t cached_ts = samples[i - 1].timestamp_ms;
            double cached_val = samples[i - 1].value;
            t_prev = cached_ts;
            v_prev = cached_val;
            upper_slope_min = 1e308;
            lower_slope_max = -1e308;

            /* Re-evaluate this point against the new base */
            dt_ms = (double)(t_i - t_prev);
            if (dt_ms > 0) {
                upper_slope = (v_i - (v_prev + compression_deviation)) / dt_ms;
                lower_slope = (v_i - (v_prev - compression_deviation)) / dt_ms;
                upper_slope_min = upper_slope;
                lower_slope_max = lower_slope;
            }
        }
    }

    /* Always keep the last point (with bounds check) */
    if (output_count < num_samples) {
        output[output_count++] = samples[num_samples - 1];
    }

    return output_count;
}

/* ---------------------------------------------------------------------------
 * L5: Deadband Compression
 *
 * Simpler alternative to swinging door:
 * Only store a sample when the value change from the last
 * stored value exceeds the deadband.
 * ------------------------------------------------------------------------- */

uint32_t pax_hist_compress_deadband(pax_hist_sample_t *samples,
                                     uint32_t num_samples,
                                     double deadband)
{
    if (samples == NULL || num_samples == 0) return 0;

    pax_hist_sample_t *output = samples;  /* In-place */
    uint32_t output_count = 0;
    uint32_t i;

    double last_stored_value = samples[0].value;
    output[output_count++] = samples[0];  /* Always keep first */

    for (i = 1; i < num_samples; i++) {
        double delta = fabs(samples[i].value - last_stored_value);

        if (delta >= deadband) {
            output[output_count++] = samples[i];
            last_stored_value = samples[i].value;
        }
    }

    /* Ensure last point is stored if not already */
    if (output_count > 0 &&
        output[output_count - 1].timestamp_ms !=
        samples[num_samples - 1].timestamp_ms) {
        output[output_count++] = samples[num_samples - 1];
    }

    return output_count;
}

/* ---------------------------------------------------------------------------
 * L5: Interpolated Data Retrieval
 *
 * Given a set of stored samples, generate interpolated values
 * at a regular time interval between start_ms and end_ms.
 *
 * Uses linear interpolation between the two nearest stored samples.
 * For timestamps before the first sample: uses first sample value.
 * For timestamps after the last sample: uses last sample value.
 * ------------------------------------------------------------------------- */

uint32_t pax_hist_retrieve_interpolated(const pax_hist_sample_t *samples,
                                         uint32_t num_samples,
                                         uint64_t start_ms, uint64_t end_ms,
                                         uint32_t interval_ms,
                                         double *output, uint32_t max_output)
{
    if (samples == NULL || output == NULL || num_samples == 0) return 0;
    if (interval_ms == 0) return 0;
    if (end_ms <= start_ms) return 0;

    uint32_t count = 0;
    uint64_t t;
    uint32_t sample_idx = 0;

    for (t = start_ms; t <= end_ms && count < max_output; t += interval_ms) {
        /* Find surrounding samples */
        while (sample_idx + 1 < num_samples &&
               samples[sample_idx + 1].timestamp_ms <= t) {
            sample_idx++;
        }

        /* If at last sample, all future values use last value */
        if (sample_idx + 1 >= num_samples ||
            samples[sample_idx + 1].timestamp_ms == samples[sample_idx].timestamp_ms) {
            output[count++] = samples[sample_idx].value;
            continue;
        }

        /* Linear interpolation between sample_idx and sample_idx+1 */
        uint64_t t0 = samples[sample_idx].timestamp_ms;
        uint64_t t1 = samples[sample_idx + 1].timestamp_ms;

        if (t <= t0) {
            output[count++] = samples[0].value;
        } else if (t >= t1) {
            output[count++] = samples[num_samples - 1].value;
        } else {
            double frac = (double)(t - t0) / (double)(t1 - t0);
            double v0 = samples[sample_idx].value;
            double v1 = samples[sample_idx + 1].value;
            output[count++] = v0 + frac * (v1 - v0);
        }
    }

    return count;
}

/* ---------------------------------------------------------------------------
 * L5: Summary Statistics Computation
 *
 * Computes statistical summary over a time window:
 *   - Average: mean of all in-window values
 *   - Min/Max: range
 *   - Standard deviation: sqrt(E[(x - mu)^2])
 *   - Total: time-integrated (area under curve using trapezoidal rule)
 *   - Delta: end value - start value
 *   - Percent good: fraction of good-quality samples
 * ------------------------------------------------------------------------- */

int pax_hist_compute_summary(const pax_hist_sample_t *samples,
                              uint32_t num_samples,
                              uint64_t start_ms, uint64_t end_ms,
                              pax_hist_summary_t *summary)
{
    if (samples == NULL || summary == NULL || num_samples == 0) return -1;

    memset(summary, 0, sizeof(pax_hist_summary_t));

    summary->start_time_ms = start_ms;
    summary->end_time_ms = end_ms;

    double sum = 0.0;
    double sum_sq = 0.0;
    uint32_t in_window = 0;
    uint32_t good_count = 0;
    double min_val = 1e308;
    double max_val = -1e308;
    double first_val = 0.0;
    double last_val = 0.0;
    bool first_found = false;
    double total_area = 0.0;

    uint32_t i;
    for (i = 0; i < num_samples; i++) {
        if (samples[i].timestamp_ms < start_ms ||
            samples[i].timestamp_ms > end_ms) {
            continue;
        }

        double v = samples[i].value;

        sum += v;
        sum_sq += v * v;

        if (!first_found) {
            first_val = v;
            first_found = true;
        }
        last_val = v;

        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
        in_window++;

        if (samples[i].quality == PAX_IO_QUALITY_GOOD) {
            good_count++;
        }

        /* Trapezoidal integration for area under curve */
        if (i > 0 && samples[i - 1].timestamp_ms >= start_ms) {
            double dt = (double)(samples[i].timestamp_ms -
                                 samples[i - 1].timestamp_ms) / 1000.0;
            double avg_v = (samples[i].value + samples[i - 1].value) / 2.0;
            total_area += avg_v * dt;
        }
    }

    if (in_window == 0) return 0;

    summary->average = sum / (double)in_window;
    summary->minimum = min_val;
    summary->maximum = max_val;

    /* Standard deviation */
    double variance = (sum_sq / (double)in_window) -
                      summary->average * summary->average;
    if (variance < 0.0) variance = 0.0;
    summary->stddev = sqrt(variance);

    summary->total = total_area;
    summary->delta = last_val - first_val;
    summary->count = in_window;
    summary->percent_good = (double)good_count / (double)in_window * 100.0;

    return 0;
}

/* ---------------------------------------------------------------------------
 * L3: Compression Ratio
 * ------------------------------------------------------------------------- */

double pax_hist_compression_ratio(uint32_t raw_count, uint32_t compressed_count)
{
    if (compressed_count == 0) return 0.0;
    if (raw_count == 0) return 0.0;

    return (double)raw_count / (double)compressed_count;
}

/* ---------------------------------------------------------------------------
 * L5: Time-Weighted Average
 *
 * TWA = sum(v_i * dt_i) / sum(dt_i)
 *
 * Each value is weighted by the time interval it represents.
 * This correctly handles irregularly-spaced samples.
 * ------------------------------------------------------------------------- */

double pax_hist_time_weighted_average(const pax_hist_sample_t *samples,
                                       uint32_t num_samples)
{
    if (samples == NULL || num_samples < 2) {
        if (num_samples == 1 && samples != NULL) return samples[0].value;
        return 0.0;
    }

    double weighted_sum = 0.0;
    double total_time = 0.0;
    uint32_t i;

    for (i = 1; i < num_samples; i++) {
        double dt = (double)(samples[i].timestamp_ms -
                             samples[i - 1].timestamp_ms) / 1000.0;
        if (dt <= 0.0) continue;

        /* Use average of two endpoints to approximate integral */
        double avg_val = (samples[i].value + samples[i - 1].value) / 2.0;
        weighted_sum += avg_val * dt;
        total_time += dt;
    }

    if (total_time <= 0.0) return samples[0].value;
    return weighted_sum / total_time;
}

/* ---------------------------------------------------------------------------
 * L5: Step Change Detection
 *
 * Uses first derivative (finite backward difference):
 *   delta = v[i] - v[i-1]
 *
 * If |delta| >= step_threshold, a step change is detected at index i.
 *
 * This is useful for:
 *   - Detecting process upsets in disturbance analysis
 *   - Identifying batch phase transitions in ISA-88 procedures
 *   - Triggering event-based data capture
 * ------------------------------------------------------------------------- */

uint32_t pax_hist_detect_step_changes(const pax_hist_sample_t *samples,
                                       uint32_t num_samples,
                                       double step_threshold,
                                       uint32_t *step_indices,
                                       uint32_t max_steps)
{
    if (samples == NULL || step_indices == NULL || num_samples < 2) {
        return 0;
    }

    uint32_t step_count = 0;
    uint32_t i;

    for (i = 1; i < num_samples && step_count < max_steps; i++) {
        double delta = fabs(samples[i].value - samples[i - 1].value);

        if (delta >= step_threshold) {
            step_indices[step_count++] = i;
            i++;  /* Skip one sample to avoid double-counting */
        }
    }

    return step_count;
}

/* ---------------------------------------------------------------------------
 * L8: Exponential Weighted Moving Average (EWMA)
 *
 * EWMA = alpha * current_value + (1 - alpha) * previous_EWMA
 *
 * Parameters:
 *   alpha = 0: No response to new data (infinite memory)
 *   alpha = 1: Instant response (no memory)
 *   alpha = 2/(N+1): Equivalent to N-period simple moving average
 *
 * EWMA is widely used in:
 *   - Process data smoothing
 *   - Statistical process control (SPC) charts
 *   - Sensor fault detection (residual analysis)
 * ------------------------------------------------------------------------- */

double pax_hist_ewma(double current_value, double previous_ewma, double alpha)
{
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;

    return alpha * current_value + (1.0 - alpha) * previous_ewma;
}

/* ---------------------------------------------------------------------------
 * L3: Timestamp Validation
 * ------------------------------------------------------------------------- */

bool pax_hist_validate_timestamp(uint64_t timestamp_ms,
                                  uint64_t archive_start_ms,
                                  uint64_t archive_end_ms)
{
    if (archive_end_ms <= archive_start_ms) return false;
    if (timestamp_ms < archive_start_ms) return false;
    if (timestamp_ms > archive_end_ms) return false;

    return true;
}
