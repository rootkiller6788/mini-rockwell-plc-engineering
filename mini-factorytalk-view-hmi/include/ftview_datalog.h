/**
 * @file ftview_datalog.h
 * @brief FactoryTalk View HMI -- Data Logging & Trending (L1/L2/L3/L5)
 *
 * Knowledge points:
 *   L1.14 -- Data Log Model: tags, trigger, interval, storage file
 *   L1.15 -- Trend configuration: pens, time span, Y-axis scaling
 *   L2.10 -- Circular-buffer data logging with lossless overwrite signalling
 *   L2.11 -- Trigger types: periodic, on-change, on-condition
 *   L3.9  -- Time-series circular buffer with dual-index (write head + read cursor)
 *   L3.10 -- Floating-point deadband compression (swinging door algorithm)
 *   L5.7  -- Swinging-door compression for time-series storage reduction
 *   L5.8  -- Piecewise linear interpolation for trend rendering
 *   L7.2  -- OPC-HDA (Historical Data Access) style query interface
 *
 * Reference: Rockwell Automation FactoryTalk Historian SE
 *            OPC HDA 1.20 Specification
 *            Bristol, E. H. "Swinging Door Trending" (ISA Transactions, 1990)
 */

#ifndef FTVIEW_DATALOG_H
#define FTVIEW_DATALOG_H

#include "ftview_common.h"
#include "ftview_tag.h"
#include <stdint.h>
#include <stddef.h>

#define FTVIEW_DATALOG_MAX_MODELS      8
#define FTVIEW_DATALOG_MAX_TAGS       16
#define FTVIEW_DATALOG_BUFFER_SIZE   256
#define FTVIEW_DATALOG_TREND_MAX_PENS 8
#define FTVIEW_DATALOG_FILE_PATH_LEN 256

/* =====================================================================
 * L2.11 -- Trigger Types
 * ===================================================================== */

typedef enum {
    FTVIEW_DATALOG_TRIG_PERIODIC    = 0,  /**< fixed interval sampling */
    FTVIEW_DATALOG_TRIG_ON_CHANGE   = 1,  /**< log when value changes > deadband */
    FTVIEW_DATALOG_TRIG_ON_CONDITION= 2,  /**< log when condition tag is true */
    FTVIEW_DATALOG_TRIG_ON_DEMAND   = 3   /**< manual snapshot trigger */
} ftview_datalog_trigger_t;

/* =====================================================================
 * L1.14 -- Data Log Model
 * ===================================================================== */

typedef struct {
    uint32_t                id;
    char                    name[64];
    char                    file_path[FTVIEW_DATALOG_FILE_PATH_LEN];
    uint32_t                tag_ids[FTVIEW_DATALOG_MAX_TAGS];
    uint32_t                tag_count;
    ftview_datalog_trigger_t trigger_type;
    uint32_t                interval_ms;       /**< for periodic trigger */
    double                  deadband_pct;      /**< for on-change trigger (% of span) */
    uint32_t                condition_tag_id;  /**< for on-condition trigger */
    bool                    enabled;
    uint32_t                max_samples;       /**< buffer capacity in samples */
    bool                    loss_notify;       /**< signal when overwriting old data */
} ftview_datalog_model_t;

/* =====================================================================
 * L1.15 -- Trend Configuration
 * ===================================================================== */

typedef struct {
    char                    tag_name[FTVIEW_TAG_NAME_MAX_LEN];
    ftview_color_t          pen_color;
    double                  pen_width;         /**< line thickness in px */
    double                  y_min;             /**< Y-axis minimum (engineering units) */
    double                  y_max;             /**< Y-axis maximum */
    bool                    auto_scale;        /**< auto-range from data */
} ftview_trend_pen_t;

typedef struct {
    uint32_t                id;
    char                    name[64];
    uint32_t                datalog_id;        /**< source data log model */
    ftview_trend_pen_t      pens[FTVIEW_DATALOG_TREND_MAX_PENS];
    uint32_t                pen_count;
    uint32_t                time_span_ms;      /**< visible time window */
    bool                    live_mode;         /**< auto-scroll to latest */
    double                  cursor_time_ms;    /**< manual cursor position */
} ftview_trend_config_t;

/* =====================================================================
 * L3.9 -- Time-series Data Point
 * ===================================================================== */

typedef struct {
    int64_t                 timestamp_ms;
    double                  values[FTVIEW_DATALOG_MAX_TAGS];
    uint32_t                tag_count;
    ftview_quality_t        qualities[FTVIEW_DATALOG_MAX_TAGS];
} ftview_datalog_sample_t;

/* =====================================================================
 * L3.9 -- Circular Buffer Data Log Store
 * ===================================================================== */

typedef struct {
    ftview_datalog_sample_t buffer[FTVIEW_DATALOG_BUFFER_SIZE];
    uint32_t                write_idx;        /**< next write position */
    uint32_t                count;            /**< valid samples (<= FTVIEW_DATALOG_BUFFER_SIZE) */
    bool                    wrapped;          /**< ring buffer has wrapped around */
    uint32_t                loss_count;       /**< samples lost to overwrite */
} ftview_datalog_store_t;

/* =====================================================================
 * Data Log Manager
 * ===================================================================== */

typedef struct {
    ftview_datalog_model_t  models[FTVIEW_DATALOG_MAX_MODELS];
    uint32_t                model_count;
    ftview_datalog_store_t  stores[FTVIEW_DATALOG_MAX_MODELS];
    ftview_trend_config_t   trends[FTVIEW_DATALOG_MAX_MODELS];
    uint32_t                trend_count;
} ftview_datalog_mgr_t;

/* ───────── API ───────── */

void ftview_datalog_mgr_init(ftview_datalog_mgr_t *mgr);

/** L1.14 -- Create a data log model */
ftview_err_t ftview_datalog_create_model(ftview_datalog_mgr_t *mgr,
                                          const ftview_datalog_model_t *model);

/** L2.10 -- Log a sample (called by periodic timer or trigger evaluator).
 *  Returns the number of samples lost to overwrite in this operation. */
uint32_t ftview_datalog_sample(ftview_datalog_mgr_t *mgr,
                                uint32_t model_id,
                                const ftview_datalog_sample_t *sample);

/** L2.11 -- Evaluate whether a trigger condition is met */
bool ftview_datalog_eval_trigger(const ftview_datalog_model_t *model,
                                  const ftview_datalog_sample_t *current,
                                  const ftview_datalog_sample_t *previous,
                                  int64_t now_ms, int64_t *last_trigger_ms);

/** L3.10 / L5.7 -- Swinging-door compression filter.
 *  Determines if a new data point should be stored, or if it falls
 *  within the compression band of the previous stored point.
 *  Returns: true if the point should be stored (outside compression band). */
bool ftview_datalog_swinging_door(const ftview_datalog_sample_t *last_stored,
                                   const ftview_datalog_sample_t *candidate,
                                   uint32_t tag_idx,
                                   double compression_deviation);

/** Get samples in time range (for OPC-HDA ReadRaw style query) */
uint32_t ftview_datalog_query_range(const ftview_datalog_mgr_t *mgr,
                                     uint32_t model_id,
                                     int64_t start_ms, int64_t end_ms,
                                     ftview_datalog_sample_t *out,
                                     uint32_t max_out);

/** L5.8 -- Piecewise linear interpolation between two samples.
 *  Given samples at t0 and t1 (with t0 < t1), interpolate value of tag_idx
 *  at time t (t0 <= t <= t1). Handles quality: returns Good only if both
 *  endpoints are Good. */
ftview_err_t ftview_datalog_interpolate(const ftview_datalog_sample_t *s0,
                                         const ftview_datalog_sample_t *s1,
                                         uint32_t tag_idx,
                                         int64_t t,
                                         double *value_out,
                                         ftview_quality_t *quality_out);

/** Create a trend configuration */
ftview_err_t ftview_datalog_create_trend(ftview_datalog_mgr_t *mgr,
                                          const ftview_trend_config_t *trend);

/** L7.2 -- Query data for trend rendering (resampled to pixel_width points) */
uint32_t ftview_datalog_trend_query(const ftview_datalog_mgr_t *mgr,
                                     uint32_t trend_id,
                                     uint32_t pen_idx,
                                     int64_t start_ms, int64_t end_ms,
                                     uint32_t pixel_width,
                                     double *values_out,
                                     ftview_quality_t *qualities_out);

/** Compute statistics over a time range (min, max, avg, stddev for one tag) */
ftview_err_t ftview_datalog_statistics(const ftview_datalog_mgr_t *mgr,
                                        uint32_t model_id,
                                        uint32_t tag_idx,
                                        int64_t start_ms, int64_t end_ms,
                                        double *min_out, double *max_out,
                                        double *avg_out, double *stddev_out);

#endif /* FTVIEW_DATALOG_H */
