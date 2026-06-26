/**
 * @file plantpax_historian.h
 * @brief PlantPAx Historian ? Time-Series Storage, Compression, Retrieval
 *
 * Module: mini-rockwell-plantpax-dcs
 * Knowledge Coverage: L1 Definitions, L3 Data Architecture, L5 Compression Algorithms
 *
 * FactoryTalk Historian (based on OSIsoft PI technology) provides
 * high-speed time-series data collection for PlantPAx DCS.
 *
 * Core concepts:
 *   - Tags/Points: Named data streams with metadata
 *   - Archive: Time-series storage with compression
 *   - Exception reporting: Only store changed values
 *   - Compression: Swinging door algorithm (Bristol 1990)
 *   - Retrieval: Interpolated, raw, summary, expression
 *
 * Swinging Door Compression:
 *   Given a series of (t, v) points, the algorithm defines a
 *   "compression deviation" (slack). As each new point arrives,
 *   it checks whether the point falls within the parallelogram
 *   defined by the deviation. If so, the previous point is
 *   discarded. If not, the previous point is stored and the
 *   process restarts.
 *
 * Reference:
 *   Bristol, E.H., "Swinging Door Trending", ISA Transactions, 1990
 *   OSIsoft PI Server Documentation
 *   FactoryTalk Historian SE User Guide
 * Curriculum: MIT 6.302, Purdue ME 575, Tsinghua Process Control
 */

#ifndef PLANTPAX_HISTORIAN_H
#define PLANTPAX_HISTORIAN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "plantpax_io_subsystem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1: Historian Core Definitions
 * ------------------------------------------------------------------------- */

/** Data types for historian points */
typedef enum {
    PAX_HIST_TYPE_FLOAT64 = 0,       /**< 64-bit floating point */
    PAX_HIST_TYPE_FLOAT32 = 1,       /**< 32-bit floating point */
    PAX_HIST_TYPE_INT32 = 2,         /**< 32-bit signed integer */
    PAX_HIST_TYPE_INT16 = 3,         /**< 16-bit signed integer */
    PAX_HIST_TYPE_DIGITAL = 4,       /**< Boolean/digital state */
    PAX_HIST_TYPE_STRING = 5         /**< Variable-length string */
} pax_hist_data_type_t;

/** Archive compression modes */
typedef enum {
    PAX_HIST_COMPRESS_NONE = 0,      /**< No compression (raw) */
    PAX_HIST_COMPRESS_SWINGING_DOOR = 1, /**< Swinging door algorithm */
    PAX_HIST_COMPRESS_DEADBAND = 2,  /**< Value change deadband */
    PAX_HIST_COMPRESS_TIME_SERIES = 3 /**< Periodic sampling */
} pax_hist_compression_t;

/** Exception reporting configuration */
typedef enum {
    PAX_HIST_EXC_NONE = 0,           /**< Report every scan */
    PAX_HIST_EXC_VALUE = 1,          /**< Report only on value change */
    PAX_HIST_EXC_DEADBAND = 2,       /**< Report when change > deadband */
    PAX_HIST_EXC_TIME = 3            /**< Report at fixed interval */
} pax_hist_exception_t;

/** Retrieval modes */
typedef enum {
    PAX_HIST_RETR_RAW = 0,           /**< All stored values */
    PAX_HIST_RETR_INTERPOLATED = 1,  /**< Interpolated at regular intervals */
    PAX_HIST_RETR_AVERAGE = 2,       /**< Time-weighted average */
    PAX_HIST_RETR_MIN = 3,           /**< Minimum in interval */
    PAX_HIST_RETR_MAX = 4,           /**< Maximum in interval */
    PAX_HIST_RETR_STDDEV = 5,        /**< Standard deviation in interval */
    PAX_HIST_RETR_COUNT = 6,         /**< Count of values in interval */
    PAX_HIST_RETR_DELTA = 7,         /**< Change over interval */
    PAX_HIST_RETR_TOTAL = 8          /**< Time-integrated total */
} pax_hist_retrieval_t;

/* ---------------------------------------------------------------------------
 * L1: Historian Data Structures
 * ------------------------------------------------------------------------- */

/** Single time-stamped data sample */
typedef struct {
    uint64_t timestamp_ms;           /**< Milliseconds since Unix epoch */
    double value;                    /**< Data value */
    pax_io_quality_t quality;        /**< Quality indicator */
    bool is_compressed;              /**< Flagged by compression algorithm */
} pax_hist_sample_t;

/** Historian point/tag definition */
typedef struct {
    uint32_t point_id;
    char tag_name[64];
    char description[256];
    pax_hist_data_type_t data_type;
    char engineering_unit[16];
    double range_min;
    double range_max;
    pax_hist_compression_t compression;
    double compression_deviation;    /**< Compression deviation (swinging door) */
    double exception_deviation;      /**< Exception reporting deadband */
    double exc_deviation_pct;        /**< Exception as percent of span */
    uint32_t scan_period_ms;         /**< Collection scan period */
    uint32_t archive_period_ms;      /**< Transfer to archive interval */
    bool is_active;                  /**< Point is actively collecting */
    uint64_t total_samples;
    uint64_t archived_samples;
    double last_value;
    uint64_t last_timestamp_ms;
    pax_io_quality_t last_quality;
} pax_hist_point_t;

/** Compressed archive segment */
typedef struct {
    uint64_t start_time_ms;
    uint64_t end_time_ms;
    uint32_t num_samples;
    pax_hist_sample_t *samples;       /**< Array of stored samples */
    double min_value;
    double max_value;
    double average_value;
    double stddev_value;
} pax_hist_archive_t;

/** Historian configuration */
typedef struct {
    uint32_t num_points;
    uint32_t archive_capacity;
    uint32_t max_samples_per_point;
    double disk_usage_mb;
    uint64_t oldest_timestamp_ms;
    uint64_t newest_timestamp_ms;
    uint64_t total_samples_stored;
    double compression_ratio;        /**< Compressed/raw sample ratio */
} pax_hist_config_t;

/** Summary statistics over a time window */
typedef struct {
    uint64_t start_time_ms;
    uint64_t end_time_ms;
    double average;
    double minimum;
    double maximum;
    double stddev;
    double total;                    /**< Time-integrated total */
    double delta;                    /**< End - start value */
    uint32_t count;
    double percent_good;             /**< Percentage of good quality values */
} pax_hist_summary_t;

/* ---------------------------------------------------------------------------
 * L1: Historian Constants
 * ------------------------------------------------------------------------- */

#define PAX_HIST_MAX_POINTS             100000
#define PAX_HIST_MAX_SAMPLES_PER_POINT  1000000
#define PAX_HIST_DEFAULT_DEVIATION       0.1
#define PAX_HIST_DEFAULT_DEVIATION_PCT   0.5
#define PAX_HIST_DEFAULT_SCAN_MS         1000
#define PAX_HIST_MAX_ARCHIVE_FILES       256
#define PAX_HIST_COMPRESSION_TARGET      10.0  /**< Target 10:1 compression */

/* ---------------------------------------------------------------------------
 * L3/L5: Historian API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize a historian point/tag.
 *
 * Configures a new data collection point with compression
 * and exception reporting settings.
 */
void pax_hist_point_init(pax_hist_point_t *point, uint32_t id,
                          const char *tag, const char *description,
                          pax_hist_data_type_t data_type,
                          const char *unit, double range_min, double range_max);

/**
 * @brief Add a sample to the data stream.
 *
 * This is the primary data ingestion function. The sample is
 * evaluated against exception reporting criteria before storage.
 *
 * @param point The historian point
 * @param value New sample value
 * @param timestamp_ms Sample timestamp
 * @param quality Data quality indicator
 * @return 1 if sample was stored (passed exception filter), 0 if filtered out
 */
int pax_hist_add_sample(pax_hist_point_t *point,
                         double value, uint64_t timestamp_ms,
                         pax_io_quality_t quality);

/**
 * @brief Apply swinging door compression to a buffer of samples.
 *
 * Implements the Bristol swinging door algorithm:
 * For each point, maintains two "doors" (upper/lower slopes).
 * A point is retained only when a subsequent point falls outside
 * the parallelogram defined by the compression deviation.
 *
 * @param samples Input/output array of samples
 * @param num_samples Number of samples (updated to compressed count)
 * @param compression_deviation Maximum vertical deviation
 * @return Number of samples after compression
 */
uint32_t pax_hist_compress_swinging_door(pax_hist_sample_t *samples,
                                          uint32_t num_samples,
                                          double compression_deviation);

/**
 * @brief Apply deadband compression.
 *
 * Only stores a sample if the value change from the last
 * stored sample exceeds the deadband.
 *
 * @param samples Input/output array
 * @param num_samples Number of samples (updated)
 * @param deadband Minimum change to trigger storage
 * @return Number of stored samples
 */
uint32_t pax_hist_compress_deadband(pax_hist_sample_t *samples,
                                     uint32_t num_samples,
                                     double deadband);

/**
 * @brief Retrieve interpolated values at regular intervals.
 *
 * Linear interpolation between stored sample points.
 *
 * @param samples Array of stored samples
 * @param num_samples Number of stored samples
 * @param start_ms Start of query window
 * @param end_ms End of query window
 * @param interval_ms Interval for interpolated points
 * @param output Output buffer for interpolated values
 * @param max_output Max size of output buffer
 * @return Number of interpolated values generated
 */
uint32_t pax_hist_retrieve_interpolated(const pax_hist_sample_t *samples,
                                         uint32_t num_samples,
                                         uint64_t start_ms, uint64_t end_ms,
                                         uint32_t interval_ms,
                                         double *output, uint32_t max_output);

/**
 * @brief Compute summary statistics over a time window.
 *
 * Calculates average, min, max, stddev, total, delta, and
 * data quality percentage.
 */
int pax_hist_compute_summary(const pax_hist_sample_t *samples,
                              uint32_t num_samples,
                              uint64_t start_ms, uint64_t end_ms,
                              pax_hist_summary_t *summary);

/**
 * @brief Compute compression ratio.
 *
 * Compression ratio = (original sample count) / (stored sample count).
 * Higher is better (more compression).
 *
 * @param raw_count Number of raw samples before compression
 * @param compressed_count Number of samples after compression
 * @return Compression ratio
 */
double pax_hist_compression_ratio(uint32_t raw_count, uint32_t compressed_count);

/**
 * @brief Calculate time-weighted average of buffered samples.
 *
 * Each sample is weighted by the time interval it represents.
 * TWA = sum(v_i * dt_i) / total_dt
 */
double pax_hist_time_weighted_average(const pax_hist_sample_t *samples,
                                       uint32_t num_samples);

/**
 * @brief Detect step changes in time-series data.
 *
 * Uses the first derivative (finite difference) to detect
 * abrupt changes exceeding a configurable threshold.
 *
 * @param samples Array of samples
 * @param num_samples Number of samples
 * @param step_threshold Minimum change to qualify as step
 * @param step_indices Output buffer for indices where steps detected
 * @param max_steps Maximum number of steps to report
 * @return Number of step changes detected
 */
uint32_t pax_hist_detect_step_changes(const pax_hist_sample_t *samples,
                                       uint32_t num_samples,
                                       double step_threshold,
                                       uint32_t *step_indices,
                                       uint32_t max_steps);

/**
 * @brief Compute Exponential Weighted Moving Average (EWMA).
 *
 * EWMA = alpha * current_value + (1 - alpha) * previous_EWMA
 * This provides noise filtering with configurable responsiveness.
 */
double pax_hist_ewma(double current_value, double previous_ewma, double alpha);

/**
 * @brief Validate that a timestamp is within the archive range.
 */
bool pax_hist_validate_timestamp(uint64_t timestamp_ms,
                                  uint64_t archive_start_ms,
                                  uint64_t archive_end_ms);

#ifdef __cplusplus
}
#endif

#endif /* PLANTPAX_HISTORIAN_H */
