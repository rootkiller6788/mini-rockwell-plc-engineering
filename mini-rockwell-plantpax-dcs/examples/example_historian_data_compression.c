/**
 * @example example_historian_data_compression.c
 * @brief Historian Data Collection and Compression ? L5/L7 Application
 *
 * Demonstrates FactoryTalk Historian data collection:
 *   - Real-time data sampling
 *   - Exception reporting (store-on-change)
 *   - Swinging door compression
 *   - Deadband compression
 *   - Summary statistics computation
 *   - EWMA smoothing
 *
 * This models a typical PlantPAx historian collecting process data
 * from a reactor temperature sensor with noise and trend patterns.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "plantpax_historian.h"

#define NUM_RAW_SAMPLES     200
#define SCAN_PERIOD_MS      500

/* Generate noisy process data with a trend */
static void generate_process_data(pax_hist_sample_t *samples, uint32_t count)
{
    uint32_t i;
    double base_temp = 150.0;
    double noise_std = 0.5;

    for (i = 0; i < count; i++) {
        samples[i].timestamp_ms = (uint64_t)i * SCAN_PERIOD_MS;

        /* Process model: gradual heat-up, then stabilize, then cool */
        if (i < 60) {
            /* Ramp up from 25 to 150 over first 30 seconds */
            double frac = (double)i / 60.0;
            samples[i].value = 25.0 + frac * 125.0;
        } else if (i < 140) {
            /* Stable at 150 with small oscillation */
            samples[i].value = base_temp + 2.0 * sin((i - 60) * 0.1);
        } else if (i < 180) {
            /* Disturbance: spike to 160 */
            double frac = (double)(i - 140) / 40.0;
            samples[i].value = base_temp + 10.0 * sin(frac * 3.14159);
        } else {
            /* Cool down */
            double frac = (double)(i - 180) / 20.0;
            samples[i].value = base_temp - frac * 20.0;
        }

        /* Add noise */
        samples[i].value += ((double)((i * 1103515245 + 12345) & 0x7fffffff)
                             / 0x7fffffff - 0.5) * 2.0 * noise_std;
        samples[i].quality = PAX_IO_QUALITY_GOOD;
        samples[i].is_compressed = false;
    }

    /* Inject a few bad-quality samples */
    samples[80].quality = PAX_IO_QUALITY_BAD;
    samples[81].quality = PAX_IO_QUALITY_BAD;
}

int main(void)
{
    printf("==================================================\n");
    printf("  PlantPAx DCS ? Historian Data Compression Demo\n");
    printf("==================================================\n\n");

    /* --- Step 1: Generate raw process data --- */
    pax_hist_sample_t raw_samples[NUM_RAW_SAMPLES];
    generate_process_data(raw_samples, NUM_RAW_SAMPLES);
    printf("[1] Generated %u raw samples\n", NUM_RAW_SAMPLES);

    /* --- Step 2: Exception Reporting --- */
    pax_hist_point_t point;
    pax_hist_point_init(&point, 1, "TIC101.PV", "Reactor Temperature",
                          PAX_HIST_TYPE_FLOAT64, "degC", 0.0, 200.0);
    point.exception_deviation = 0.2;
    point.scan_period_ms = SCAN_PERIOD_MS;

    uint32_t stored_after_exception = 0;
    uint32_t i;
    for (i = 0; i < NUM_RAW_SAMPLES; i++) {
        int s = pax_hist_add_sample(&point, raw_samples[i].value,
                                     raw_samples[i].timestamp_ms,
                                     raw_samples[i].quality);
        stored_after_exception += (uint32_t)(s > 0 ? 1 : 0);
    }
    printf("[2] Exception reporting: %u/%u samples stored (%.1f%%)\n",
           stored_after_exception, NUM_RAW_SAMPLES,
           100.0 * stored_after_exception / NUM_RAW_SAMPLES);

    /* --- Step 3: Swinging Door Compression --- */
    pax_hist_sample_t compressed[NUM_RAW_SAMPLES];
    memcpy(compressed, raw_samples, sizeof(raw_samples));
    uint32_t after_swinging_door = pax_hist_compress_swinging_door(
        compressed, NUM_RAW_SAMPLES, 0.5);
    double sd_ratio = pax_hist_compression_ratio(NUM_RAW_SAMPLES,
                                                  after_swinging_door);
    printf("[3] Swinging door compression: %u samples (%.1f:1 ratio)\n",
           after_swinging_door, sd_ratio);

    /* --- Step 4: Deadband Compression --- */
    pax_hist_sample_t db_compressed[NUM_RAW_SAMPLES];
    memcpy(db_compressed, raw_samples, sizeof(raw_samples));
    uint32_t after_deadband = pax_hist_compress_deadband(
        db_compressed, NUM_RAW_SAMPLES, 0.5);
    double db_ratio = pax_hist_compression_ratio(NUM_RAW_SAMPLES,
                                                  after_deadband);
    printf("[4] Deadband compression:    %u samples (%.1f:1 ratio)\n",
           after_deadband, db_ratio);

    /* --- Step 5: Summary Statistics --- */
    pax_hist_summary_t summary;
    pax_hist_compute_summary(raw_samples, NUM_RAW_SAMPLES,
                              0, (NUM_RAW_SAMPLES - 1) * SCAN_PERIOD_MS,
                              &summary);
    printf("\n[5] Summary Statistics:\n");
    printf("    Count:     %u\n", summary.count);
    printf("    Average:   %.2f degC\n", summary.average);
    printf("    Min:       %.2f degC\n", summary.minimum);
    printf("    Max:       %.2f degC\n", summary.maximum);
    printf("    StdDev:    %.2f degC\n", summary.stddev);
    printf("    Delta:     %.2f degC\n", summary.delta);
    printf("    %% Good:    %.1f%%\n", summary.percent_good);

    /* --- Step 6: EWMA Smoothing Demo --- */
    printf("\n[6] EWMA Smoothing (alpha sweep):\n");
    printf("    %-8s %-12s\n", "Alpha", "Last EWMA");
    double alphas[] = {0.1, 0.3, 0.5, 0.9};
    uint32_t j;
    for (j = 0; j < 4; j++) {
        double ewma = raw_samples[0].value;
        for (i = 1; i < NUM_RAW_SAMPLES; i++) {
            ewma = pax_hist_ewma(raw_samples[i].value, ewma, alphas[j]);
        }
        printf("    %-8.1f %-12.2f\n", alphas[j], ewma);
    }

    /* --- Step 7: Step Change Detection --- */
    uint32_t step_indices[10];
    uint32_t num_steps = pax_hist_detect_step_changes(
        raw_samples, NUM_RAW_SAMPLES, 5.0, step_indices, 10);
    printf("\n[7] Step change detection: %u steps found\n", num_steps);
    for (i = 0; i < num_steps; i++) {
        printf("    Step at sample %u: %.2f degC\n",
               step_indices[i], raw_samples[step_indices[i]].value);
    }

    /* --- Step 8: Time-Weighted Average --- */
    double twa = pax_hist_time_weighted_average(raw_samples, NUM_RAW_SAMPLES);
    printf("\n[8] Time-weighted average: %.2f degC\n", twa);

    return 0;
}
