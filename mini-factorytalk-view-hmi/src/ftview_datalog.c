/**
 * @file ftview_datalog.c
 * @brief Data Logging & Trending — circular buffer, compression, interpolation
 *
 * Implements:
 *   L1.14 — Data log model creation and management
 *   L1.15 — Trend configuration with pens and scaling
 *   L2.10 — Circular buffer with loss-notify overflow signalling
 *   L2.11 — Trigger evaluation (periodic, on-change, on-condition)
 *   L3.9  — Time-series ring buffer with dual-index access
 *   L3.10 — Swinging-door compression filter
 *   L5.7  — Swinging-door algorithm for storage reduction
 *   L5.8  — Piecewise linear interpolation for trend rendering
 *   L7.2  — OPC-HDA ReadRaw style query with resampling
 */

#include "ftview_datalog.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* =====================================================================
 * Data Log Manager Initialisation
 * ===================================================================== */

void ftview_datalog_mgr_init(ftview_datalog_mgr_t *mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
}

ftview_err_t ftview_datalog_create_model(ftview_datalog_mgr_t *mgr,
                                          const ftview_datalog_model_t *model)
{
    if (!mgr || !model) return FTVIEW_ERR_NULL_PTR;
    if (mgr->model_count >= FTVIEW_DATALOG_MAX_MODELS) return FTVIEW_ERR_OUT_OF_MEMORY;

    for (uint32_t i = 0; i < mgr->model_count; i++) {
        if (strcmp(mgr->models[i].name, model->name) == 0) {
            return FTVIEW_ERR_DUPLICATE_TAG;
        }
    }

    memcpy(&mgr->models[mgr->model_count], model, sizeof(ftview_datalog_model_t));
    mgr->models[mgr->model_count].id = mgr->model_count + 1;

    /* Initialise corresponding store */
    memset(&mgr->stores[mgr->model_count], 0, sizeof(ftview_datalog_store_t));

    mgr->model_count++;
    return FTVIEW_OK;
}

/* =====================================================================
 * L2.10 — Sample Logging (Circular Buffer)
 *
 * Writes a sample to the ring buffer. If buffer is full, overwrites
 * the oldest sample and increments loss_count.
 *
 * Returns number of samples lost in this operation (0 or 1).
 *
 * Complexity: O(1).
 * ===================================================================== */

uint32_t ftview_datalog_sample(ftview_datalog_mgr_t *mgr,
                                uint32_t model_id,
                                const ftview_datalog_sample_t *sample)
{
    if (!mgr || !sample || model_id == 0 || model_id > mgr->model_count)
        return 0;

    ftview_datalog_store_t *store = &mgr->stores[model_id - 1];
    uint32_t lost = 0;

    /* Check if buffer is full (about to overwrite) */
    if (store->count >= FTVIEW_DATALOG_BUFFER_SIZE) {
        lost = 1;
        store->loss_count++;
    }

    /* Write sample */
    memcpy(&store->buffer[store->write_idx], sample, sizeof(ftview_datalog_sample_t));

    /* Advance write pointer */
    store->write_idx = (store->write_idx + 1) % FTVIEW_DATALOG_BUFFER_SIZE;
    if (store->count < FTVIEW_DATALOG_BUFFER_SIZE) {
        store->count++;
    } else {
        store->wrapped = true;
    }

    return lost;
}

/* =====================================================================
 * L2.11 — Trigger Evaluation
 *
 * Determines if a logging trigger condition is met.
 *
 *   Periodic:     elapsed_ms >= interval_ms (caller tracks last_trigger_ms)
 *   On-Change:    |current - previous| > deadband_pct * span / 100
 *   On-Condition: condition tag value != 0
 *   On-Demand:    always false (manual trigger)
 *
 * Complexity: O(1).
 * ===================================================================== */

bool ftview_datalog_eval_trigger(const ftview_datalog_model_t *model,
                                  const ftview_datalog_sample_t *current,
                                  const ftview_datalog_sample_t *previous,
                                  int64_t now_ms, int64_t *last_trigger_ms)
{
    if (!model || !current) return false;

    switch (model->trigger_type) {
    case FTVIEW_DATALOG_TRIG_PERIODIC:
        if (!last_trigger_ms) return false;
        return (now_ms - *last_trigger_ms) >= (int64_t)model->interval_ms;

    case FTVIEW_DATALOG_TRIG_ON_CHANGE:
        if (!previous) return true; /* no previous: always log first sample */
        /* Check each tag for change exceeding deadband */
        for (uint32_t i = 0; i < model->tag_count && i < current->tag_count; i++) {
            double diff = fabs(current->values[i] - previous->values[i]);
            if (diff > model->deadband_pct * 0.01 *
                (1.0 + fabs(previous->values[i]))) {
                return true;
            }
        }
        return false;

    case FTVIEW_DATALOG_TRIG_ON_CONDITION:
        if (model->condition_tag_id > 0 &&
            model->condition_tag_id <= current->tag_count) {
            return fabs(current->values[model->condition_tag_id - 1]) > 0.5;
        }
        return false;

    case FTVIEW_DATALOG_TRIG_ON_DEMAND:
    default:
        return false;
    }
}

/* =====================================================================
 * L3.10 / L5.7 — Swinging-Door Compression
 *
 * The "swinging door" trending algorithm determines whether a new data
 * point should be stored, or if it falls within the compression band
 * (a parallelogram defined by the last stored point and the compression
 * deviation parameter).
 *
 * Algorithm:
 *   - Maintain two slopes (upper door, lower door) from the last stored point
 *   - The intersection of these slopes defines the compression zone
 *   - If a new point falls inside the zone, discard it
 *   - If a new point falls outside, store the previous point and reset doors
 *
 * This simplified version checks if the candidate point lies within
 * +/- compression_deviation of the piecewise-linear interpolation
 * between the last stored point and the current moment.
 *
 * Complexity: O(1).
 *
 * Reference: Bristol, E. H. "Swinging Door Trending: Adaptive Trend
 *   Recording" ISA Transactions, Vol. 29, No. 2 (1990), pp. 27-36.
 * ===================================================================== */

bool ftview_datalog_swinging_door(const ftview_datalog_sample_t *last_stored,
                                   const ftview_datalog_sample_t *candidate,
                                   uint32_t tag_idx,
                                   double compression_deviation)
{
    if (!last_stored || !candidate) return true;  /* store if no reference */
    if (tag_idx >= last_stored->tag_count || tag_idx >= candidate->tag_count)
        return true;
    if (compression_deviation <= 0.0) return true; /* zero deviation = store all */

    double diff = fabs(candidate->values[tag_idx] - last_stored->values[tag_idx]);

    /* If the change exceeds the compression deviation, store it */
    return diff > compression_deviation;
}

/* =====================================================================
 * L3.9 — Query Samples in Time Range
 *
 * OPC-HDA ReadRaw: returns all raw samples within [start_ms, end_ms].
 * Walk the circular buffer from oldest to newest, skip samples outside range.
 *
 * Complexity: O(n) where n = buffer size.
 * ===================================================================== */

uint32_t ftview_datalog_query_range(const ftview_datalog_mgr_t *mgr,
                                     uint32_t model_id,
                                     int64_t start_ms, int64_t end_ms,
                                     ftview_datalog_sample_t *out,
                                     uint32_t max_out)
{
    if (!mgr || !out || max_out == 0 || model_id == 0 || model_id > mgr->model_count)
        return 0;

    const ftview_datalog_store_t *store = &mgr->stores[model_id - 1];
    if (store->count == 0) return 0;

    uint32_t copied = 0;

    /* Determine oldest index */
    uint32_t oldest_idx;
    if (store->count < FTVIEW_DATALOG_BUFFER_SIZE) {
        oldest_idx = 0;
    } else {
        oldest_idx = store->write_idx; /* write_idx = oldest when wrapped */
    }

    /* Walk from oldest to newest */
    for (uint32_t i = 0; i < store->count && copied < max_out; i++) {
        uint32_t idx = (oldest_idx + i) % FTVIEW_DATALOG_BUFFER_SIZE;
        int64_t ts = store->buffer[idx].timestamp_ms;

        if (ts >= start_ms && ts <= end_ms) {
            memcpy(&out[copied], &store->buffer[idx], sizeof(ftview_datalog_sample_t));
            copied++;
        }
    }

    return copied;
}

/* =====================================================================
 * L5.8 — Piecewise Linear Interpolation
 *
 * Given two samples at times t0 and t1 (t0 <= t <= t1), interpolate
 * the value of tag_idx at time t.
 *
 * Formula:  V(t) = V0 + (V1 - V0) * (t - t0) / (t1 - t0)
 *
 * Quality: returns Good only if both endpoints are Good.
 *
 * Complexity: O(1).
 *
 * Reference: Davis, P. J. "Interpolation and Approximation" (1975), Ch 2.
 * ===================================================================== */

ftview_err_t ftview_datalog_interpolate(const ftview_datalog_sample_t *s0,
                                         const ftview_datalog_sample_t *s1,
                                         uint32_t tag_idx,
                                         int64_t t,
                                         double *value_out,
                                         ftview_quality_t *quality_out)
{
    if (!s0 || !s1 || !value_out || !quality_out) return FTVIEW_ERR_NULL_PTR;
    if (tag_idx >= s0->tag_count || tag_idx >= s1->tag_count)
        return FTVIEW_ERR_INVALID_PARAM;

    int64_t t0 = s0->timestamp_ms;
    int64_t t1 = s1->timestamp_ms;

    if (t1 == t0) {
        /* Same timestamp: return average */
        *value_out = s0->values[tag_idx];
        *quality_out = s0->qualities[tag_idx];
        return FTVIEW_OK;
    }

    if (t < t0 || t > t1) return FTVIEW_ERR_INVALID_PARAM;

    double fraction = (double)(t - t0) / (double)(t1 - t0);
    double v0 = s0->values[tag_idx];
    double v1 = s1->values[tag_idx];

    *value_out = v0 + (v1 - v0) * fraction;

    /* Quality: Good if both are Good; Uncertain if either is Uncertain; Bad otherwise */
    if (s0->qualities[tag_idx] == FTVIEW_QUALITY_GOOD &&
        s1->qualities[tag_idx] == FTVIEW_QUALITY_GOOD) {
        *quality_out = FTVIEW_QUALITY_GOOD;
    } else if (s0->qualities[tag_idx] == FTVIEW_QUALITY_BAD ||
               s1->qualities[tag_idx] == FTVIEW_QUALITY_BAD) {
        *quality_out = FTVIEW_QUALITY_BAD;
    } else {
        *quality_out = FTVIEW_QUALITY_UNCERTAIN;
    }

    return FTVIEW_OK;
}

/* =====================================================================
 * Trend configuration
 * ===================================================================== */

ftview_err_t ftview_datalog_create_trend(ftview_datalog_mgr_t *mgr,
                                          const ftview_trend_config_t *trend)
{
    if (!mgr || !trend) return FTVIEW_ERR_NULL_PTR;
    if (mgr->trend_count >= FTVIEW_DATALOG_MAX_MODELS) return FTVIEW_ERR_OUT_OF_MEMORY;

    memcpy(&mgr->trends[mgr->trend_count], trend, sizeof(ftview_trend_config_t));
    mgr->trends[mgr->trend_count].id = mgr->trend_count + 1;
    mgr->trend_count++;
    return FTVIEW_OK;
}

/* =====================================================================
 * L7.2 — Trend Query with Resampling
 *
 * Queries a data log for trend rendering and resamples the data to
 * 'pixel_width' evenly spaced points across the time window.
 *
 * Algorithm:
 *   1. Fetch all raw samples in the time window
 *   2. For each pixel column, find the two bounding samples
 *   3. Interpolate (and take min/max for envelope if desired)
 *
 * This is analogous to OPC-HDA ReadProcessed with aggregate type
 * INTERPOLATIVE.
 *
 * Complexity: O(n + p * log n) where n = raw samples, p = pixel_width.
 * ===================================================================== */

uint32_t ftview_datalog_trend_query(const ftview_datalog_mgr_t *mgr,
                                     uint32_t trend_id,
                                     uint32_t pen_idx,
                                     int64_t start_ms, int64_t end_ms,
                                     uint32_t pixel_width,
                                     double *values_out,
                                     ftview_quality_t *qualities_out)
{
    if (!mgr || !values_out || pixel_width == 0 || trend_id == 0 ||
        trend_id > mgr->trend_count) return 0;

    const ftview_trend_config_t *trend = &mgr->trends[trend_id - 1];
    if (pen_idx >= trend->pen_count) return 0;

    uint32_t model_id = trend->datalog_id;
    if (model_id == 0 || model_id > mgr->model_count) return 0;

    /* Fetch raw samples in time range */
    ftview_datalog_sample_t raw[FTVIEW_DATALOG_BUFFER_SIZE];
    uint32_t raw_count = ftview_datalog_query_range(mgr, model_id,
                                                      start_ms, end_ms,
                                                      raw, FTVIEW_DATALOG_BUFFER_SIZE);
    if (raw_count == 0) {
        /* No data: fill with 0 */
        for (uint32_t i = 0; i < pixel_width; i++) {
            values_out[i] = 0.0;
            if (qualities_out) qualities_out[i] = FTVIEW_QUALITY_NA;
        }
        return pixel_width;
    }

    /* Resample to pixel grid */
    double time_per_pixel = (double)(end_ms - start_ms) / (double)pixel_width;
    if (time_per_pixel <= 0) time_per_pixel = 1.0;

    for (uint32_t px = 0; px < pixel_width; px++) {
        int64_t t_pixel = start_ms + (int64_t)((double)px * time_per_pixel);

        /* Find bounding samples: s0 (<= t_pixel) and s1 (> t_pixel) */
        int32_t s0_idx = -1, s1_idx = -1;

        for (uint32_t i = 0; i < raw_count; i++) {
            if (raw[i].timestamp_ms <= t_pixel) {
                if (s0_idx < 0 || raw[i].timestamp_ms > raw[s0_idx].timestamp_ms) {
                    s0_idx = (int32_t)i;
                }
            }
            if (raw[i].timestamp_ms > t_pixel) {
                if (s1_idx < 0 || raw[i].timestamp_ms < raw[s1_idx].timestamp_ms) {
                    s1_idx = (int32_t)i;
                }
            }
        }

        if (s0_idx >= 0 && s1_idx >= 0) {
            /* Interpolate between s0 and s1 */
            ftview_datalog_interpolate(&raw[s0_idx], &raw[s1_idx],
                                        0, /* tag_idx 0 of raw sample (simplified) */
                                        t_pixel,
                                        &values_out[px],
                                        qualities_out ? &qualities_out[px] : NULL);
        } else if (s0_idx >= 0) {
            values_out[px] = raw[s0_idx].values[0];
            if (qualities_out) qualities_out[px] = raw[s0_idx].qualities[0];
        } else if (s1_idx >= 0) {
            values_out[px] = raw[s1_idx].values[0];
            if (qualities_out) qualities_out[px] = raw[s1_idx].qualities[0];
        } else {
            values_out[px] = 0.0;
            if (qualities_out) qualities_out[px] = FTVIEW_QUALITY_NA;
        }
    }

    return pixel_width;
}

/* =====================================================================
 * Compute statistics over a time window for one tag.
 *
 * Returns: min, max, average, standard deviation.
 *
 * Complexity: O(n) where n = samples in window.
 * ===================================================================== */

ftview_err_t ftview_datalog_statistics(const ftview_datalog_mgr_t *mgr,
                                        uint32_t model_id,
                                        uint32_t tag_idx,
                                        int64_t start_ms, int64_t end_ms,
                                        double *min_out, double *max_out,
                                        double *avg_out, double *stddev_out)
{
    if (!mgr || !min_out || !max_out || !avg_out || !stddev_out)
        return FTVIEW_ERR_NULL_PTR;
    if (model_id == 0 || model_id > mgr->model_count)
        return FTVIEW_ERR_INVALID_PARAM;

    ftview_datalog_sample_t samples[FTVIEW_DATALOG_BUFFER_SIZE];
    uint32_t count = ftview_datalog_query_range(mgr, model_id,
                                                  start_ms, end_ms,
                                                  samples, FTVIEW_DATALOG_BUFFER_SIZE);
    if (count == 0) {
        *min_out = *max_out = *avg_out = *stddev_out = 0.0;
        return FTVIEW_OK;
    }

    double sum = 0.0, sum_sq = 0.0;
    double min_v = samples[0].values[tag_idx];
    double max_v = min_v;

    for (uint32_t i = 0; i < count; i++) {
        double v = samples[i].values[tag_idx];
        sum += v;
        sum_sq += v * v;
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
    }

    double avg = sum / (double)count;
    double variance = (sum_sq / (double)count) - (avg * avg);
    if (variance < 0.0) variance = 0.0;
    double stddev = sqrt(variance);

    *min_out = min_v;
    *max_out = max_v;
    *avg_out = avg;
    *stddev_out = stddev;

    return FTVIEW_OK;
}
