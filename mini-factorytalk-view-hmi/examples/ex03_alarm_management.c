/**
 * @file ex03_alarm_management.c
 * @brief Example 3: Alarm Management & Trending (L6/L7)
 *
 * Scenario: Monitor multiple process tags with alarms, log data, query trends.
 * Demonstrates:
 *   - ISA-18.2 alarm lifecycle (raise, ack, return, shelve)
 *   - Alarm rate metrics (EEMUA 191)
 *   - Data logging with trigger types (periodic, on-change)
 *   - Trend query with resampling
 *   - OPC-HDA style time-range queries
 *
 * This models a FactoryTalk View SE alarm & trend server.
 *
 * Build: gcc -I../include ../src/ftview_common.c ../src/ftview_tag.c
 *            ../src/ftview_alarm.c ../src/ftview_datalog.c -o ex03_alarm_management
 *            ex03_alarm_management.c -lm
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../include/ftview_tag.h"
#include "../include/ftview_alarm.h"
#include "../include/ftview_datalog.h"

/* Simulate a floating-point sine wave for realistic process data */
static double process_simulation(double time_sec, double amplitude,
                                  double period, double offset)
{
    return offset + amplitude * sin(2.0 * M_PI * time_sec / period);
}

int main(void)
{
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  FactoryTalk View HMI — Alarm & Trend Example ║\n");
    printf("║  L6: Multi-tag alarm management with logging   ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");

    /* Initialise subsystems */
    ftview_tagdb_t tagdb;
    ftview_tagdb_init(&tagdb);

    ftview_alarm_mgr_t alarm_mgr;
    ftview_alarm_mgr_init(&alarm_mgr);

    ftview_datalog_mgr_t datalog_mgr;
    ftview_datalog_mgr_init(&datalog_mgr);

    /* 1. Create process tags */
    printf("--- Creating Process Tags ---\n");
    const char *tag_names[] = {
        "FIC101.PV",  /* Flow indicator */
        "PIC102.PV",  /* Pressure indicator */
        "LIC103.PV",  /* Level indicator */
        "TIC104.PV"   /* Temperature indicator */
    };
    for (int i = 0; i < 4; i++) {
        ftview_tag_t tag;
        memset(&tag, 0, sizeof(tag));
        strcpy(tag.name, tag_names[i]);
        tag.type = FTVIEW_TYPE_ANALOG;
        tag.mode = FTVIEW_TAG_MODE_MEMORY;
        tag.eu.range_lo = 0.0;
        tag.eu.range_hi = 100.0;
        ftview_tagdb_add(&tagdb, &tag);
        printf("  + %s\n", tag_names[i]);
    }

    /* 2. Configure alarms per ISA-18.2 */
    printf("\n--- Configuring Alarms ---\n");
    ftview_alarm_def_t alarms[] = {
        {.tag_name="FIC101.PV", .message="Flow rate HIGH",    .group="Utilities",
         .severity=FTVIEW_ALARM_SEV_HIGH,    .condition=FTVIEW_ALARM_COND_HI,
         .threshold=80.0, .deadband=2.0, .on_delay_ms=500, .enabled=true},
        {.tag_name="FIC101.PV", .message="Flow rate LOW",     .group="Utilities",
         .severity=FTVIEW_ALARM_SEV_MEDIUM,  .condition=FTVIEW_ALARM_COND_LO,
         .threshold=20.0, .deadband=2.0, .enabled=true},
        {.tag_name="PIC102.PV", .message="Pressure HIGH-HIGH", .group="Process",
         .severity=FTVIEW_ALARM_SEV_CRITICAL,.condition=FTVIEW_ALARM_COND_HIHI,
         .threshold=80.0, .threshold2=95.0, .deadband=1.0, .enabled=true},
        {.tag_name="LIC103.PV", .message="Level deviation",   .group="Process",
         .severity=FTVIEW_ALARM_SEV_HIGH,    .condition=FTVIEW_ALARM_COND_DEV,
         .threshold=25.0, .deadband=3.0, .enabled=true},
        {.tag_name="TIC104.PV", .message="Temperature LOW-LOW",.group="Reactor",
         .severity=FTVIEW_ALARM_SEV_CRITICAL,.condition=FTVIEW_ALARM_COND_LOLO,
         .threshold=10.0, .threshold2=5.0, .deadband=0.5, .enabled=true}
    };
    for (int i = 0; i < 5; i++) {
        ftview_alarm_add_def(&alarm_mgr, &alarms[i]);
        printf("  + Alarm: %s (%s)\n", alarms[i].message, alarms[i].tag_name);
    }

    /* 3. Configure data log models */
    printf("\n--- Configuring Data Logs ---\n");
    ftview_datalog_model_t fast_log, slow_log;
    memset(&fast_log, 0, sizeof(fast_log));
    strcpy(fast_log.name, "Fast_Trend");
    fast_log.trigger_type = FTVIEW_DATALOG_TRIG_PERIODIC;
    fast_log.interval_ms = 500;
    fast_log.tag_count = 4;
    fast_log.max_samples = 256;
    ftview_datalog_create_model(&datalog_mgr, &fast_log);
    printf("  + Fast log: 500ms periodic, 4 tags\n");

    memset(&slow_log, 0, sizeof(slow_log));
    strcpy(slow_log.name, "OnChange_Log");
    slow_log.trigger_type = FTVIEW_DATALOG_TRIG_ON_CHANGE;
    slow_log.deadband_pct = 1.0;
    slow_log.tag_count = 4;
    slow_log.max_samples = 128;
    ftview_datalog_create_model(&datalog_mgr, &slow_log);
    printf("  + Change log: 1%% deadband, 4 tags\n");

    /* 4. Simulate 60 seconds of process data */
    printf("\n--- Simulation: 60 seconds ---\n");
    double prev_values[4] = {0, 0, 0, 0};
    int alarm_events = 0;

    for (int t = 0; t < 60; t++) {
        /* Generate process values (sinusoidal with noise) */
        double values[4] = {
            process_simulation((double)t, 40.0, 25.0, 50.0),   /* Flow: oscillates 10-90 */
            process_simulation((double)t, 30.0, 15.0, 55.0),   /* Pressure: oscillates 25-85 */
            process_simulation((double)t, 20.0, 40.0, 45.0),   /* Level: slow drift 25-65 */
            process_simulation((double)t, 35.0, 30.0, 30.0) +  /* Temp: slow with trend */
             (double)t * 0.5                                    /* gradual ramp */
        };

        /* Update tags */
        for (int i = 0; i < 4; i++) {
            ftview_value_t val;
            memset(&val, 0, sizeof(val));
            val.type = FTVIEW_TYPE_ANALOG;
            val.data.analog = values[i];
            val.quality = FTVIEW_QUALITY_GOOD;
            val.timestamp_ms = t * 1000;

            ftview_tag_t *tag = ftview_tagdb_find(&tagdb, tag_names[i]);
            if (tag) ftview_tag_update_value(tag, &val);
        }

        /* Evaluate alarms */
        uint32_t changes = ftview_alarm_evaluate(&alarm_mgr, &tagdb);
        alarm_events += (int)changes;

        /* Log to fast trend */
        ftview_datalog_sample_t sample;
        memset(&sample, 0, sizeof(sample));
        sample.timestamp_ms = t * 1000;
        for (int i = 0; i < 4; i++) {
            sample.values[i] = values[i];
            sample.qualities[i] = FTVIEW_QUALITY_GOOD;
        }
        sample.tag_count = 4;
        ftview_datalog_sample(&datalog_mgr, 1, &sample);

        /* On-change logging */
        bool changed = false;
        for (int i = 0; i < 4; i++) {
            if (t == 0 || fabs(values[i] - prev_values[i]) > 1.0) {
                changed = true;
                break;
            }
        }
        if (changed) {
            ftview_datalog_sample(&datalog_mgr, 2, &sample);
        }
        memcpy(prev_values, values, sizeof(values));

        if (t % 10 == 0) {
            printf("  t=%2ds  Flow: %5.1f  Press: %5.1f  Level: %5.1f  Temp: %5.1f  Alarms: %d\n",
                   t, values[0], values[1], values[2], values[3], alarm_events);
        }

        /* Acknowledge alarms periodically */
        if (t % 15 == 5) {
            for (uint32_t i = 0; i < alarm_mgr.instance_count; i++) {
                if (alarm_mgr.instances[i].state == FTVIEW_ALARM_STATE_UNACK_ALM) {
                    ftview_alarm_acknowledge(&alarm_mgr,
                        alarm_mgr.instances[i].alarm_def_id,
                        "operator1", "Routine check");
                }
            }
        }
    }

    /* 5. Statistics */
    printf("\n--- Alarm Statistics ---\n");
    double alarm_rate = ftview_alarm_rate_per_hour(&alarm_mgr, 60000, 60000);
    printf("  Alarm events in 60s: %d\n", alarm_events);
    printf("  Alarm rate: %.1f alarms/hr\n", alarm_rate);
    printf("  EEMUA 191 target: < 150 alarms/hr per operator\n");

    /* Alarm log query */
    printf("\n--- Alarm Log (first 5 entries) ---\n");
    ftview_alarm_log_entry_t log_out[10];
    uint32_t log_n = ftview_alarm_log_query(&alarm_mgr, 0, 70000, log_out, 5);
    for (uint32_t i = 0; i < log_n && i < 5; i++) {
        printf("  [%llu] t=%lldms  %s: %s  state=%d  val=%.1f\n",
               (unsigned long long)log_out[i].sequence,
               (long long)log_out[i].timestamp_ms,
               log_out[i].tag_name, log_out[i].message,
               (int)log_out[i].state, log_out[i].value);
    }

    /* Trend data query */
    printf("\n--- Trend Query (Fast Log, 0-30s, 10-point resample) ---\n");
    double trend_values[10];
    ftview_quality_t trend_qualities[10];
    /* Set up a trend config referencing the fast log */
    ftview_trend_config_t trend_cfg;
    memset(&trend_cfg, 0, sizeof(trend_cfg));
    strcpy(trend_cfg.name, "Flow_Trend");
    trend_cfg.datalog_id = 1;
    trend_cfg.time_span_ms = 30000;
    trend_cfg.live_mode = true;
    trend_cfg.pen_count = 1;
    strcpy(trend_cfg.pens[0].tag_name, "FIC101.PV");
    trend_cfg.pens[0].pen_color = (ftview_color_t){120, 180, 255};
    trend_cfg.pens[0].y_min = 0;
    trend_cfg.pens[0].y_max = 100;
    trend_cfg.pens[0].auto_scale = true;
    ftview_datalog_create_trend(&datalog_mgr, &trend_cfg);

    /* Query trend data at 10-pixel resolution */
    uint32_t points = ftview_datalog_trend_query(&datalog_mgr, 1, 0, 0, 30000, 10,
                                                   trend_values, trend_qualities);
    printf("  Resampled to %u points:\n", points);
    for (uint32_t i = 0; i < points; i++) {
        printf("    [%u] %.1f (Q=%02X)\n", i, trend_values[i], trend_qualities[i]);
    }

    /* Statistics for temperature tag */
    printf("\n--- Temperature Statistics (0-60s) ---\n");
    double t_min, t_max, t_avg, t_std;
    ftview_datalog_statistics(&datalog_mgr, 1, 3, 0, 60000,
                               &t_min, &t_max, &t_avg, &t_std);
    printf("  TIC104.PV: min=%.1f  max=%.1f  avg=%.1f  stddev=%.2f\n",
           t_min, t_max, t_avg, t_std);

    /* Total samples in each log */
    printf("\n--- Data Log Summary ---\n");
    for (uint32_t i = 0; i < datalog_mgr.model_count; i++) {
        printf("  %s: %u samples stored, %u lost\n",
               datalog_mgr.models[i].name,
               datalog_mgr.stores[i].count,
               datalog_mgr.stores[i].loss_count);
    }

    printf("\n✓ Example 3 completed successfully.\n");
    return 0;
}
