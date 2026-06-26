/**
 * @file ex02_pid_faceplate.c
 * @brief Example 2: PID Controller Faceplate HMI (L6 / L7 — PlantPAx)
 *
 * Scenario: Reactor temperature PID controller faceplate.
 * Demonstrates:
 *   - PID faceplate with PV, SP, MV bar graph
 *   - Auto/Manual mode switching
 *   - Alarm on PV deviation
 *   - Data logging for trend
 *   - ISA-101 colour greyscale for normal, colour for alarms
 *
 * This models a PlantPAx P_AIn (Analog Input) faceplate.
 *
 * Build: gcc -I../include ../src/ftview_common.c ../src/ftview_tag.c
 *            ../src/ftview_alarm.c ../src/ftview_datalog.c
 *            ../src/ftview_graphics.c -o ex02_pid_faceplate ex02_pid_faceplate.c -lm
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../include/ftview_tag.h"
#include "../include/ftview_alarm.h"
#include "../include/ftview_datalog.h"
#include "../include/ftview_graphics.h"

/* PID controller (simulated) */
typedef struct {
    double  pv;       /* process value (temperature °C) */
    double  sp;       /* setpoint */
    double  mv;       /* manipulated variable (valve 0-100%) */
    double  kp;       /* gain */
    double  ti;       /* integral time (s) */
    double  td;       /* derivative time (s) */
    bool    auto_mode;
    double  integral;
    double  prev_error;
    double  output;
} pid_ctrl_t;

/* Simulate simple first-order plant: tank heating */
static double plant_update(double heat_input, double ambient, double *temperature,
                            double tau, double dt)
{
    /* First-order ODE: tau * dT/dt + T = K*u + ambient */
    double K = 1.0;  /* process gain */
    double dT_dt = (K * heat_input + ambient - *temperature) / tau;
    *temperature += dT_dt * dt;
    return *temperature;
}

void pid_execute(pid_ctrl_t *pid, double dt)
{
    double error = pid->sp - pid->pv;
    double p_term = pid->kp * error;

    /* Anti-windup: clamp integral */
    pid->integral += error * dt;
    double i_max = 100.0 / pid->kp;
    if (pid->integral > i_max) pid->integral = i_max;
    if (pid->integral < 0.0) pid->integral = 0.0;
    double i_term = (pid->kp / pid->ti) * pid->integral;

    /* Derivative with filtering */
    double d_term = pid->kp * pid->td * (error - pid->prev_error) / dt;
    pid->prev_error = error;

    pid->output = p_term + i_term + d_term;
    if (pid->output > 100.0) pid->output = 100.0;
    if (pid->output < 0.0) pid->output = 0.0;

    pid->mv = pid->output;
}

/* Render PID faceplate in text mode */
void render_pid_faceplate(const char *tag_base, double pv, double sp, double mv,
                           bool auto_mode, bool deviation_alarm)
{
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║   %s — PID Controller         ║\n", tag_base);
    printf("╠══════════════════════════════════════════╣\n");

    /* PV Bar: 40-char bar */
    int pv_bar = (int)(pv / 200.0 * 40.0);
    if (pv_bar < 0) pv_bar = 0; if (pv_bar > 40) pv_bar = 40;
    printf("║ PV: %6.1f °C  [", pv);
    for (int i = 0; i < 40; i++) {
        if (i < pv_bar) printf("█");
        else printf("·");
    }
    const char *color_code = deviation_alarm ? "!!!" : "   ";
    printf("] %s ║\n", color_code);

    /* SP marker */
    int sp_pos = (int)(sp / 200.0 * 40.0);
    printf("║ SP: %6.1f °C  ", sp);
    for (int i = 0; i < 40; i++) {
        if (i == sp_pos) printf("▲");
        else printf(" ");
    }
    printf("     ║\n");

    /* MV Bar */
    int mv_bar = (int)(mv / 100.0 * 40.0);
    printf("║ MV: %6.1f %%   [", mv);
    for (int i = 0; i < 40; i++) {
        if (i < mv_bar) printf("▓");
        else printf("░");
    }
    printf("]   ║\n");

    /* Mode */
    printf("║ Mode:  %s                          ║\n",
           auto_mode ? "AUTO" : "MANUAL");

    /* Limits */
    printf("║ Limits: PV 0-200°C | MV 0-100%%        ║\n");
    printf("╚══════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  FactoryTalk View HMI — PID Faceplate Example ║\n");
    printf("║  L6: Reactor Temperature Control              ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");

    /* Initialise */
    ftview_tagdb_t tagdb;
    ftview_tagdb_init(&tagdb);

    ftview_alarm_mgr_t alarm_mgr;
    ftview_alarm_mgr_init(&alarm_mgr);

    ftview_datalog_mgr_t datalog_mgr;
    ftview_datalog_mgr_init(&datalog_mgr);

    /* Create tags */
    printf("--- Configuring PID Tags ---\n");
    const char *names[] = {"TIC101.PV", "TIC101.SP", "TIC101.MV", "TIC101.MODE"};
    ftview_type_t types[] = {FTVIEW_TYPE_ANALOG, FTVIEW_TYPE_ANALOG,
                              FTVIEW_TYPE_ANALOG, FTVIEW_TYPE_DIGITAL};

    for (int i = 0; i < 4; i++) {
        ftview_tag_t tag;
        memset(&tag, 0, sizeof(tag));
        strcpy(tag.name, names[i]);
        tag.type = types[i];
        tag.mode = FTVIEW_TAG_MODE_MEMORY;
        tag.eu.range_lo = 0.0;
        tag.eu.range_hi = (i < 2) ? 200.0 : 100.0;
        ftview_tagdb_add(&tagdb, &tag);
        printf("  + %s\n", names[i]);
    }

    /* Configure alarm on high PV */
    ftview_alarm_def_t alarm_def;
    memset(&alarm_def, 0, sizeof(alarm_def));
    strcpy(alarm_def.tag_name, "TIC101.PV");
    strcpy(alarm_def.message, "Reactor temperature HIGH");
    strcpy(alarm_def.group, "Reactor");
    alarm_def.severity = FTVIEW_ALARM_SEV_HIGH;
    alarm_def.condition = FTVIEW_ALARM_COND_HI;
    alarm_def.threshold = 160.0;
    alarm_def.deadband = 2.0;
    alarm_def.enabled = true;
    ftview_alarm_add_def(&alarm_mgr, &alarm_def);

    /* Deviation alarm */
    memset(&alarm_def, 0, sizeof(alarm_def));
    strcpy(alarm_def.tag_name, "TIC101.PV");  /* same tag, different alarm */
    strcpy(alarm_def.message, "Reactor temperature deviation");
    alarm_def.severity = FTVIEW_ALARM_SEV_MEDIUM;
    alarm_def.condition = FTVIEW_ALARM_COND_DEV;
    alarm_def.threshold = 15.0;  /* 15°C deviation */
    alarm_def.deadband = 2.0;
    alarm_def.enabled = true;
    ftview_alarm_add_def(&alarm_mgr, &alarm_def);

    /* Configure data log for trending */
    ftview_datalog_model_t log_model;
    memset(&log_model, 0, sizeof(log_model));
    strcpy(log_model.name, "TIC101_Trend");
    log_model.trigger_type = FTVIEW_DATALOG_TRIG_PERIODIC;
    log_model.interval_ms = 1000;
    log_model.tag_count = 3;
    log_model.tag_ids[0] = 1; /* TIC101.PV */
    log_model.tag_ids[1] = 2; /* TIC101.SP */
    log_model.tag_ids[2] = 3; /* TIC101.MV */
    ftview_datalog_create_model(&datalog_mgr, &log_model);

    /* Set up PID controller */
    pid_ctrl_t pid;
    memset(&pid, 0, sizeof(pid));
    pid.sp = 120.0;   /* target 120°C */
    pid.kp = 2.0;
    pid.ti = 60.0;    /* 1 minute integral */
    pid.td = 5.0;     /* derivative */
    pid.auto_mode = true;

    double temperature = 25.0;  /* ambient start */
    double ambient = 25.0;

    /* Simulation */
    printf("\n--- Simulation: PID Control Loop ---\n");
    for (int step = 0; step < 20; step++) {
        double dt = 1.0;  /* 1 second per step */

        /* Update PID */
        pid.pv = temperature;
        pid_execute(&pid, dt);

        /* Update plant */
        plant_update(pid.mv, ambient, &temperature, 50.0, dt);

        /* Update tags */
        ftview_value_t val;
        memset(&val, 0, sizeof(val));
        val.type = FTVIEW_TYPE_ANALOG;
        val.quality = FTVIEW_QUALITY_GOOD;

        ftview_tag_t *tag;
        tag = ftview_tagdb_find(&tagdb, "TIC101.PV");
        if (tag) { val.data.analog = pid.pv; ftview_tag_update_value(tag, &val); }
        tag = ftview_tagdb_find(&tagdb, "TIC101.SP");
        if (tag) { val.data.analog = pid.sp; ftview_tag_update_value(tag, &val); }
        tag = ftview_tagdb_find(&tagdb, "TIC101.MV");
        if (tag) { val.data.analog = pid.mv; ftview_tag_update_value(tag, &val); }

        /* Update digital mode tag */
        val.type = FTVIEW_TYPE_DIGITAL;
        val.data.digital = pid.auto_mode;
        tag = ftview_tagdb_find(&tagdb, "TIC101.MODE");
        if (tag) ftview_tag_update_value(tag, &val);

        /* Log data */
        ftview_datalog_sample_t sample;
        memset(&sample, 0, sizeof(sample));
        sample.timestamp_ms = step * 1000;
        sample.values[0] = pid.pv;
        sample.values[1] = pid.sp;
        sample.values[2] = pid.mv;
        sample.tag_count = 3;
        sample.qualities[0] = sample.qualities[1] = sample.qualities[2] = FTVIEW_QUALITY_GOOD;
        ftview_datalog_sample(&datalog_mgr, 1, &sample);

        /* Evaluate alarms */
        ftview_alarm_evaluate(&alarm_mgr, &tagdb);

        printf("[Step %2d] PV=%6.1f°C  SP=%6.1f°C  MV=%5.1f%%  Mode=%s  %s\n",
               step, pid.pv, pid.sp, pid.mv,
               pid.auto_mode ? "AUTO" : "MAN",
               (temperature > 160.0) ? "⚠ HI ALARM" : "");

        if (step == 10) {
            /* Change setpoint */
            pid.sp = 150.0;
            printf("  → Setpoint changed to 150.0°C\n");
        }

        if (step == 5 || step == 15) {
            render_pid_faceplate("TIC101", pid.pv, pid.sp, pid.mv,
                                  pid.auto_mode,
                                  fabs(pid.sp - pid.pv) > 15.0);
        }
    }

    /* Statistics from data log */
    printf("\n--- Trend Statistics (TIC101.PV) ---\n");
    double min_v, max_v, avg, stddev;
    ftview_datalog_statistics(&datalog_mgr, 1, 0, 0, 20000,
                               &min_v, &max_v, &avg, &stddev);
    printf("  Min: %.1f  Max: %.1f  Avg: %.1f  StdDev: %.2f\n",
           min_v, max_v, avg, stddev);

    /* Alarm summary */
    printf("\n--- Alarm Summary ---\n");
    for (uint32_t i = 0; i < alarm_mgr.log_count; i++) {
        uint32_t idx = (alarm_mgr.log_write_idx - 1 - i + FTVIEW_ALARM_LOG_SIZE)
                        % FTVIEW_ALARM_LOG_SIZE;
        if (alarm_mgr.log[idx].sequence == 0) continue;
        printf("  [%llu] %s: %s (state=%d, val=%.1f)\n",
               (unsigned long long)alarm_mgr.log[idx].sequence,
               alarm_mgr.log[idx].tag_name,
               alarm_mgr.log[idx].message,
               (int)alarm_mgr.log[idx].state,
               alarm_mgr.log[idx].value);
    }

    printf("\n✓ Example 2 completed successfully.\n");
    return 0;
}
