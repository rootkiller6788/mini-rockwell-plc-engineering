#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../include/control_logix_platform.h"
#include "../include/studio5000_project.h"
#include "../include/logix_tag_model.h"
#include "../include/logix_pide_instruction.h"
#include "../include/logix_instruction_set.h"
#include "../include/logix_alarm_model.h"
#include "../include/logix_motion_control.h"
#include "../include/logix_redundancy.h"
#include "../include/logix_iec61131_compliance.h"
#include "../include/logix_execution_model.h"

int main(void)
{
    /* Test 1: Chassis initialization */
    logix_chassis_t chassis;
    assert(logix_chassis_init(&chassis, CHASSIS_10_SLOT, CHASSIS_TYPE_STANDARD));
    assert(chassis.size == 10);

    /* Test 2: Module installation and power budget */
    assert(logix_chassis_install_module(&chassis, 1, 0x1001, 0.5, 0.1, 0.0));
    assert(logix_chassis_verify_power_budget(&chassis));
    assert(logix_chassis_occupied_slot_count(&chassis) == 1);

    /* Test 3: Controller type naming */
    const char *name = logix_controller_type_name(CTLR_1756_L85E);
    assert(strstr(name, "L85E") != NULL);

    /* Test 4: Tag creation and validation */
    logix_tag_t tag;
    assert(logix_tag_create_controller_scope(&tag, "Tank101_Level", LOGIX_TYPE_REAL));
    assert(logix_tag_validate_name("Motor_Run_Cmd"));
    assert(!logix_tag_validate_name("123Invalid"));
    assert(!logix_tag_validate_name(""));

    /* Test 5: Tag write/read */
    float write_val = 75.5f;
    assert(logix_tag_write(&tag, &write_val));
    float read_val = 0.0f;
    assert(logix_tag_read(&tag, &read_val) == 4);
    assert(read_val == 75.5f);

    /* Test 6: Type sizes */
    assert(logix_type_size(LOGIX_TYPE_BOOL) == 1);
    assert(logix_type_size(LOGIX_TYPE_DINT) == 4);
    assert(logix_type_size(LOGIX_TYPE_REAL) == 4);
    assert(logix_type_size(LOGIX_TYPE_LREAL) == 8);

    /* Test 7: PIDE initialization and execution */
    logix_pide_t pide;
    logix_pide_init(&pide, 0.1);
    assert(pide.kp == 1.0);
    assert(pide.mode == PIDE_MODE_MANUAL);

    pide.pv = 50.0;
    pide.sp = 70.0;
    logix_pide_set_mode(&pide, PIDE_MODE_AUTO);
    double cv = logix_pide_execute(&pide);
    /* Should output positive CV since SP > PV */
    assert(cv >= 0.0);

    /* Test 8: Timer execution */
    logix_timer_t timer;
    logix_timer_init(&timer, TIMER_TYPE_TON, 1000);
    /* First call: enable, no time elapsed yet */
    assert(!logix_timer_execute(&timer, true, 0));
    /* After 500ms */
    assert(!logix_timer_execute(&timer, true, 500));
    /* After 1500ms (ACC >= PRE) */
    assert(logix_timer_execute(&timer, true, 1500));

    /* Test 9: Counter execution */
    logix_counter_t ctr;
    logix_counter_init(&ctr, 3);
    logix_counter_execute(&ctr, true, false, false);
    logix_counter_execute(&ctr, false, false, false);
    logix_counter_execute(&ctr, true, false, false);
    logix_counter_execute(&ctr, false, false, false);
    logix_counter_execute(&ctr, true, false, false);
    logix_counter_execute(&ctr, false, false, false);
    assert(ctr.acc == 3);
    assert(ctr.dn);

    /* Test 10: Scale instruction */
    logix_scl_t scl;
    logix_scl_init(&scl, 0.0, 100.0, 4.0, 20.0, true);
    double scaled = logix_scl_execute(&scl, 50.0);
    assert(scaled >= 11.9 && scaled <= 12.1); /* 50% should give 12mA */

    /* Test 11: LDLG filter */
    logix_ldlg_t filter;
    logix_ldlg_init(&filter, 1.0, 0.0, 1.0, 1, 0.1);
    double filtered = logix_ldlg_execute(&filter, 100.0);
    assert(filtered >= 0.0);

    /* Test 12: ALMA alarm */
    logix_alma_t alarm;
    logix_alma_init(&alarm, "Tank101", 0.1);
    logix_alma_set_limits(&alarm, 90.0, 80.0, 20.0, 10.0, 0.5, 0.0, 100.0);
    logix_alma_execute(&alarm, 95.0);
    /* HH should be active after on-delay (default 0) */
    assert(alarm.hh_active);

    /* Test 13: ALMD digital alarm */
    logix_almd_t dalm;
    logix_almd_init(&dalm, "MotorFault", 0.1, true);
    logix_almd_execute(&dalm, true);
    assert(dalm.active);

    /* Test 14: Motion axis */
    motion_axis_t axis;
    logix_motion_axis_init(&axis, "ServoX", AXIS_TYPE_ROTARY, 1);
    assert(axis.axis_number == 1);
    assert(axis.state == AXIS_STATE_DISABLED);

    /* Test 15: Redundancy availability */
    double avail = logix_redundancy_availability(100000.0, 8.0);
    assert(avail > 0.999);

    /* Test 16: IEC reserved keyword */
    assert(logix_iec_is_reserved_keyword("PROGRAM"));
    assert(logix_iec_is_reserved_keyword("TON"));
    assert(!logix_iec_is_reserved_keyword("MyTag"));

    /* Test 17: IEC compliance */
    assert(logix_iec_get_compliance(LOGIX_TYPE_BOOL) == IEC_COMPLIANCE_FULL);
    assert(logix_iec_get_compliance(LOGIX_TYPE_LREAL) == IEC_COMPLIANCE_EXTENDED);

    /* Test 18: Controller mode transition */
    assert(logix_controller_mode_transition(CONTROLLER_MODE_PROGRAM,
                                             CONTROLLER_MODE_RUN, false));
    assert(!logix_controller_mode_transition(CONTROLLER_MODE_PROGRAM,
                                              CONTROLLER_MODE_RUN, true));

    printf("All tests passed!\n");
    return 0;
}
