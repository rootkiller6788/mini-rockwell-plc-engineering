/**
 * @file example_analog_scaling.c
 * @brief End-to-end example: Analog signal processing and scaling.
 *
 * Demonstrates:
 *   1. Analog input scaling (4-20mA → engineering units)
 *   2. Signal filtering (IIR low-pass)
 *   3. Loop integrity checking
 *   4. Process alarm evaluation
 *   5. Analog output with ramping
 *
 * This example simulates a temperature control loop:
 *   - AI Channel 0: Tank temperature (4-20mA, 0-150 degC)
 *   - AO Channel 0: Steam valve position (4-20mA, 0-100% open)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pointio_types.h"
#include "pointio_module.h"
#include "pointio_analog.h"

int main(void)
{
    printf("==============================================\n");
    printf("  Point I/O Analog Scaling Demo\n");
    printf("  Tank Temperature Control\n");
    printf("==============================================\n\n");

    /* --- Step 1: Configure analog input module (1734-IE2C) --- */
    pointio_module_t ai_mod;
    pointio_analog_scaling_t ai_scaling;
    memset(&ai_scaling, 0, sizeof(ai_scaling));
    ai_scaling.eu_low = 0.0;
    ai_scaling.eu_high = 150.0;
    ai_scaling.raw_low = 3277.0;    /* 4mA in counts */
    ai_scaling.raw_high = 16383.0;  /* 20mA in counts */
    ai_scaling.range = POINTIO_AI_RANGE_4_20MA;
    ai_scaling.format = POINTIO_DATA_FORMAT_EU;
    strncpy(ai_scaling.eu_label, "degC", sizeof(ai_scaling.eu_label) - 1);

    pointio_module_config_analog_input(&ai_mod, "1734-IE2C", 2,
        POINTIO_AI_RANGE_4_20MA, &ai_scaling, "Tank_Temp_AI");
    ai_mod.slot.slot = 1;
    printf("[1] Configured %s: 2ch AI at slot %d\n", ai_mod.identity.catalog_number, ai_mod.slot.slot);

    /* --- Step 2: Configure AI channel with alarms --- */
    pointio_ai_config_t ch_config;
    memset(&ch_config, 0, sizeof(ch_config));
    ch_config.channel = 0;
    ch_config.range = POINTIO_AI_RANGE_4_20MA;
    ch_config.data_format = POINTIO_DATA_FORMAT_EU;
    ch_config.scaling = ai_scaling;
    ch_config.enable_filtering = 1;
    ch_config.filter_time_constant_ms = 500.0;  /* 0.5s time constant */
    ch_config.enable_alarm = 1;
    ch_config.alarm_high_high = 140.0;  /* HH at 140 degC */
    ch_config.alarm_high = 120.0;       /* H at 120 degC */
    ch_config.alarm_low = 10.0;         /* L at 10 degC */
    ch_config.alarm_low_low = 5.0;      /* LL at 5 degC */
    ch_config.alarm_deadband = 2.0;     /* 2 degC hysteresis */
    strncpy(ch_config.alias, "Tank_Temp", sizeof(ch_config.alias) - 1);

    printf("[2] Configured channel 0: %s\n", ch_config.alias);
    printf("     Range: 4-20mA → 0-150 degC\n");
    printf("     Alarms: HH=140, H=120, L=10, LL=5 (deadband=2 degC)\n\n");

    /* --- Step 3: Simulate temperature readings --- */
    double simulated_temps_ma[] = {
        4.0,   /* 0 degC - cold start */
        8.0,   /* 37.5 degC */
        12.0,  /* 75 degC */
        16.0,  /* 112.5 degC */
        19.0,  /* 140.6 degC - HH alarm */
        20.0,  /* 150 degC - max range */
    };
    int num_samples = sizeof(simulated_temps_ma) / sizeof(simulated_temps_ma[0]);

    printf("[3] Processing temperature samples:\n");
    printf("  %-12s %-12s %-12s %-12s %-10s\n",
        "Loop mA", "Raw Counts", "Temp (degC)", "Filtered", "Alarm");

    double prev_filtered = 0.0;
    int prev_alarm = 0;
    int alarm_events = 0;

    for (int i = 0; i < num_samples; i++) {
        double ma = simulated_temps_ma[i];

        /* Convert mA to raw counts */
        uint16_t raw_count = (uint16_t)((ma / 20.0) * 16383.0);

        /* Scale to engineering units */
        double eu_value;
        pointio_ai_raw_to_eu(raw_count, &ai_scaling, &eu_value);

        /* Apply filtering */
        double filtered;
        pointio_ai_filter(eu_value, &prev_filtered,
            500.0, 100.0, &filtered);  /* tau=500ms, Ts=100ms */

        /* Check alarms */
        int alarm;
        pointio_ai_evaluate_alarms(filtered, &ch_config, &prev_alarm, &alarm);

        const char *alarm_str;
        switch (alarm) {
        case 0: alarm_str = "NORMAL"; break;
        case 1: alarm_str = "HIGH"; break;
        case 2: alarm_str = "HH!"; alarm_events++; break;
        case 3: alarm_str = "LOW"; break;
        case 4: alarm_str = "LL!"; alarm_events++; break;
        default: alarm_str = "?"; break;
        }

        printf("  %6.1f mA     %6u        %7.1f        %7.1f      %-10s\n",
            ma, raw_count, eu_value, filtered, alarm_str);
    }

    printf("  Total alarm events: %d\n\n", alarm_events);

    /* --- Step 4: Check loop integrity --- */
    printf("[4] Loop integrity check:\n");

    /* Simulate various loop conditions */
    uint16_t test_readings[] = {100, 5000, 10000, 16383, 17000};

    for (int i = 0; i < 5; i++) {
        int status;
        double loop_ma;
        pointio_ai_check_loop_integrity(test_readings[i], &ai_scaling,
            &status, &loop_ma);

        const char *status_str;
        int is_open;
        pointio_ai_detect_open_wire(test_readings[i], &ai_scaling, &is_open);

        switch (status) {
        case 0: status_str = "OK"; break;
        case 1: status_str = "OPEN WIRE"; break;
        case 2: status_str = "SHORT"; break;
        case 3: status_str = "Under-range"; break;
        case 4: status_str = "Over-range"; break;
        default: status_str = "Unknown"; break;
        }
        printf("  Counts:%6u → %.1f mA [%s]%s\n",
            test_readings[i], loop_ma, status_str,
            is_open ? " OPEN_WIRE_DETECTED" : "");
    }
    printf("\n");

    /* --- Step 5: Analog output with ramping --- */
    printf("[5] Analog output ramping demo:\n");

    pointio_module_t ao_mod;
    pointio_module_config_analog_output(&ao_mod, "1734-OE2C", 1,
        POINTIO_AO_RANGE_4_20MA, "Steam_Valve_AO");
    ao_mod.slot.slot = 2;

    pointio_ao_config_t ao_config;
    memset(&ao_config, 0, sizeof(ao_config));
    ao_config.channel = 0;
    ao_config.range = POINTIO_AO_RANGE_4_20MA;
    ao_config.scaling.eu_low = 0.0;
    ao_config.scaling.eu_high = 100.0;
    ao_config.scaling.raw_low = 3277.0;
    ao_config.scaling.raw_high = 16383.0;
    ao_config.scaling.range = POINTIO_AI_RANGE_4_20MA;
    ao_config.clamp_high_eu = 100.0;
    ao_config.clamp_low_eu = 0.0;
    ao_config.ramp_rate_eu_s = 10.0;  /* 10% per second max */
    strncpy(ao_config.alias, "Steam_Valve", sizeof(ao_config.alias) - 1);

    printf("  Valve target: 70%% open\n");
    printf("  Ramp rate: 10%%/s\n");

    /* Ramp from 0% to 70% in 1-second steps */
    double current = 0.0;
    for (int step = 1; step <= 10; step++) {
        int rc = pointio_ao_ramp_to(&ao_mod, 0, &ao_config, 70.0, 10.0, 1.0, &current);
        printf("  Step %d: Valve = %5.1f%% %s\n",
            step, current, rc == 1 ? "[AT TARGET]" : "(ramping...)");
        if (rc == 1) break;
    }
    printf("\n");

    printf("=== Demo Complete ===\n");
    return 0;
}
