/** @file example_pid_control.c
 *  @brief L6: PID Temperature Control via AOI (Rockwell PIDE pattern)
 *  Knowledge: L1-L6, PID with auto/manual, anti-windup, output clamping */
#include "plc_rockwell_aoi.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
typedef struct{double kp,ki,kd,sp,pv,integral,prev_err,out,omin,omax;int auto_mode;}ps;
static void pid_cb(aoi_instance_t*i,const aoi_execution_context_t*c,void*u){
    (void)c;ps*s=(ps*)u;aoi_atomic_value_t v;
    aoi_instance_get_param(i,"SP",&v);s->sp=v.real_val;
    aoi_instance_get_param(i,"PV",&v);s->pv=v.real_val;
    aoi_instance_get_param(i,"Kp",&v);s->kp=v.real_val;
    aoi_instance_get_param(i,"Ki",&v);s->ki=v.real_val;
    aoi_instance_get_param(i,"Kd",&v);s->kd=v.real_val;
    aoi_instance_get_param(i,"Auto",&v);s->auto_mode=v.bool_val;
    if(!s->auto_mode){aoi_instance_get_param(i,"ManOut",&v);s->out=v.real_val;s->integral=0;}
    else{double err=s->sp-s->pv;double dt=c->scan_cycle_ms/1000.0;
        s->integral+=s->ki*err*dt;if(s->integral>s->omax)s->integral=s->omax;if(s->integral<s->omin)s->integral=s->omin;
        s->out=s->kp*err+s->integral+s->kd*(s->prev_err-err)/dt;s->prev_err=err;
        if(s->out>s->omax)s->out=s->omax;if(s->out<s->omin)s->out=s->omin;}
    v.real_val=(float)s->out;aoi_instance_set_param(i,"CV",&v);}
int main(void){printf("=== PID Temperature Control AOI ===\n");
    aoi_definition_t*d=aoi_definition_create("TempPID","Temperature PID loop");assert(d);
    aoi_definition_add_parameter(d,"SP",0,5,1);aoi_definition_add_parameter(d,"PV",0,5,1);
    aoi_definition_add_parameter(d,"Kp",0,5,1);aoi_definition_add_parameter(d,"Ki",0,5,1);
    aoi_definition_add_parameter(d,"Kd",0,5,1);aoi_definition_add_parameter(d,"Auto",0,0,1);
    aoi_definition_add_parameter(d,"ManOut",0,5,0);aoi_definition_add_parameter(d,"CV",1,5,0);
    ps st;memset(&st,0,sizeof(st));st.omin=0;st.omax=100;st.auto_mode=1;
    aoi_definition_set_logic_callback(d,pid_cb,&st);
    aoi_instance_t*l=aoi_instance_create("TIC101",d);assert(l);
    aoi_execution_context_t ctx;memset(&ctx,0,sizeof(ctx));ctx.phase=AOI_PHASE_NORMAL_SCAN;ctx.scan_cycle_ms=100;
    aoi_atomic_value_t v;l->enable_in=1;
    v.real_val=150.0f;aoi_instance_set_param(l,"SP",&v);v.real_val=2.5f;aoi_instance_set_param(l,"Kp",&v);
    v.real_val=0.1f;aoi_instance_set_param(l,"Ki",&v);v.real_val=0.5f;aoi_instance_set_param(l,"Kd",&v);
    v.bool_val=1;aoi_instance_set_param(l,"Auto",&v);
    double pv=25.0;printf("Cycle  SP      PV       CV\n");
    for(int c=0;c<30;c++){v.real_val=(float)pv;aoi_instance_set_param(l,"PV",&v);
        aoi_instance_execute(l,&ctx);aoi_instance_get_param(l,"CV",&v);
        double cv=v.real_val;pv+=cv*0.08-(pv-25.0)*0.02;
        if(c%5==0||c<3)printf("%-6d %-7.1f %-8.1f %-8.1f\n",c,150.0,pv,cv);}
    printf("Final PV: %.1f C (SP: 150.0 C)\n",pv);
    printf("=== Complete ===\n");aoi_instance_free(l);aoi_definition_free(d);return 0;}
