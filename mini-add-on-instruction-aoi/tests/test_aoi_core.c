#include "plc_rockwell_aoi.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
static int tr=0,tp=0;
void tlog(const char*n,int ok){tr++;if(ok){printf("  PASS: %s\n",n);tp++;}else printf("  FAIL: %s\n",n);}
static void tcb(aoi_instance_t*i,const aoi_execution_context_t*c,void*u){
    (void)c;(void)u;aoi_atomic_value_t v;
    if(aoi_instance_get_param(i,"InVal",&v)){v.dint_val+=1;aoi_instance_set_param(i,"OutVal",&v);}}
int main(void){printf("=== AOI Core Tests ===\n");
    aoi_definition_t*d=aoi_definition_create("MtrCtrl","Motor control");
    tlog("Create AOI",d&&!strcmp(d->name,"MtrCtrl")&&d->parameter_count==0);
    tlog("NULL name",!aoi_definition_create(0,0));
    tlog("Reserved name",!aoi_definition_create("TON",0));
    tlog("Add params",AOI_VALID_OK==aoi_definition_add_parameter(d,"Start",0,0,1)&&
         AOI_VALID_OK==aoi_definition_add_parameter(d,"InVal",0,3,0)&&
         AOI_VALID_OK==aoi_definition_add_parameter(d,"OutVal",1,3,0)&&d->parameter_count==3);
    tlog("Dup param",AOI_VALID_ERR_NAME_DUP==aoi_definition_add_parameter(d,"Start",0,0,1));
    tlog("Add local tag",AOI_VALID_OK==aoi_definition_add_local_tag(d,"Tmr",3,0)&&d->local_tag_count==1);
    tlog("Validate def",AOI_VALID_OK==aoi_definition_validate(d));
    aoi_instance_t*i=aoi_instance_create("M1",d);
    tlog("Create instance",i!=0&&strcmp(i->instance_name,"M1")==0&&i->scan_counter==0);
    aoi_atomic_value_t v;v.bool_val=1;
    tlog("Set/get param",AOI_VALID_OK==aoi_instance_set_param(i,"Start",&v));
    aoi_atomic_value_t rv;
    tlog("Get param",aoi_instance_get_param(i,"Start",&rv)&&rv.bool_val);
    int32_t ix=aoi_definition_get_param_index(d,"Start");
    tlog("Index access",ix>=0&&aoi_instance_get_param_by_index(i,(uint16_t)ix).bool_val);
    aoi_atomic_value_t lv;lv.dint_val=42;
    tlog("Local tag set",AOI_VALID_OK==aoi_instance_set_local_tag(i,"Tmr",&lv));
    aoi_atomic_value_t lr;
    tlog("Local tag get",aoi_instance_get_local_tag(i,"Tmr",&lr)&&lr.dint_val==42);
    aoi_definition_set_logic_callback(d,tcb,0);
    aoi_execution_context_t ctx;memset(&ctx,0,sizeof(ctx));ctx.phase=AOI_PHASE_NORMAL_SCAN;ctx.scan_cycle_ms=10;
    i->enable_in=1;aoi_atomic_value_t inv;inv.dint_val=100;aoi_instance_set_param(i,"InVal",&inv);
    tlog("Execute",aoi_instance_execute(i,&ctx)&&i->scan_counter==1);
    aoi_atomic_value_t ov;
    tlog("Output check",aoi_instance_get_param(i,"OutVal",&ov)&&ov.dint_val==101);
    i->enable_in=0;
    tlog("EnableIn=false",aoi_instance_execute(i,&ctx)&&!i->enable_out);
    aoi_signature_t s1,s2;aoi_compute_signature(d,&s1);aoi_compute_signature(d,&s2);
    tlog("Signature",aoi_signature_equal(&s1,&s2));
    aoi_library_t*l=aoi_library_create("Proj","1756-L85E",36,11);
    aoi_definition_t*d2=aoi_definition_create("Vlv","Valve");
    tlog("Library",l&&aoi_library_count(l)==0&&AOI_VALID_OK==aoi_library_add_aoi(l,d2)&&aoi_library_count(l)==1);
    aoi_library_free(l);aoi_definition_free(d2);
    aoi_instance_reset(i);
    tlog("Reset",aoi_instance_get_param(i,"OutVal",&ov)&&ov.dint_val==0);
    aoi_context_param_t cp;memset(&cp,0,sizeof(cp));cp.context=AOI_CTX_SAFETY_TASK;cp.param_index=1;cp.override_required=1;cp.new_required=1;
    tlog("CSP add",AOI_VALID_OK==aoi_definition_add_context_override(d,&cp));
    bool rq,vs;aoi_definition_resolve_context(d,1,AOI_CTX_SAFETY_TASK,&rq,&vs);
    tlog("CSP safety",rq);
    aoi_definition_resolve_context(d,1,AOI_CTX_STANDARD_TASK,&rq,&vs);
    tlog("CSP standard",!rq);
    aoi_safety_cert_t sc;memset(&sc,0,sizeof(sc));sc.sil_level=AOI_SIL_2;
    aoi_definition_t*sd=aoi_definition_create("Safe","Safety");aoi_definition_add_parameter(sd,"Cmd",0,0,0);
    aoi_definition_set_safety_cert(sd,&sc);
    tlog("Safety cert",sd->is_certified&&sd->is_source_protected&&sd->parameters[0].required);
    aoi_definition_free(sd);aoi_instance_free(i);aoi_definition_free(d);
    printf("\n=== %d/%d tests passed ===\n",tp,tr);
    return(tp==tr)?0:1;}
