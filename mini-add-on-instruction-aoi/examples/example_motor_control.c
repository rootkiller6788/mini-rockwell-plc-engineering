/** @file example_motor_control.c
 *  @brief L6: Motor Start/Stop with E-Stop via Rockwell AOI
 *  Knowledge: L1-L6, Rockwell motor control pattern */
#include "plc_rockwell_aoi.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
static bool EMERGENCY = false, MOTOR_RUN = false;
static void mtr_logic(aoi_instance_t*i,const aoi_execution_context_t*c,void*u){
    (void)c;(void)u;aoi_atomic_value_t s,st,e,fb,run,fault;
    aoi_instance_get_param(i,"Start",&s);aoi_instance_get_param(i,"Stop",&st);
    aoi_instance_get_param(i,"EStop",&e);aoi_instance_get_param(i,"Fback",&fb);
    bool rs=s.bool_val,rt=st.bool_val,re=e.bool_val||EMERGENCY,rf=fb.bool_val;
    run.bool_val=rs&&!rt&&!re;if(run.bool_val&&!MOTOR_RUN){fault.bool_val=1;run.bool_val=0;}
    else fault.bool_val=0;aoi_instance_set_param(i,"Run",&run);aoi_instance_set_param(i,"Fault",&fault);}
int main(void){printf("=== Motor Start/Stop AOI ===\n");
    aoi_definition_t*d=aoi_definition_create("MtrCtrl","Motor VFD control");assert(d);
    aoi_definition_add_parameter(d,"Start",0,0,1);aoi_definition_add_parameter(d,"Stop",0,0,1);
    aoi_definition_add_parameter(d,"EStop",0,0,1);aoi_definition_add_parameter(d,"Fback",0,0,0);
    aoi_definition_add_parameter(d,"Run",1,0,0);aoi_definition_add_parameter(d,"Fault",1,0,0);
    aoi_definition_set_logic_callback(d,mtr_logic,0);
    aoi_instance_t*p=aoi_instance_create("Pump1",d);assert(p);
    aoi_execution_context_t ctx;memset(&ctx,0,sizeof(ctx));ctx.phase=AOI_PHASE_NORMAL_SCAN;ctx.scan_cycle_ms=10;
    aoi_atomic_value_t v;v.bool_val=1;p->enable_in=1;MOTOR_RUN=1;aoi_instance_set_param(p,"Fback",&v);
    aoi_instance_execute(p,&ctx);aoi_instance_get_param(p,"Run",&v);
    printf("Normal start: Run=%s Fault=%s\n",v.bool_val?"TRUE":"FALSE",v.bool_val?"FALSE":"TRUE");
    EMERGENCY=1;aoi_instance_execute(p,&ctx);aoi_instance_get_param(p,"Run",&v);
    printf("E-Stop: Run=%s\n",v.bool_val?"TRUE":"FALSE");
    EMERGENCY=0;aoi_instance_execute(p,&ctx);aoi_instance_get_param(p,"Run",&v);
    printf("Restart: Run=%s\n",v.bool_val?"TRUE":"FALSE");
    aoi_signature_t sig;aoi_compute_signature(d,&sig);
    printf("Signature: %02x%02x%02x%02x...\n",sig.digest[0],sig.digest[1],sig.digest[2],sig.digest[3]);
    printf("=== Complete ===\n");aoi_instance_free(p);aoi_definition_free(d);return 0;}
