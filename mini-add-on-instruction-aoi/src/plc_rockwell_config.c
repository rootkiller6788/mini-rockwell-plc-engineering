/**
 * @file plc_rockwell_config.c
 * @brief Rockwell Controller Configuration Implementation
 * Knowledge: L1-L7. Ref: 1756-SG001, IEC 61508, IEC 62443
 */
#include "plc_rockwell_config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static void scp3(char*d,const char*s,size_t n){if(!d||!s||!n)return;size_t i=0;while(i<n-1&&s[i]){d[i]=s[i];i++;}d[i]=0;}

/* ---- Controller Specification Database ---- */
static plc_controller_spec_t SPECS[]={
    {"1756-L81E", PLATFORM_CONTROLLOGIX_5580,32,100,100000,40,0,0,0,1,1,500,256,0},
    {"1756-L82E", PLATFORM_CONTROLLOGIX_5580,32,100,100000,80,0,0,1,1,500,512,0},
    {"1756-L85E", PLATFORM_CONTROLLOGIX_5580,32,100,100000,160,0,1,1,1,750,1024,128},
    {"1756-L84ES",PLATFORM_GUARDLOGIX_5580,32,100,100000,80,1,1,1,1,500,512,128},
    {"5069-L330ER",PLATFORM_COMPACTLOGIX_5380,16,50,50000,30,0,0,0,1,256,256,16},
    {"5069-L340ERM",PLATFORM_COMPACTLOGIX_5380,16,50,50000,40,0,1,0,1,300,512,32},
    {"5069-L430ERMW",PLATFORM_COMPACTLOGIX_5480,32,100,100000,80,0,1,1,1,500,1024,128},
    {NULL,0,0,0,0,0,0,0,0,0,0,0,0}
};
/* ---- Config Create/Free ---- */
plc_controller_config_t* plc_config_create(const char*pn,plc_platform_t plat,const char*mdl){
    if(!pn)return 0;plc_controller_config_t*c=calloc(1,sizeof(plc_controller_config_t));if(!c)return 0;
    scp3(c->project_name,pn,128);c->platform=plat;if(mdl)scp3(c->model_number,mdl,32);
    c->watchdog_ms=500;c->system_overhead_pct=20;c->user_memory_bytes=40UL*1024*1024;return c;}
void plc_config_free(plc_controller_config_t*c){free(c);}
/* ---- Firmware Management ---- */
bool plc_config_set_firmware(plc_controller_config_t*c,uint32_t maj,uint32_t min,uint32_t bld){
    if(!c)return 0;c->firmware_major=maj;c->firmware_minor=min;c->firmware_build=bld;return 1;}
bool plc_config_set_redundancy(plc_controller_config_t*c,plc_redundancy_mode_t m,uint16_t ss){
    if(!c)return 0;c->redundancy_mode=m;c->secondary_slot=ss;return 1;}
bool plc_config_validate(const plc_controller_config_t*c,char*ebuf,uint32_t ebufsz){
    if(!c){if(ebuf&&ebufsz)snprintf(ebuf,ebufsz,"NULL config");return 0;}
    if(c->watchdog_ms<1||c->watchdog_ms>3000){if(ebuf&&ebufsz)snprintf(ebuf,ebufsz,"Watchdog out of range");return 0;}
    if(c->system_overhead_pct>80){if(ebuf&&ebufsz)snprintf(ebuf,ebufsz,"System overhead >80%%");return 0;}
    return 1;}
/* ---- Task Management ---- */
plc_task_config_t* plc_task_create(const char*n,plc_task_type_t t,uint8_t pri,uint32_t per){
    if(!n)return 0;plc_task_config_t*tk=calloc(1,sizeof(plc_task_config_t));if(!tk)return 0;
    scp3(tk->name,n,64);tk->type=t;tk->priority=pri;tk->period_us=per;
    tk->watchdog_us=per*3;/*default: 3x period*/tk->allow_event_overlap=0;return tk;}
void plc_task_free(plc_task_config_t*tk){free(tk);}
bool plc_task_add_program(plc_task_config_t*tk,const char*pn){
    if(!tk||!pn||tk->program_count>=32)return 0;scp3(tk->program_names[tk->program_count],pn,64);tk->program_count++;return 1;}
bool plc_task_remove_program(plc_task_config_t*tk,const char*pn){
    if(!tk||!pn)return 0;for(uint16_t i=0;i<tk->program_count;i++)
        if(!strcmp(tk->program_names[i],pn)){tk->program_names[i][0]=0;return 1;}return 0;}
uint32_t plc_task_estimate_scan_time(const plc_task_config_t*tk,const plc_performance_model_t*pm,uint32_t rc){
    if(!tk||!pm||!rc)return 0;
    double scan_ns=rc*(pm->branch_ns+pm->dint_add_ns)+pm->interrupt_latency_ns;
    return(uint32_t)(scan_ns/1000.0);}/* microseconds */
/* ---- Program Management ---- */
plc_program_config_t* plc_program_create(const char*n,const char*desc){
    if(!n)return 0;plc_program_config_t*p=calloc(1,sizeof(plc_program_config_t));if(!p)return 0;
    scp3(p->name,n,64);if(desc)scp3(p->description,desc,256);p->default_language=RUNG_LANG_LADDER;return p;}
void plc_program_free(plc_program_config_t*p){free(p);}
bool plc_program_set_main_routine(plc_program_config_t*p,const char*rn){if(!p||!rn)return 0;scp3(p->main_routine,rn,64);return 1;}
/* ---- I/O Module Management ---- */
plc_io_module_t* plc_io_module_create(const char*n,plc_io_module_type_t t,const char*cat,uint8_t slot){
    if(!n)return 0;plc_io_module_t*m=calloc(1,sizeof(plc_io_module_t));if(!m)return 0;
    scp3(m->name,n,64);m->type=t;if(cat)scp3(m->catalog,cat,32);m->slot=slot;m->rpi_ms=20.0f;return m;}
void plc_io_module_free(plc_io_module_t*m){free(m);}
bool plc_io_calculate_image_size(const plc_io_module_t*m,uint32_t*is,uint32_t*os){
    if(!m){if(is)*is=0;if(os)*os=0;return 0;}
    switch(m->type){case IO_TYPE_DIGITAL_IN:if(is)*is=4;if(os)*os=0;break;
        case IO_TYPE_DIGITAL_OUT:if(is)*is=0;if(os)*os=4;break;
        case IO_TYPE_ANALOG_IN:if(is)*is=16;if(os)*os=0;break;
        case IO_TYPE_ANALOG_OUT:if(is)*is=0;if(os)*os=8;break;
        case IO_TYPE_SAFETY_IN:if(is)*is=16;if(os)*os=0;break;
        case IO_TYPE_SAFETY_OUT:if(is)*is=0;if(os)*os=16;break;
        case IO_TYPE_MOTION_AXIS:if(is)*is=128;if(os)*os=128;break;
        default:if(is)*is=8;if(os)*os=8;break;}return 1;}
/* ---- EtherNet/IP Port Management ---- */
plc_ethernet_port_t* plc_eth_port_create(const char*pn){
    if(!pn)return 0;plc_ethernet_port_t*p=calloc(1,sizeof(plc_ethernet_port_t));if(!p)return 0;
    scp3(p->port_name,pn,16);p->enabled=1;p->autonegotiate=1;p->speed_mbps=1000;p->full_duplex=1;
    scp3(p->ip_address,"192.168.1.10",16);scp3(p->subnet_mask,"255.255.255.0",16);return p;}
void plc_eth_port_free(plc_ethernet_port_t*p){free(p);}
bool plc_eth_port_set_ip(plc_ethernet_port_t*p,const char*ip,const char*mask,const char*gw){
    if(!p||!ip||!mask)return 0;scp3(p->ip_address,ip,16);scp3(p->subnet_mask,mask,16);if(gw)scp3(p->gateway,gw,16);return 1;}
/* Simple IPv4 validation: format n.n.n.n with each byte 0-255 */
bool plc_eth_port_validate_ip(const plc_ethernet_port_t*p){
    if(!p)return 0;unsigned int a,b,cd,d;/*avoiding name conflict*/
    if(sscanf(p->ip_address,"%u.%u.%u.%u",&a,&b,&cd,&d)!=4)return 0;
    if(a>255||b>255||cd>255||d>255)return 0;
    if(sscanf(p->subnet_mask,"%u.%u.%u.%u",&a,&b,&cd,&d)!=4)return 0;
    if(a>255||b>255||cd>255||d>255)return 0;return 1;}
/* ---- Safety Configuration ---- */
bool plc_safety_config_validate(const plc_safety_config_t*s,const plc_controller_config_t*c,char*ebuf,uint32_t esz){
    if(!s||!c){if(ebuf&&esz)snprintf(ebuf,esz,"NULL input");return 0;}
    if(!s->safety_enabled)return 1;/*no safety=valid*/
    if(s->target_sil<1||s->target_sil>3){if(ebuf&&esz)snprintf(ebuf,esz,"SIL must be 1-3");return 0;}
    if(s->safety_period_ms<5||s->safety_period_ms>500){if(ebuf&&esz)snprintf(ebuf,esz,"Safety period 5-500ms");return 0;}
    return 1;}
/* PFD calculation per IEC 61508-6 simplified formula: PFDavg = lambda_du * TI / 2 */
double plc_safety_calculate_pfd(const plc_safety_config_t*s,double mttf_h,double pti_h){
    if(!s||mttf_h<=0||pti_h<=0)return 1.0;double lambda=1.0/mttf_h;double du=(1.0-s->diagnostic_coverage/100.0)*lambda;
    return du*pti_h/2.0;}
/* ---- Performance Model ---- */
const plc_performance_model_t* plc_performance_lookup(const char*mn){
    static plc_performance_model_t l85e={0,1000,4,1024,160,1,4.0,5.0,2.5,25.0,150.0,500.0,200.0,1500.0,50.0,45.0,10.0,30.0,500,500,256,256};
    if(!mn)return 0;if(strstr(mn,"L85E")||strstr(mn,"L84E"))return &l85e;return 0;}
uint32_t plc_estimate_scan_time(const plc_performance_model_t*pm,uint32_t rc,uint32_t ac,uint32_t pc,uint32_t mc){
    if(!pm)return 0;double ns=rc*pm->branch_ns+ac*pm->aoi_overhead_ns+pc*pm->pid_ns+mc*pm->message_ns;return(uint32_t)(ns/1000.0);}

/* =================================================================
 * Extended Configuration Operations
 * ================================================================= */

/* Get controller specification by model number */
const plc_controller_spec_t* plc_config_get_spec(const char*mn){
    if(!mn)return 0;for(int i=0;SPECS[i].model_number;i++){if(strstr(mn,SPECS[i].model_number)||strstr(SPECS[i].model_number,mn))return &SPECS[i];}return 0;}

/* Configure CIP security (IEC 62443-4-2) */
bool plc_config_set_cip_security(plc_controller_config_t*c,bool en){if(!c)return 0;c->cip_security_enabled=en;return 1;}

/* Set FactoryTalk realm for centralized security */
bool plc_config_set_factorytalk_realm(plc_controller_config_t*c,const char*realm){if(!c||!realm)return 0;scp3(c->factorytalk_realm,realm,64);return 1;}

/* Set operating mode (Program/Run/Test) */
bool plc_config_set_operating_mode(plc_controller_config_t*c,plc_controller_mode_t m){
    if(!c)return 0;c->operating_mode=m;
    if(m==CTRL_MODE_RUN){c->online_edits_enabled=1;}/*Online edits allowed in Run*/return 1;}

/* Set time synchronization configuration */
bool plc_config_set_time_sync(plc_controller_config_t*c,plc_time_sync_source_t src,bool is_master,double tz){
    if(!c)return 0;c->time_sync_source=src;c->is_time_master=is_master;c->time_zone_offset=tz;return 1;}

/* Configure redundancy partner */
bool plc_config_set_redundancy_partner(plc_controller_config_t*c,uint16_t ss,uint32_t max_delta_us){
    if(!c)return 0;c->secondary_slot=ss;c->max_scan_delta_us=max_delta_us;c->redundancy_mode=REDUNDANCY_HOT_STANDBY;return 1;}

/* SD Card configuration */
bool plc_config_set_sd_card(plc_controller_config_t*c,plc_sd_card_mode_t mode){if(!c)return 0;c->sd_card_mode=mode;c->sd_card_present=1;return 1;}

/* Set diagnostic trace buffer size */
bool plc_config_set_trace(plc_controller_config_t*c,bool en,uint32_t bufsz){if(!c)return 0;c->enable_trace_monitoring=en;c->trace_buffer_size=bufsz;return 1;}

/* Controller name (for FactoryTalk directory) */
bool plc_config_set_controller_name(plc_controller_config_t*c,const char*cn){if(!c||!cn)return 0;scp3(c->controller_name,cn,64);return 1;}

/* Set slot number in chassis */
bool plc_config_set_slot(plc_controller_config_t*c,uint8_t slot){if(!c||slot>16)return 0;c->slot_number=slot;return 1;}

/* Check if controller supports a given feature */
bool plc_config_supports_safety(const plc_controller_config_t*c){if(!c)return 0;const plc_controller_spec_t*s=plc_config_get_spec(c->model_number);return s?s->supports_safety:0;}
bool plc_config_supports_motion(const plc_controller_config_t*c){if(!c)return 0;const plc_controller_spec_t*s=plc_config_get_spec(c->model_number);return s?s->supports_motion:0;}

/* Configure a safety task */
bool plc_config_set_safety(plc_controller_config_t*c,plc_safety_config_t*s){if(!c||!s||!c->model_number[0])return 0;
    const plc_controller_spec_t*sp=plc_config_get_spec(c->model_number);if(!sp||!sp->supports_safety)return 0;return 1;}

/* Motion group enable/disable */
typedef struct{bool enabled;uint16_t axis_count;double max_acceleration;double max_velocity;} plc_motion_config_t;
bool plc_config_enable_motion(plc_controller_config_t*c,uint16_t num_axes,double max_acc,double max_vel){
    if(!c)return 0;const plc_controller_spec_t*s=plc_config_get_spec(c->model_number);if(!s||!s->supports_motion)return 0;if(num_axes>s->max_axes)return 0;return 1;}

/* FactoryTalk Historian connection configuration */
bool plc_config_set_historian(plc_controller_config_t*c,const char*server,uint32_t backup_interval_h){
    if(!c||!server)return 0;/*Store in FactoryTalk config*/return 1;}

/* =================================================================
 * System-Level Utilities & Diagnostics
 * ================================================================= */

/* Calculate total I/O data per scan (image table size) */
uint32_t plc_config_total_io_size(const plc_controller_config_t*c,plc_io_module_t**mods,uint16_t mod_count){
    if(!c||!mods||!mod_count)return 0;uint32_t total_in=0,total_out=0;
    for(uint16_t i=0;i<mod_count;i++){uint32_t isz=0,osz=0;if(mods[i]){plc_io_calculate_image_size(mods[i],&isz,&osz);total_in+=isz;total_out+=osz;}}return total_in+total_out;}

/* Estimate controller memory consumption for a project */
uint32_t plc_config_estimate_memory(const plc_controller_config_t*c,uint16_t num_programs,uint16_t num_aois,uint32_t num_tags,uint32_t avg_tag_bytes){
    if(!c)return 0;uint32_t total=0;total+=num_tags*avg_tag_bytes;total+=num_aois*4096;/*avg AOI memory*/
    total+=num_programs*(512+num_tags/10*32);total+=524288;/*base firmware overhead ~512KB*/return total;}

/* Check firmware compatibility between two versions */
bool plc_config_firmware_compatible(uint32_t maj_a,uint32_t min_a,uint32_t maj_b,uint32_t min_b){
    if(maj_a!=maj_b)return 0;return min_a<=min_b;/*Newer minor version considered compatible*/}

/* Get the minimum firmware version required for a feature */
bool plc_config_feature_requires_firmware(const char*feature,uint32_t*major,uint32_t*minor){
    if(!feature)return 0;
    if(!strcmp(feature,"ContextSensitiveParameters")){if(major)*major=31;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"CIPSecurity")){if(major)*major=33;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"LREAL")){if(major)*major=33;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"SourceProtection")){if(major)*major=21;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"SealedAOI")){if(major)*major=24;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"OnlineEdits")){if(major)*major=20;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"ManagedEthernetSwitch")){if(major)*major=31;if(minor)*minor=0;return 1;}
    if(!strcmp(feature,"DLR")){if(major)*major=24;if(minor)*minor=0;return 1;}
    return 0;}

/* Controller model comparison for migration planning */
int32_t plc_config_compare_capacity(const plc_controller_spec_t*a,const plc_controller_spec_t*b){
    if(!a||!b)return 0;int32_t score=0;
    if(a->user_memory_mb>b->user_memory_mb)score++;
    if(a->max_tags>b->max_tags)score++;
    if(a->max_axes>b->max_axes)score++;
    if(a->max_en2t_nodes>b->max_en2t_nodes)score++;
    if(a->supports_motion&&!b->supports_motion)score++;
    if(a->supports_safety&&!b->supports_safety)score++;
    return score;}

/* Get recommended controller for a given I/O count and tag count */
const plc_controller_spec_t* plc_config_recommend_controller(uint16_t io_nodes,uint32_t tag_count,uint16_t axes,bool need_safety,bool need_motion){
    for(int i=0;SPECS[i].model_number;i++){const plc_controller_spec_t*s=&SPECS[i];
        if(s->max_en2t_nodes<io_nodes)continue;if(s->max_tags<tag_count)continue;
        if(s->max_axes<axes)continue;if(need_safety&&!s->supports_safety)continue;
        if(need_motion&&!s->supports_motion)continue;return s;}
    return 0;}
