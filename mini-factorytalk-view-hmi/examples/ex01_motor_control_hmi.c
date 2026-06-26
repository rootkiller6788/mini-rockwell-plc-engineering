/**
 * @file ex01_motor_control_hmi.c
 * @brief Example 1: Motor Start/Stop Control HMI (L6 — Canonical Problem)
 *
 * Scenario: Operator controls a pump motor via an HMI display.
 * Demonstrates:
 *   - Tag database for I/O points (start PB, stop PB, run status, speed)
 *   - Alarm on motor overload
 *   - Security login before start permitted
 *   - Display screen with motor control objects
 *
 * This models a typical FactoryTalk View SE motor control pop-up.
 *
 * Build: gcc -I../include ../src/ftview_common.c ../src/ftview_tag.c
 *            ../src/ftview_alarm.c ../src/ftview_security.c
 *            ../src/ftview_graphics.c -o ex01_motor_control_hmi ex01_motor_control_hmi.c -lm
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/ftview_tag.h"
#include "../include/ftview_alarm.h"
#include "../include/ftview_security.h"
#include "../include/ftview_graphics.h"

/* Simulated plant device state */
typedef struct {
    bool    running;
    double  speed_rpm;
    double  current_amps;
    double  temperature_c;
    bool    overload_trip;
    bool    start_pb;
    bool    stop_pb;
} pump_motor_t;

/* Simulate one scan cycle of the PLC logic */
static void plc_scan(pump_motor_t *motor)
{
    if (motor->start_pb && !motor->running && !motor->overload_trip) {
        motor->running = true;
        motor->speed_rpm = 1750.0;
        motor->current_amps = 15.0;
    }
    if (motor->stop_pb && motor->running) {
        motor->running = false;
        motor->speed_rpm = 0.0;
        motor->current_amps = 0.0;
    }
    if (motor->running) {
        motor->temperature_c += 1.2;  /* heat up */
        if (motor->temperature_c > 130.0) {
            motor->overload_trip = true;
            motor->running = false;
            motor->speed_rpm = 0.0;
            printf("  *** MOTOR OVERLOAD TRIP ***\n");
        }
    } else {
        motor->temperature_c *= 0.98;  /* cool down */
        if (motor->temperature_c < 25.0) motor->temperature_c = 25.0;
        if (motor->overload_trip && motor->temperature_c < 40.0) {
            motor->overload_trip = false;  /* auto-reset when cooled */
        }
    }
}

/* Update HMI tags from plant state */
static void update_hmi_tags(ftview_tagdb_t *db, const pump_motor_t *motor)
{
    ftview_value_t val;
    memset(&val, 0, sizeof(val));

    /* Motor run status (digital) */
    val.type = FTVIEW_TYPE_DIGITAL;
    val.data.digital = motor->running;
    val.quality = FTVIEW_QUALITY_GOOD;
    ftview_tag_t *tag = ftview_tagdb_find(db, "MTR1001.RUN");
    if (tag) ftview_tag_update_value(tag, &val);

    /* Motor speed (analog) */
    val.type = FTVIEW_TYPE_ANALOG;
    val.data.analog = motor->speed_rpm;
    tag = ftview_tagdb_find(db, "MTR1001.SPEED");
    if (tag) ftview_tag_update_value(tag, &val);

    /* Motor current */
    val.data.analog = motor->current_amps;
    tag = ftview_tagdb_find(db, "MTR1001.AMPS");
    if (tag) ftview_tag_update_value(tag, &val);

    /* Motor temperature */
    val.data.analog = motor->temperature_c;
    tag = ftview_tagdb_find(db, "MTR1001.TEMP");
    if (tag) ftview_tag_update_value(tag, &val);

    /* Start PB */
    val.type = FTVIEW_TYPE_DIGITAL;
    val.data.digital = motor->start_pb;
    tag = ftview_tagdb_find(db, "MTR1001.START_PB");
    if (tag) ftview_tag_update_value(tag, &val);

    /* Stop PB */
    val.data.digital = motor->stop_pb;
    tag = ftview_tagdb_find(db, "MTR1001.STOP_PB");
    if (tag) ftview_tag_update_value(tag, &val);
}

/* Print HMI display (simplified text-based) */
static void render_hmi(const ftview_tagdb_t *db, const ftview_alarm_mgr_t *alarm_mgr,
                       const ftview_security_mgr_t *sec_mgr, uint32_t session_id)
{
    ftview_value_t val;
    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║     PUMP MTR-1001 CONTROL PANEL          ║\n");
    printf("╠══════════════════════════════════════════╣\n");

    /* Status */
    if (ftview_tagdb_resolve(db, "MTR1001.RUN", &val) == FTVIEW_OK) {
        printf("║  Status:    %s", val.data.digital ? "► RUNNING" : "■ STOPPED");
        /* Add spacing */
        int pad = val.data.digital ? 27 : 28;
        for (int i = 0; i < pad; i++) printf(" ");
        printf("║\n");
    }

    if (ftview_tagdb_resolve(db, "MTR1001.SPEED", &val) == FTVIEW_OK) {
        printf("║  Speed:     %6.0f RPM                        ║\n", val.data.analog);
    }

    if (ftview_tagdb_resolve(db, "MTR1001.AMPS", &val) == FTVIEW_OK) {
        printf("║  Current:   %6.1f A                          ║\n", val.data.analog);
    }

    if (ftview_tagdb_resolve(db, "MTR1001.TEMP", &val) == FTVIEW_OK) {
        printf("║  Temp:      %6.1f °C                         ║\n", val.data.analog);
    }

    printf("╠══════════════════════════════════════════╣\n");

    /* User info */
    if (session_id > 0) {
        printf("║  User:      %-30s ║\n", sec_mgr->sessions[session_id - 1].username);
    } else {
        printf("║  User:      NOT LOGGED IN                  ║\n");
    }

    /* Alarm summary */
    printf("╠══════════════════════════════════════════╣\n");
    printf("║  Alarms:                                  ║\n");
    bool has_active = false;
    for (uint32_t i = 0; i < alarm_mgr->instance_count; i++) {
        if (alarm_mgr->instances[i].state == FTVIEW_ALARM_STATE_UNACK_ALM ||
            alarm_mgr->instances[i].state == FTVIEW_ALARM_STATE_ACK_ALM) {
            has_active = true;
            printf("║    ⚠ %-35s ║\n",
                   alarm_mgr->defs[alarm_mgr->instances[i].alarm_def_id - 1].message);
        }
    }
    if (!has_active) {
        printf("║    No active alarms                        ║\n");
    }
    printf("╚══════════════════════════════════════════╝\n");
}

int main(void)
{
    printf("╔════════════════════════════════════════════════╗\n");
    printf("║  FactoryTalk View HMI — Motor Control Example ║\n");
    printf("║  L6: Canonical Motor Start/Stop HMI           ║\n");
    printf("╚════════════════════════════════════════════════╝\n\n");

    /* 1. Initialise subsystems */
    ftview_tagdb_t tagdb;
    ftview_tagdb_init(&tagdb);

    ftview_alarm_mgr_t alarm_mgr;
    ftview_alarm_mgr_init(&alarm_mgr);

    ftview_security_mgr_t sec_mgr;
    ftview_security_mgr_init(&sec_mgr);

    ftview_display_mgr_t disp_mgr;
    ftview_display_mgr_init(&disp_mgr);

    /* 2. Create HMI tags */
    printf("--- Configuring HMI Tags ---\n");
    const char *tag_names[] = {
        "MTR1001.RUN", "MTR1001.SPEED", "MTR1001.AMPS",
        "MTR1001.TEMP", "MTR1001.START_PB", "MTR1001.STOP_PB"
    };
    ftview_type_t tag_types[] = {
        FTVIEW_TYPE_DIGITAL, FTVIEW_TYPE_ANALOG, FTVIEW_TYPE_ANALOG,
        FTVIEW_TYPE_ANALOG, FTVIEW_TYPE_DIGITAL, FTVIEW_TYPE_DIGITAL
    };
    double tag_ranges[][4] = {
        {0,1,0,1}, {0,1800,0,1800}, {0,30,0,30},
        {0,200,0,200}, {0,1,0,1}, {0,1,0,1}
    };

    for (int i = 0; i < 6; i++) {
        ftview_tag_t tag;
        memset(&tag, 0, sizeof(tag));
        strcpy(tag.name, tag_names[i]);
        tag.type = tag_types[i];
        tag.mode = FTVIEW_TAG_MODE_MEMORY;
        tag.eu.range_lo = tag_ranges[i][0];
        tag.eu.range_hi = tag_ranges[i][1];
        tag.eu.clamp_lo = tag_ranges[i][2];
        tag.eu.clamp_hi = tag_ranges[i][3];
        ftview_tagdb_add(&tagdb, &tag);
        printf("  + Tag: %s (%s)\n", tag_names[i],
               tag_types[i] == FTVIEW_TYPE_ANALOG ? "analog" : "digital");
    }

    /* 3. Configure alarm */
    printf("\n--- Configuring Alarm ---\n");
    ftview_alarm_def_t alarm_def;
    memset(&alarm_def, 0, sizeof(alarm_def));
    strcpy(alarm_def.tag_name, "MTR1001.TEMP");
    strcpy(alarm_def.message, "Motor winding over-temperature");
    strcpy(alarm_def.group, "Pump House");
    alarm_def.severity = FTVIEW_ALARM_SEV_HIGH;
    alarm_def.condition = FTVIEW_ALARM_COND_HI;
    alarm_def.threshold = 120.0;
    alarm_def.deadband = 2.0;
    alarm_def.on_delay_ms = 2000;
    alarm_def.enabled = true;
    ftview_alarm_add_def(&alarm_mgr, &alarm_def);
    printf("  + Alarm: HI on MTR1001.TEMP at 120°C (deadband 2°C)\n");

    /* 4. Configure security */
    printf("\n--- Configuring Security ---\n");
    ftview_role_t op_role, eng_role;
    memset(&op_role, 0, sizeof(op_role));
    strcpy(op_role.name, "Operator");
    op_role.privilege_mask = FTVIEW_PRIV_VIEW | FTVIEW_PRIV_ACK_ALARMS;
    ftview_security_add_role(&sec_mgr, &op_role);

    memset(&eng_role, 0, sizeof(eng_role));
    strcpy(eng_role.name, "Engineer");
    eng_role.privilege_mask = FTVIEW_PRIV_VIEW | FTVIEW_PRIV_ACK_ALARMS |
                               FTVIEW_PRIV_START_STOP | FTVIEW_PRIV_CHANGE_SP;
    ftview_security_add_role(&sec_mgr, &eng_role);

    ftview_security_add_user(&sec_mgr, "operator1", "op123", 1);
    ftview_security_add_user(&sec_mgr, "engineer1", "eng456", 2);
    printf("  + Roles: Operator (view+ack), Engineer (full control)\n");
    printf("  + Users: operator1, engineer1\n");

    /* 5. Login as engineer */
    printf("\n--- Login ---\n");
    uint32_t session_id = ftview_security_login(&sec_mgr, "engineer1", "eng456", "HMI-STN01");
    if (session_id > 0) {
        printf("  ✓ Logged in as 'engineer1' (session %u)\n", session_id);
    }

    /* 6. Create display screen */
    ftview_display_screen_t screen;
    memset(&screen, 0, sizeof(screen));
    strcpy(screen.name, "MotorControl");
    screen.isa_level = ISA101_L3_EQUIPMENT_MODULE;
    screen.archetype = ISA101_DISPLAY_MOTOR_CONTROL;
    screen.width = 1024;
    screen.height = 768;
    screen.background_color = ISA101_COLOR_BG_DARK;
    ftview_display_add_screen(&disp_mgr, &screen);

    /* 7. Simulation loop */
    printf("\n--- Simulation: 10 Scan Cycles ---\n");
    pump_motor_t motor;
    memset(&motor, 0, sizeof(motor));
    motor.temperature_c = 25.0;

    for (int scan = 0; scan < 10; scan++) {
        /* Operator actions (simulated) */
        if (scan == 1) motor.start_pb = true;   /* press start at cycle 1 */
        else motor.start_pb = false;

        if (scan == 8) motor.stop_pb = true;    /* press stop at cycle 8 */
        else motor.stop_pb = false;

        plc_scan(&motor);
        update_hmi_tags(&tagdb, &motor);
        ftview_alarm_evaluate(&alarm_mgr, &tagdb);

        printf("\n[Scan %d] Motor %s | Temp: %.1f°C | Speed: %.0f RPM\n",
               scan, motor.running ? "RUNNING" : "stopped",
               motor.temperature_c, motor.speed_rpm);

        if (scan == 3 || scan == 7) {
            render_hmi(&tagdb, &alarm_mgr, &sec_mgr, session_id);
        }
    }

    /* Check if any alarms triggered */
    printf("\n--- Alarm Summary ---\n");
    uint32_t alarm_count = 0;
    for (uint32_t i = 0; i < alarm_mgr.instance_count; i++) {
        if (alarm_mgr.instances[i].state != FTVIEW_ALARM_STATE_NORMAL) {
            printf("  ⚠ Alarm: %s (state=%d)\n",
                   alarm_mgr.defs[alarm_mgr.instances[i].alarm_def_id - 1].message,
                   (int)alarm_mgr.instances[i].state);
            alarm_count++;
        }
    }
    printf("  Total active alarms: %u\n", alarm_count);

    /* Acknowledge alarms if operator is logged in */
    if (alarm_count > 0 && session_id > 0) {
        printf("\n--- Acknowledging Alarms ---\n");
        for (uint32_t i = 0; i < alarm_mgr.instance_count; i++) {
            if (alarm_mgr.instances[i].state == FTVIEW_ALARM_STATE_UNACK_ALM) {
                ftview_alarm_acknowledge(&alarm_mgr,
                    alarm_mgr.instances[i].alarm_def_id,
                    "engineer1", "Investigated and acknowledged");
                printf("  ✓ Acknowledged alarm %u\n",
                       alarm_mgr.instances[i].alarm_def_id);
            }
        }
    }

    /* Logout */
    ftview_security_logout(&sec_mgr, session_id, "HMI-STN01");
    printf("\n--- Session ended ---\n");

    /* Check audit trail */
    printf("\n--- Audit Trail (last 5 entries) ---\n");
    ftview_audit_entry_t audit_out[10];
    uint32_t audit_count = ftview_security_audit_query(&sec_mgr, 0, 1000000000,
                                                         audit_out, 10);
    uint32_t start = audit_count > 5 ? audit_count - 5 : 0;
    for (uint32_t i = start; i < audit_count; i++) {
        printf("  [%s] %s: %s\n",
               audit_out[i].success ? "PASS" : "FAIL",
               audit_out[i].username,
               audit_out[i].detail);
    }

    printf("\n✓ Example 1 completed successfully.\n");
    return 0;
}
