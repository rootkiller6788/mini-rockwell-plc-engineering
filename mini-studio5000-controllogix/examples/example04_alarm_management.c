/**
 * example04_alarm_management.c
 *
 * ControlLogix ALMA/ALMD — Alarm Management per ISA-18.2
 *
 * Demonstrates:
 *   - ALMA (Analog Alarm): tank level HH/H/LO/LL with deadband
 *   - ALMD (Digital Alarm): motor fault detection
 *   - Alarm state machine: NORMAL -> ACTIVE_UNACK -> ACTIVE_ACK -> NORMAL
 *   - Alarm on-delay to prevent nuisance alarms
 *   - Severity and class assignment
 */

#include <stdio.h>
#include "../include/logix_alarm_model.h"

int main(void)
{
    printf("=== Example 04: ALMA/ALMD Alarm Management ===\n\n");

    /* ------------------------------------------------
     * Part 1: Analog Alarm (ALMA)
     *
     * Tank 101 level alarm:
     *   HH = 9.5m  H = 8.5m  L = 2.0m  LL = 1.0m
     *   Deadband = 0.3m
     *   On-delay = 1.0 sec (avoid nuisance)
     * ------------------------------------------------ */
    logix_alma_t alma;
    logix_alma_init(&alma, "Tank101", 0.1);  /* Ts = 0.1s */
    logix_alma_set_limits(&alma,
                          9.5,   /* HH limit */
                          8.5,   /* H limit */
                          2.0,   /* L limit */
                          1.0,   /* LL limit */
                          0.3,   /* Deadband */
                          0.0, 100.0);  /* EU range */
    logix_alma_set_delays(&alma, 1.0, 0.0, 1.0, 0.0);  /* HH:1s, H:0s, L:1s, LL:0s */
    logix_alma_set_severity(&alma, ALARM_SEVERITY_CRITICAL, ALARM_CLASS_PROCESS);

    printf("ALMA: Tank101 Level Alarm\n");
    printf("  Limits: HH=9.5 H=8.5 L=2.0 LL=1.0 DB=0.3\n");
    printf("  On-delay: HH=1.0s L=1.0s\n\n");

    /* Scenario: level rises from normal to HH */
    double scenario[] = {5.0, 5.0, 5.0, 8.6, 8.6, 8.6,
                         9.0, 9.0, 9.6, 9.6, 9.6, 9.6,
                         9.0, 9.0, 5.0, 5.0};
    int nsteps = sizeof(scenario) / sizeof(scenario[0]);

    printf("  Step  Level(m)  HH    H     L     LL    State\n");
    printf("  ----  --------  ----  ----  ----  ----  -----\n");

    for (int i = 0; i < nsteps; i++) {
        logix_alma_execute(&alma, scenario[i]);
        logix_alma_state_t st = logix_alma_get_state(&alma);
        const char *state_names[] = {"NORMAL","ACTIVE_UNACK","ACTIVE_ACK",
                                     "NORMAL_UNACK","SHELVED","SUPPRESSED"};

        printf("  %4d  %8.1f  %-4s  %-4s  %-4s  %-4s  %s\n",
               i, scenario[i],
               alma.hh_active ? "ON" : "off",
               alma.h_active  ? "ON" : "off",
               alma.l_active  ? "ON" : "off",
               alma.ll_active ? "ON" : "off",
               state_names[st]);

        /* Acknowledge after alarm becomes active */
        if (alma.hh_active && !alma.hh_acked) {
            logix_alma_acknowledge(&alma);
            printf("       >> HH acknowledged\n");
        }
        if (alma.l_active && !alma.l_acked) {
            logix_alma_acknowledge(&alma);
            printf("       >> L acknowledged\n");
        }
    }

    /* ------------------------------------------------
     * Part 2: Digital Alarm (ALMD)
     *
     * Motor Fault detection with on-delay
     * ------------------------------------------------ */
    printf("\n--- Digital Alarm (ALMD) ---\n");
    logix_almd_t almd;
    logix_almd_init(&almd, "MotorX_Fault", 0.1, true);  /* Normally closed */
    logix_almd_set_severity(&almd, ALARM_SEVERITY_HIGH, ALARM_CLASS_DEVICE);
    logix_almd_set_on_delay(&almd, 0.5);  /* 500ms on-delay */

    printf("ALMD: MotorX Fault (NC, on-delay=0.5s)\n\n");

    bool fault_scenario[] = {true, true, true, true, false, false,
                             true, true, true, true, true, false};
    int ns2 = sizeof(fault_scenario) / sizeof(fault_scenario[0]);

    printf("  Step  Input  Active  Acknowledged\n");
    printf("  ----  -----  ------  ------------\n");
    for (int i = 0; i < ns2; i++) {
        logix_almd_execute(&almd, fault_scenario[i]);

        if (almd.active && !almd.acked) {
            logix_almd_acknowledge(&almd);
            printf("  %4d  %-5s  YES    YES (ack)\n",
                   i, fault_scenario[i] ? "TRUE" : "FALSE");
        } else {
            printf("  %4d  %-5s  %-5s  %s\n",
                   i, fault_scenario[i] ? "TRUE" : "FALSE",
                   almd.active ? "YES" : "no",
                   almd.acked ? "YES" : "no");
        }
    }

    printf("\n=== Alarm management demo complete ===\n");
    return 0;
}
