"""Build all source files for Rockwell AOI module."""
import os
def w(p,s):
    os.makedirs(os.path.dirname(p),exist_ok=True)
    with open(p,'w')as f:f.write(s)
    print(f"  {p}: {s.count(chr(10))} lines")

# === FILE 1: plc_rockwell_aoi.c ===
aoi=r'''/**
 * @file plc_rockwell_aoi.c
 * @brief Add-On Instruction Core Implementation (Rockwell Studio 5000/RSLogix 5000)
 * Knowledge Coverage: L1 Definitions · L2 Core Concepts · L3 Engineering Structures
 *   L4 Engineering Laws (IEC 61508 safety AOI) · L5 Algorithms (SHA-1, CSP)
 * Ref: Rockwell 1756-PM010, IEC 61131-3, FIPS 180-4
 */
#include "plc_rockwell_aoi.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
/* Reserved Logix 5000 keywords that cannot be used as AOI parameter names */
static const char* RSVD[] = {
    "XIC","XIO","OTE","OTL","OTU","ONS","OSR","OSF",
    "TON","TOF","RTO","CTU","CTD","RES",
    "CMP","EQU","NEQ","GRT","GEQ","LES","LEQ","LIM",
    "ADD","SUB","MUL","DIV","MOD","SQR","NEG","ABS",
    "MOV","MVM","AND","OR","XOR","NOT","CLR",
    "JSR","RET","SBR","TND","MCR","AFI","NOP",
    "GSV","SSV","MSG","FOR","NXT","BRK",
    "FAL","FSC","FLL","COP","CPS","SIZE",
    "PID","PIDE","IMC","SCL","SCP",
    "BOOL","SINT","INT","DINT","LINT","REAL","LREAL",
    "STRING","COUNTER","TIMER","CONTROL",
    "EnableIn","EnableOut","EN","DN","ER",
    NULL
};
static uint32_t GC = 0;
static int scmp(const char*a,const char*b){
    if(!a||!b)return(a?1:(b?-1:0));
    while(*a&&*b){char ca=(*a>='A'&&*a<='Z')?*a+32:*a;
        char cb=(*b>='A'&&*b<='Z')?*b+32:*b;
        if(ca!=cb)return(int)((unsigned char)ca-(unsigned char)cb);a++;b++;}
    return(int)((unsigned char)*a-(unsigned char)*b);}
static bool is_rsvd(const char*n){if(!n)return 1;
    for(int i=0;RSVD[i];i++)if(!scmp(n,RSVD[i]))return 1;return 0;}
static void scp(char*d,const char*s,size_t n){
    if(!d||!s||!n)return;size_t i=0;while(i<n-1&&s[i]){d[i]=s[i];i++;}d[i]=0;}
static uint16_t atsz(aoi_data_type_t dt){
    switch(dt){case AOI_TYPE_BOOL:return 4;case AOI_TYPE_SINT:return 4;
        case AOI_TYPE_INT:return 4;case AOI_TYPE_DINT:return 4;
        case AOI_TYPE_LINT:return 8;case AOI_TYPE_REAL:return 4;
        case AOI_TYPE_LREAL:return 8;case AOI_TYPE_DWORD:return 4;
        case AOI_TYPE_LWORD:return 8;case AOI_TYPE_TIMER:return 12;
        case AOI_TYPE_COUNTER:return 12;case AOI_TYPE_STRING:return 88;
        default:return 4;}}
/* ---- AOI Definition Create/Free ---- */
aoi_definition_t* aoi_definition_create(const char*n,const char*desc){
    if(!n||!*n)return 0;if(is_rsvd(n))return 0;if(strlen(n)>39)return 0;
    aoi_definition_t*d=calloc(1,sizeof(aoi_definition_t));if(!d)return 0;
    scp(d->name,n,40);if(desc)scp(d->description,desc,512);
    d->scan_mode=AOI_SCAN_OVERLOAD;d->has_enablein=d->has_enableout=0;
    d->is_source_protected=d->is_certified=0;d->estimated_scan_ns=0;
    d->memory_bytes=0;d->csp_count=0;d->csp_overrides=0;
    d->logic_callback=0;d->logic_user_data=0;
    d->revision.major_version=1;d->revision.minor_version=0;
    scp(d->revision.revision_note,"Initial creation",128);
    d->revision.source_protected=d->revision.sealed=0;
    memset(d->revision.signature.digest,0,20);
    d->revision.signature.timestamp=0;d->revision.signature.change_counter=0;
    return d;}
void aoi_definition_free(aoi_definition_t*d){if(d){free(d->csp_overrides);free(d);}}
/* ---- AOI Parameter Management ---- */
aoi_validation_error_t aoi_definition_add_parameter(
    aoi_definition_t*d,const char*n,aoi_param_direction_t dir,
    aoi_data_type_t dt,bool req){
    if(!d)return AOI_VALID_ERR_TYPE_MISMATCH;
    if(!n||!*n||strlen(n)>39||is_rsvd(n))return AOI_VALID_ERR_NAME_RESERVED;
    if(d->parameter_count>=AOI_MAX_PARAMETERS)return AOI_VALID_ERR_PARAM_COUNT;
    for(uint16_t i=0;i<d->parameter_count;i++)
        if(!scmp(d->parameters[i].name,n))return AOI_VALID_ERR_NAME_DUP;
    if(dir==AOI_PARAM_ENABLEIN)d->has_enablein=1;
    if(dir==AOI_PARAM_ENABLEOUT)d->has_enableout=1;
    aoi_parameter_t*p=&d->parameters[d->parameter_count];
    scp(p->name,n,40);p->direction=dir;p->data_type=dt;
    p->required=req;p->visible=1;p->constant=0;
    p->min_value=0.0;p->max_value=0.0;p->bit_mask=0;
    d->parameter_count++;d->memory_bytes+=atsz(dt);
    d->estimated_scan_ns+=60;d->revision.signature.change_counter=++GC;
    return AOI_VALID_OK;}
/* ---- AOI Local Tag Management ---- */
aoi_validation_error_t aoi_definition_add_local_tag(
    aoi_definition_t*d,const char*n,aoi_data_type_t dt,bool rp){
    if(!d||!n||!*n||strlen(n)>39||is_rsvd(n))return AOI_VALID_ERR_NAME_RESERVED;
    if(d->local_tag_count>=AOI_MAX_LOCAL_TAGS)return AOI_VALID_ERR_TAG_COUNT;
    for(uint16_t i=0;i<d->parameter_count;i++)
        if(!scmp(d->parameters[i].name,n))return AOI_VALID_ERR_NAME_DUP;
    for(uint16_t i=0;i<d->local_tag_count;i++)
        if(!scmp(d->local_tags[i].tag_name,n))return AOI_VALID_ERR_NAME_DUP;
    aoi_local_tag_t*t=&d->local_tags[d->local_tag_count];
    scp(t->tag_name,n,40);t->data_type=dt;memset(&t->initial_value,0,sizeof(aoi_atomic_value_t));
    t->retain_on_powerup=rp;t->is_array=0;t->array_dims_count=0;
    d->local_tag_count++;d->revision.signature.change_counter=++GC;
    return AOI_VALID_OK;}
/* ---- Scan Mode & Safety Cert ---- */
void aoi_definition_set_scan_mode(aoi_definition_t*d,aoi_scan_mode_t m){
    if(!d)return;d->scan_mode=m;d->revision.signature.change_counter=++GC;
    if(m==AOI_SCAN_ALWAYS_SCAN){
        if(!d->has_enablein)aoi_definition_add_parameter(d,"EnableIn",AOI_PARAM_ENABLEIN,AOI_TYPE_BOOL,1);
        if(!d->has_enableout)aoi_definition_add_parameter(d,"EnableOut",AOI_PARAM_ENABLEOUT,AOI_TYPE_BOOL,0);}}
void aoi_definition_set_safety_cert(aoi_definition_t*d,const aoi_safety_cert_t*c){
    if(!d||!c)return;d->is_certified=1;d->is_source_protected=1;
    for(uint16_t i=0;i<d->parameter_count;i++)d->parameters[i].required=1;
    d->scan_mode=AOI_SCAN_ALWAYS_SCAN;d->revision.signature.change_counter=++GC;}
/* ---- AOI Instance Create/Free/Reset ---- */
aoi_instance_t* aoi_instance_create(const char*n,aoi_definition_t*d){
    if(!n||!d)return 0;
    aoi_instance_t*i=calloc(1,sizeof(aoi_instance_t));if(!i)return 0;
    scp(i->instance_name,n,40);i->definition=d;
    i->enable_in=i->enable_out=i->error=0;i->error_code=0;
    i->scan_counter=0;i->last_scan_us=0;
    if(d->parameter_count>0){i->parameter_values=calloc(d->parameter_count,sizeof(aoi_atomic_value_t));
        if(!i->parameter_values){free(i);return 0;}}
    else i->parameter_values=0;
    if(d->local_tag_count>0){i->local_tag_values=calloc(d->local_tag_count,sizeof(aoi_atomic_value_t));
        if(!i->local_tag_values){free(i->parameter_values);free(i);return 0;}
        for(uint16_t j=0;j<d->local_tag_count;j++)i->local_tag_values[j]=d->local_tags[j].initial_value;}
    else i->local_tag_values=0;
    return i;}
void aoi_instance_free(aoi_instance_t*i){if(!i)return;free(i->parameter_values);free(i->local_tag_values);free(i);}
void aoi_instance_reset(aoi_instance_t*i){
    if(!i||!i->definition)return;aoi_definition_t*d=i->definition;
    for(uint16_t j=0;j<d->parameter_count;j++)memset(&i->parameter_values[j],0,sizeof(aoi_atomic_value_t));
    for(uint16_t j=0;j<d->local_tag_count;j++)i->local_tag_values[j]=d->local_tags[j].initial_value;
    i->enable_in=i->enable_out=i->error=0;i->error_code=0;}
void aoi_definition_set_logic_callback(aoi_definition_t*d,aoi_logic_callback_t cb,void*ud)
    {if(d){d->logic_callback=cb;d->logic_user_data=ud;}}
/* ---- AOI Execution Engine ---- */
bool aoi_instance_execute(aoi_instance_t*i,const aoi_execution_context_t*c){
    if(!i||!i->definition||!c)return 0;
    aoi_definition_t*d=i->definition;bool en=i->enable_in;
    if(c->phase==AOI_PHASE_PRESCAN)en=0;
    if(d->scan_mode==AOI_SCAN_PRESCAN_IMMUNE&&c->phase==AOI_PHASE_PRESCAN){i->enable_out=0;return 1;}
    if(d->scan_mode!=AOI_SCAN_ALWAYS_SCAN&&!en){i->enable_out=0;return 1;}
    i->enable_out=1;i->scan_counter++;
    if(d->logic_callback){d->logic_callback(i,c,d->logic_user_data);if(i->error){i->enable_out=0;return 0;}}
    return 1;}
/* ---- Parameter Access by Name (O(P) linear search) ---- */
bool aoi_instance_get_param(aoi_instance_t*i,const char*n,aoi_atomic_value_t*v){
    if(!i||!n||!v)return 0;if(!i->definition||!i->parameter_values)return 0;
    aoi_definition_t*d=i->definition;
    for(uint16_t j=0;j<d->parameter_count;j++)if(!scmp(d->parameters[j].name,n)){*v=i->parameter_values[j];return 1;}
    return 0;}
aoi_validation_error_t aoi_instance_set_param(aoi_instance_t*i,const char*n,const aoi_atomic_value_t*v){
    if(!i||!n||!v)return AOI_VALID_ERR_TYPE_MISMATCH;
    if(!i->definition||!i->parameter_values)return AOI_VALID_ERR_TYPE_MISMATCH;
    aoi_definition_t*d=i->definition;
    for(uint16_t j=0;j<d->parameter_count;j++)if(!scmp(d->parameters[j].name,n)){
        if(d->parameters[j].constant)return AOI_VALID_ERR_CONST_IMMUTABLE;
        i->parameter_values[j]=*v;return AOI_VALID_OK;}
    return AOI_VALID_ERR_NAME_DUP;}
/* ---- Local Tag Access by Name ---- */
bool aoi_instance_get_local_tag(aoi_instance_t*i,const char*n,aoi_atomic_value_t*v){
    if(!i||!n||!v)return 0;if(!i->definition||!i->local_tag_values)return 0;
    aoi_definition_t*d=i->definition;
    for(uint16_t j=0;j<d->local_tag_count;j++)if(!scmp(d->local_tags[j].tag_name,n)){*v=i->local_tag_values[j];return 1;}
    return 0;}
aoi_validation_error_t aoi_instance_set_local_tag(aoi_instance_t*i,const char*n,const aoi_atomic_value_t*v){
    if(!i||!n||!v)return AOI_VALID_ERR_TYPE_MISMATCH;
    if(!i->definition||!i->local_tag_values)return AOI_VALID_ERR_TYPE_MISMATCH;
    aoi_definition_t*d=i->definition;
    for(uint16_t j=0;j<d->local_tag_count;j++)if(!scmp(d->local_tags[j].tag_name,n)){
        i->local_tag_values[j]=*v;return AOI_VALID_OK;}
    return AOI_VALID_ERR_NAME_DUP;}
/* ---- O(1) Index-based Access ---- */
int32_t aoi_definition_get_param_index(const aoi_definition_t*d,const char*n){
    if(!d||!n)return -1;for(uint16_t i=0;i<d->parameter_count;i++)if(!scmp(d->parameters[i].name,n))return(int32_t)i;return -1;}
aoi_atomic_value_t aoi_instance_get_param_by_index(const aoi_instance_t*i,uint16_t idx){
    aoi_atomic_value_t z;memset(&z,0,sizeof(z));
    if(!i||!i->definition||!i->parameter_values||idx>=i->definition->parameter_count)return z;
    return i->parameter_values[idx];}
void aoi_instance_set_param_by_index(aoi_instance_t*i,uint16_t idx,const aoi_atomic_value_t*v){
    if(!i||!i->definition||!i->parameter_values||!v)return;
    if(idx>=i->definition->parameter_count||i->definition->parameters[idx].constant)return;
    i->parameter_values[idx]=*v;}
/* ---- AOI Definition Validation ---- */
aoi_validation_error_t aoi_definition_validate(const aoi_definition_t*d){
    if(!d)return AOI_VALID_ERR_TYPE_MISMATCH;if(!d->name[0]||is_rsvd(d->name))return AOI_VALID_ERR_NAME_RESERVED;
    for(uint16_t i=0;i<d->parameter_count;i++)for(uint16_t j=i+1;j<d->parameter_count;j++)
        if(!scmp(d->parameters[i].name,d->parameters[j].name))return AOI_VALID_ERR_NAME_DUP;
    for(uint16_t i=0;i<d->parameter_count;i++)if(d->parameters[i].data_type>AOI_TYPE_AOI_REF)return AOI_VALID_ERR_TYPE_MISMATCH;
    return AOI_VALID_OK;}
/* ---- AOI Instance Validation (incl. safety checks per IEC 61508-3) ---- */
aoi_validation_error_t aoi_instance_validate(const aoi_instance_t*i){
    if(!i||!i->definition)return AOI_VALID_ERR_TYPE_MISMATCH;
    aoi_definition_t*d=i->definition;
    if(d->is_certified){for(uint16_t j=0;j<d->parameter_count;j++)if(!d->parameters[j].required)return AOI_VALID_ERR_SAFETY_NONCERT;
        if(!d->is_source_protected)return AOI_VALID_ERR_SAFETY_NONCERT;}
    return AOI_VALID_OK;}

/* =================================================================
 * SHA-1 Implementation (FIPS 180-4) for AOI Signature Computation
 * Used by Rockwell Studio 5000 for instruction integrity verification.
 * Ref: FIPS 180-4 S6.1, Rockwell KB BF12312
 * ================================================================= */
static uint32_t rl32(uint32_t x,uint8_t n){return(x<<n)|(x>>(32-n));}
static void s1i(aoi_sha1_ctx_t*c){c->state[0]=0x67452301;c->state[1]=0xEFCDAB89;c->state[2]=0x98BADCFE;c->state[3]=0x10325476;c->state[4]=0xC3D2E1F0;c->byte_count=0;c->buffer_offset=0;memset(c->buffer,0,64);}
static void s1t(aoi_sha1_ctx_t*c,const uint8_t b[64]){
    uint32_t w[80],a,B,cd,d,e;for(int i=0;i<16;i++)w[i]=((uint32_t)b[i*4]<<24)|((uint32_t)b[i*4+1]<<16)|((uint32_t)b[i*4+2]<<8)|((uint32_t)b[i*4+3]);
    for(int i=16;i<80;i++)w[i]=rl32(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=c->state[0];B=c->state[1];cd=c->state[2];d=c->state[3];e=c->state[4];
    for(int i=0;i<80;i++){uint32_t f,k;
        if(i<20){f=(B&cd)|((~B)&d);k=0x5A827999;}else if(i<40){f=B^cd^d;k=0x6ED9EBA1;}
        else if(i<60){f=(B&cd)|(B&d)|(cd&d);k=0x8F1BBCDC;}else{f=B^cd^d;k=0xCA62C1D6;}
        uint32_t t=rl32(a,5)+f+e+k+w[i];e=d;d=cd;cd=rl32(B,30);B=a;a=t;}
    c->state[0]+=a;c->state[1]+=B;c->state[2]+=cd;c->state[3]+=d;c->state[4]+=e;}
static void s1u(aoi_sha1_ctx_t*c,const uint8_t*data,size_t len){for(size_t i=0;i<len;i++){c->buffer[c->buffer_offset++]=data[i];c->byte_count++;if(c->buffer_offset==64){s1t(c,c->buffer);c->buffer_offset=0;}}}
static void s1f(aoi_sha1_ctx_t*c,uint8_t dg[20]){
    uint64_t bc=c->byte_count*8;c->buffer[c->buffer_offset++]=0x80;
    if(c->buffer_offset>56){while(c->buffer_offset<64)c->buffer[c->buffer_offset++]=0;s1t(c,c->buffer);c->buffer_offset=0;}
    while(c->buffer_offset<56)c->buffer[c->buffer_offset++]=0;
    for(int i=7;i>=0;i--){c->buffer[56+i]=(uint8_t)(bc&0xFF);bc>>=8;}s1t(c,c->buffer);
    for(int i=0;i<5;i++){dg[i*4]=(c->state[i]>>24)&0xFF;dg[i*4+1]=(c->state[i]>>16)&0xFF;dg[i*4+2]=(c->state[i]>>8)&0xFF;dg[i*4+3]=c->state[i]&0xFF;}}
/* ---- Canonical Serialization Helpers (big-endian writes) ---- */
static void wb32(uint8_t**b,uint32_t v){(*b)[0]=(v>>24)&0xFF;(*b)[1]=(v>>16)&0xFF;(*b)[2]=(v>>8)&0xFF;(*b)[3]=v&0xFF;*b+=4;}
static void wb16(uint8_t**b,uint16_t v){(*b)[0]=(v>>8)&0xFF;(*b)[1]=v&0xFF;*b+=2;}
static int pnc(const void*a,const void*b){return scmp(((const aoi_parameter_t*)a)->name,((const aoi_parameter_t*)b)->name);}
/* ---- AOI Signature Computation ---- */
void aoi_compute_signature(const aoi_definition_t*d,aoi_signature_t*s){
    if(!d||!s)return;uint8_t buf[8192];uint8_t*p=buf;size_t nl=strlen(d->name);memcpy(p,d->name,nl);p+=nl;*p++=0;
    wb32(&p,(uint32_t)d->scan_mode);
    aoi_parameter_t sp[AOI_MAX_PARAMETERS];uint16_t pc=d->parameter_count;
    if(pc>0){memcpy(sp,d->parameters,pc*sizeof(aoi_parameter_t));qsort(sp,pc,sizeof(aoi_parameter_t),pnc);
        for(uint16_t i=0;i<pc;i++){nl=strlen(sp[i].name);memcpy(p,sp[i].name,nl);p+=nl;*p++=0;
            *p++=(uint8_t)sp[i].direction;*p++=(uint8_t)sp[i].data_type;*p++=sp[i].required?1:0;*p++=sp[i].visible?1:0;}}
    wb16(&p,pc);
    aoi_local_tag_t st[AOI_MAX_LOCAL_TAGS];uint16_t tc=d->local_tag_count;
    if(tc>0){memcpy(st,d->local_tags,tc*sizeof(aoi_local_tag_t));
        for(uint16_t i=0;i<tc;i++)for(uint16_t j=i+1;j<tc;j++)if(scmp(st[i].tag_name,st[j].tag_name)>0){aoi_local_tag_t t=st[i];st[i]=st[j];st[j]=t;}
        for(uint16_t i=0;i<tc;i++){nl=strlen(st[i].tag_name);memcpy(p,st[i].tag_name,nl);p+=nl;*p++=0;*p++=(uint8_t)st[i].data_type;*p++=st[i].retain_on_powerup?1:0;}}
    wb16(&p,tc);
    size_t tl=(size_t)(p-buf);aoi_sha1_ctx_t c;s1i(&c);s1u(&c,buf,tl);s1f(&c,s->digest);
    s->timestamp=(uint32_t)time(0);s->change_counter=d->revision.signature.change_counter;}
/* ---- Constant-Time Signature Comparison ---- */
bool aoi_signature_equal(const aoi_signature_t*a,const aoi_signature_t*b){
    if(!a||!b)return 0;uint8_t r=0;for(int i=0;i<AOI_SIGNATURE_LENGTH;i++)r|=(a->digest[i]^b->digest[i]);return r==0;}
/* ---- AOI Library Management ---- */
aoi_library_t* aoi_library_create(const char*pn,const char*ct,uint32_t fm,uint32_t fn){
    if(!pn)return 0;aoi_library_t*l=calloc(1,sizeof(aoi_library_t));if(!l)return 0;
    scp(l->project_name,pn,64);if(ct)scp(l->controller_type,ct,32);l->firmware_major=fm;l->firmware_minor=fn;
    l->aoi_count=0;l->aoi_capacity=16;l->aoi_definitions=calloc(16,sizeof(aoi_definition_t));
    if(!l->aoi_definitions){free(l);return 0;}memset(&l->library_signature,0,20);return l;}
void aoi_library_free(aoi_library_t*l){if(!l)return;
    for(uint16_t i=0;i<l->aoi_count;i++)free(l->aoi_definitions[i].csp_overrides);
    free(l->aoi_definitions);free(l);}
aoi_validation_error_t aoi_library_add_aoi(aoi_library_t*l,aoi_definition_t*d){
    if(!l||!d)return AOI_VALID_ERR_TYPE_MISMATCH;
    for(uint16_t i=0;i<l->aoi_count;i++)if(!scmp(l->aoi_definitions[i].name,d->name))return AOI_VALID_ERR_NAME_DUP;
    if(l->aoi_count>=l->aoi_capacity){uint16_t nc=l->aoi_capacity*2;
        aoi_definition_t*na=realloc(l->aoi_definitions,nc*sizeof(aoi_definition_t));if(!na)return AOI_VALID_ERR_PARAM_COUNT;
        memset(na+l->aoi_capacity,0,(nc-l->aoi_capacity)*sizeof(aoi_definition_t));l->aoi_definitions=na;l->aoi_capacity=nc;}
    memcpy(&l->aoi_definitions[l->aoi_count],d,sizeof(aoi_definition_t));l->aoi_count++;return AOI_VALID_OK;}
aoi_definition_t* aoi_library_find_aoi(const aoi_library_t*l,const char*n){
    if(!l||!n)return 0;for(uint16_t i=0;i<l->aoi_count;i++)if(!scmp(l->aoi_definitions[i].name,n))return &l->aoi_definitions[i];return 0;}
uint16_t aoi_library_count(const aoi_library_t*l){return l?l->aoi_count:0;}
void aoi_library_compute_signature(aoi_library_t*l){
    if(!l)return;for(uint16_t i=0;i<l->aoi_count;i++)aoi_compute_signature(&l->aoi_definitions[i],&l->aoi_definitions[i].revision.signature);
    uint16_t ix[64];for(uint16_t i=0;i<l->aoi_count&&i<64;i++)ix[i]=i;
    for(uint16_t i=0;i<l->aoi_count&&i<64;i++)for(uint16_t j=i+1;j<l->aoi_count&&j<64;j++)
        if(scmp(l->aoi_definitions[ix[i]].name,l->aoi_definitions[ix[j]].name)>0){uint16_t t=ix[i];ix[i]=ix[j];ix[j]=t;}
    aoi_sha1_ctx_t c;s1i(&c);for(uint16_t i=0;i<l->aoi_count&&i<64;i++){aoi_definition_t*a=&l->aoi_definitions[ix[i]];
        s1u(&c,(const uint8_t*)a->name,strlen(a->name));s1u(&c,a->revision.signature.digest,20);}s1f(&c,l->library_signature.digest);}
/* ---- Context-Sensitive Parameter (Studio 5000 v31+) ---- */
aoi_validation_error_t aoi_definition_add_context_override(aoi_definition_t*d,const aoi_context_param_t*c){
    if(!d||!c)return AOI_VALID_ERR_TYPE_MISMATCH;if(c->param_index>=d->parameter_count)return AOI_VALID_ERR_PARAM_COUNT;
    uint16_t nc=d->csp_count+1;aoi_context_param_t*na=realloc(d->csp_overrides,nc*sizeof(aoi_context_param_t));
    if(!na)return AOI_VALID_ERR_PARAM_COUNT;na[d->csp_count]=*c;d->csp_overrides=na;d->csp_count=nc;
    d->revision.signature.change_counter=++GC;return AOI_VALID_OK;}
void aoi_definition_resolve_context(const aoi_definition_t*d,uint16_t pi,aoi_context_type_t ctx,bool*er,bool*ev){
    if(!d||pi>=d->parameter_count){if(er)*er=0;if(ev)*ev=0;return;}
    bool rq=d->parameters[pi].required;bool vs=d->parameters[pi].visible;
    for(uint16_t i=0;i<d->csp_count;i++){if(d->csp_overrides[i].param_index==pi&&d->csp_overrides[i].context==ctx){
        if(d->csp_overrides[i].override_required)rq=d->csp_overrides[i].new_required;
        if(d->csp_overrides[i].override_visible)vs=d->csp_overrides[i].new_visible;}}
    if(er)*er=rq;if(ev)*ev=vs;}
'''
w("src/plc_rockwell_aoi.c", aoi)

# === FILE 2: plc_rockwell_tag.c ===
tag=r'''/**
 * @file plc_rockwell_tag.c
 * @brief Rockwell Tag System Implementation - Tag Database, UDTs, Memory Model
 * Knowledge: L1-L5. Ref: 1756-PM004, IEC 61131-3
 */
#include "plc_rockwell_tag.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* ---- Helpers ---- */
static void scp2(char*d,const char*s,size_t n){if(!d||!s||!n)return;size_t i=0;while(i<n-1&&s[i]){d[i]=s[i];i++;}d[i]=0;}
static int scmp2(const char*a,const char*b){if(!a||!b)return(a?1:(b?-1:0));while(*a&&*b){char ca=(*a>='A'&&*a<='Z')?*a+32:*a;char cb=(*b>='A'&&*b<='Z')?*b+32:*b;if(ca!=cb)return(int)((unsigned char)ca-(unsigned char)cb);a++;b++;}return(int)((unsigned char)*a-(unsigned char)*b);}

/* ---- Tag Database Init ---- */
void plc_tag_database_init(plc_tag_database_t*db,uint32_t mps){if(!db)return;memset(db,0,sizeof(plc_tag_database_t));db->memory_pool_size=mps;}
/* ---- Create a tag in the database ---- */
plc_tag_t* plc_tag_database_create_tag(plc_tag_database_t*db,const char*n,plc_tag_scope_t sc,plc_type_class_t tc,uint16_t tcode,const char*owner){
    if(!db||!n||!*n||strlen(n)>63)return 0;
    if(db->tag_count>=TAG_DB_MAX_TAGS)return 0;
    for(uint16_t i=0;i<db->tag_count;i++){plc_tag_t*t=&db->tags[i];
        if(!scmp2(t->name,n)){if(t->scope==sc)return 0;/*duplicate*/}}
    plc_tag_t*t=&db->tags[db->tag_count];memset(t,0,sizeof(plc_tag_t));
    scp2(t->name,n,64);t->scope=sc;t->type_class=tc;t->type_code=tcode;
    if(owner&&sc==TAG_SCOPE_PROGRAM)scp2(t->owner,owner,64);
    t->tag_class=TAG_CLASS_STANDARD;t->style=TAG_STYLE_DECIMAL;t->external_access=TAG_EXTERNAL_NONE;
    t->is_array=0;t->array_dims_count=0;t->retain_on_powerup=0;
    uint32_t sz=plc_type_atomic_size(tcode);
    if(db->memory_used+sz>db->memory_pool_size)return 0;
    t->memory_offset=db->memory_used;t->memory_size=sz;db->memory_used+=sz;
    db->tag_count++;return t;}
/* ---- Delete a tag from the database ---- */
bool plc_tag_database_delete_tag(plc_tag_database_t*db,const char*n){if(!db||!n)return 0;
    for(uint16_t i=0;i<db->tag_count;i++){if(!scmp2(db->tags[i].name,n)){
        db->memory_used-=db->tags[i].memory_size;db->tags[i]=db->tags[db->tag_count-1];db->tag_count--;return 1;}}return 0;}
/* ---- Tag Resolution Algorithm ---- */
void plc_tag_resolve(const plc_tag_database_t*db,const char*n,const char*cprog,plc_tag_search_result_t*r){
    if(!db||!n||!r){if(r)memset(r,0,sizeof(plc_tag_search_result_t));return;}
    memset(r,0,sizeof(plc_tag_search_result_t));r->tag_index=0xFFFF;
    /* 1. Check program scope first if current_program given */
    if(cprog&&*cprog){for(uint16_t i=0;i<db->tag_count;i++){const plc_tag_t*t=&db->tags[i];
        if(t->scope==TAG_SCOPE_PROGRAM&&!scmp2(t->owner,cprog)&&!scmp2(t->name,n)){r->found=1;r->tag_index=i;r->is_program_scoped=1;return;}}}
    /* 2. Check controller scope */
    for(uint16_t i=0;i<db->tag_count;i++){const plc_tag_t*t=&db->tags[i];
        if(t->scope==TAG_SCOPE_CONTROLLER&&!scmp2(t->name,n)){r->found=1;r->tag_index=i;r->is_program_scoped=0;return;}}}
/* ---- UDT Management ---- */
plc_udt_t* plc_udt_create(plc_tag_database_t*db,const char*n,const char*desc){
    if(!db||!n||!*n||strlen(n)>39)return 0;if(db->udt_count>=TAG_DB_MAX_UDTS)return 0;
    for(uint16_t i=0;i<db->udt_count;i++)if(!scmp2(db->udts[i].name,n))return 0;
    plc_udt_t*u=&db->udts[db->udt_count];memset(u,0,sizeof(plc_udt_t));
    scp2(u->name,n,40);if(desc)scp2(u->description,desc,512);db->udt_count++;return u;}
bool plc_udt_add_member(plc_udt_t*u,const char*n,plc_type_class_t tc,uint16_t tcode){
    if(!u||!n||!*n||strlen(n)>39)return 0;if(u->member_count>=UDT_MAX_MEMBERS)return 0;
    for(uint16_t i=0;i<u->member_count;i++)if(!scmp2(u->members[i].name,n))return 0;
    plc_udt_member_t*m=&u->members[u->member_count];memset(m,0,sizeof(plc_udt_member_t));
    scp2(m->name,n,40);m->type_class=tc;m->type_code=tcode;m->byte_offset=u->total_byte_size;
    m->byte_size=plc_type_atomic_size(tcode);
    /* Align offset to member's alignment boundary */
    uint32_t align=(m->byte_size>4)?8:4;
    if(m->byte_offset%align){m->byte_offset+=align-(m->byte_offset%align);}
    u->total_byte_size=m->byte_offset+m->byte_size;u->member_count++;return 1;}
plc_udt_t* plc_udt_find(plc_tag_database_t*db,const char*n){if(!db||!n)return 0;
    for(uint16_t i=0;i<db->udt_count;i++)if(!scmp2(db->udts[i].name,n))return &db->udts[i];return 0;}
/* ---- Type Size Calculation ---- */
uint32_t plc_type_atomic_size(uint16_t tc){
    switch(tc){case AOI_TYPE_BOOL:return 4;case AOI_TYPE_SINT:return 4;case AOI_TYPE_INT:return 4;
        case AOI_TYPE_DINT:return 4;case AOI_TYPE_LINT:return 8;case AOI_TYPE_REAL:return 4;
        case AOI_TYPE_LREAL:return 8;case AOI_TYPE_DWORD:return 4;case AOI_TYPE_LWORD:return 8;
        case AOI_TYPE_TIMER:return 12;case AOI_TYPE_COUNTER:return 12;case AOI_TYPE_STRING:return 88;default:return 4;}}
/* ---- UDT Size Computation (with alignment) ---- */
uint32_t plc_udt_compute_byte_size(const plc_udt_t*u){
    if(!u||!u->member_count)return 0;uint32_t sz=0;
    for(uint16_t i=0;i<u->member_count;i++){const plc_udt_member_t*m=&u->members[i];
        uint32_t a=(m->byte_size>4)?8:4;if(sz%a)sz+=a-(sz%a);sz+=m->byte_size;}
    /* Pad to largest alignment */
    uint32_t max_align=4;for(uint16_t i=0;i<u->member_count;i++){if(u->members[i].byte_size>max_align)max_align=u->members[i].byte_size;}
    if(sz%max_align)sz+=max_align-(sz%max_align);return sz;}
/* ---- L5K Export ---- */
uint32_t plc_tag_database_export_l5k(const plc_tag_database_t*db,char*buf,uint32_t bsz){
    if(!db||!buf||!bsz)return 0;uint32_t pos=0;
    pos+=snprintf(buf+pos,bsz-pos,"TAG\n");
    for(uint16_t i=0;i<db->tag_count;i++){const plc_tag_t*t=&db->tags[i];if(bsz-pos<256)break;
        pos+=snprintf(buf+pos,bsz-pos,"  %s : %s := 0;\n",t->name,t->scope==TAG_SCOPE_PROGRAM?"LOCAL":"GLOBAL");}
    pos+=snprintf(buf+pos,bsz-pos,"END_TAG\n");return pos;}
'''
w("src/plc_rockwell_tag.c", tag)

# === FILE 3: plc_rockwell_config.c ===
cfg=r'''/**
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
'''
w("src/plc_rockwell_config.c", cfg)

# === FILE 4: plc_rockwell_cip.c ===
cip=r'''/**
 * @file plc_rockwell_cip.c
 * @brief Common Industrial Protocol (CIP) Implementation - EtherNet/IP
 * Knowledge: L1-L7. Ref: ODVA CIP Vol 1 & 2, Rockwell 1756-PM020
 */
#include "plc_rockwell_cip.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
/* ---- Helpers ---- */
static void scp4(char*d,const char*s,size_t n){if(!d||!s||!n)return;size_t i=0;while(i<n-1&&s[i]){d[i]=s[i];i++;}d[i]=0;}
/* ---- CIP Identity Object ---- */
cip_identity_t* cip_identity_create(uint16_t vid,uint16_t dt,uint16_t pc,const char*pn){
    cip_identity_t*id=calloc(1,sizeof(cip_identity_t));if(!id)return 0;
    id->vendor_id=vid;id->device_type=dt;id->product_code=pc;id->major_revision=1;id->minor_revision=0;
    id->status=CIP_STATUS_OWNED;id->serial_number=0xDEADBEEF;if(pn)scp4(id->product_name,pn,32);id->state=3;return id;}
void cip_identity_free(cip_identity_t*id){free(id);}
/* ---- Build Get_Attribute_All for Identity Object ---- */
bool cip_build_identity_request(cip_request_t*r){
    if(!r)return 0;memset(r,0,sizeof(cip_request_t));r->service_code=CIP_SERVICE_GET_ATTRIBUTE_ALL;
    cip_epath_init(&r->request_path);cip_epath_add_class(&r->request_path,CIP_CLASS_IDENTITY);
    cip_epath_add_instance(&r->request_path,1);r->request_data_len=0;return 1;}
/* ---- Parse Identity Response ---- */
bool cip_parse_identity_response(const cip_response_t*r,cip_identity_t*id){
    if(!r||!id||r->general_status!=0)return 0;const uint8_t*d=r->response_data;
    if(r->response_data_len<20)return 0;
    id->vendor_id=(uint16_t)(d[0]|(d[1]<<8));id->device_type=(uint16_t)(d[2]|(d[3]<<8));
    id->product_code=(uint16_t)(d[4]|(d[5]<<8));id->major_revision=d[6];id->minor_revision=d[7];
    id->status=(uint16_t)(d[8]|(d[9]<<8));id->serial_number=(uint32_t)(d[10]|(d[11]<<8)|(d[12]<<16)|(d[13]<<24));
    scp4(id->product_name,(const char*)(d+14),32);id->state=d[14+32]?d[14+32]:3;return 1;}
/* ---- Connection Manager ---- */
void cip_conn_mgr_init(cip_connection_manager_t*m){if(!m)return;memset(m,0,sizeof(cip_connection_manager_t));m->next_transaction_id=1;}
cip_status_code_t cip_conn_mgr_forward_open(cip_connection_manager_t*m,const cip_forward_open_params_t*p,cip_connection_t**oc){
    if(!m||!p||!oc)return CIP_STATUS_INVALID_PARAMETER;if(m->connection_count>=CIP_MAX_CONNECTIONS)return CIP_STATUS_RESOURCE_UNAVAILABLE;
    cip_connection_t*c=&m->connections[m->connection_count];memset(c,0,sizeof(cip_connection_t));
    c->connection_id=m->next_transaction_id++;c->conn_type=(p->transport_trigger==CIP_TRIGGER_CYCLIC)?CIP_CONN_IMPLICIT_IO:CIP_CONN_EXPLICIT;
    c->state=CIP_CONN_STATE_ESTABLISHED;c->o_to_t_size=p->o_to_t_size;c->t_to_o_size=p->t_to_o_size;
    c->requested_packet_interval=p->o_to_t_rpi_ns/1000;c->actual_packet_interval=c->requested_packet_interval;
    c->watchdog_timeout=c->requested_packet_interval*4;m->connection_count++;*oc=c;return CIP_STATUS_SUCCESS;}
cip_status_code_t cip_conn_mgr_forward_close(cip_connection_manager_t*m,uint32_t cid){
    if(!m)return CIP_STATUS_INVALID_PARAMETER;cip_connection_t*c=cip_conn_mgr_find(m,cid);if(!c)return CIP_STATUS_OBJECT_DOES_NOT_EXIST;
    c->state=CIP_CONN_STATE_CLOSING;memset(c,0,sizeof(cip_connection_t));return CIP_STATUS_SUCCESS;}
cip_connection_t* cip_conn_mgr_find(const cip_connection_manager_t*m,uint32_t cid){
    if(!m)return 0;for(uint16_t i=0;i<m->connection_count;i++)if(m->connections[i].connection_id==cid)return &m->connections[i];return 0;}
uint16_t cip_conn_mgr_cleanup_timeouts(cip_connection_manager_t*m,uint32_t ctm){
    if(!m)return 0;uint16_t cnt=0;for(uint16_t i=0;i<m->connection_count;i++){cip_connection_t*c=&m->connections[i];
        if(c->state==CIP_CONN_STATE_ESTABLISHED&&ctm-c->last_packet_time_ms>c->watchdog_timeout){c->state=CIP_CONN_STATE_TIMED_OUT;cnt++;}}return cnt;}
/* ---- EPATH Construction ---- */
void cip_epath_init(cip_epath_t*e){if(e){memset(e,0,sizeof(cip_epath_t));}}
bool cip_epath_add_class(cip_epath_t*e,uint32_t cid){if(!e||e->segment_count>=CIP_MAX_EPATH_SEGMENTS)return 0;
    cip_epath_segment_t*s=&e->segments[e->segment_count];s->segment_type=0x20;s->segment_format=0;/*8-bit if<256*/
    if(cid<256){s->segment_format=0;s->segment_data.value_8=(uint8_t)cid;}
    else if(cid<65536){s->segment_format=1;s->segment_data.value_16=(uint16_t)cid;}
    else{s->segment_format=2;s->segment_data.value_32=cid;}e->segment_count++;return 1;}
bool cip_epath_add_instance(cip_epath_t*e,uint32_t iid){if(!e||e->segment_count>=CIP_MAX_EPATH_SEGMENTS)return 0;
    cip_epath_segment_t*s=&e->segments[e->segment_count];s->segment_type=0x24;
    if(iid<256){s->segment_format=0;s->segment_data.value_8=(uint8_t)iid;}
    else if(iid<65536){s->segment_format=1;s->segment_data.value_16=(uint16_t)iid;}
    else{s->segment_format=2;s->segment_data.value_32=iid;}e->segment_count++;return 1;}
bool cip_epath_add_attribute(cip_epath_t*e,uint32_t aid){if(!e||e->segment_count>=CIP_MAX_EPATH_SEGMENTS)return 0;
    cip_epath_segment_t*s=&e->segments[e->segment_count];s->segment_type=0x30;s->segment_format=0;s->segment_data.value_8=(uint8_t)aid;e->segment_count++;return 1;}
/* ---- EPATH: Add Symbol (Logix tag name) ---- */
bool cip_epath_add_symbol(cip_epath_t*e,const char*tn){
    if(!e||!tn||e->segment_count>=CIP_MAX_EPATH_SEGMENTS)return 0;
    cip_epath_segment_t*s=&e->segments[e->segment_count];s->segment_type=0x80;/*symbolic segment*/
    s->segment_format=0;size_t tlen=strlen(tn);if(tlen>31)tlen=31;scp4(s->segment_data.value_symbolic,tn,32);
    e->segment_count++;return 1;}
/* ---- EPATH Serialize ---- */
uint16_t cip_epath_serialize(const cip_epath_t*e,uint8_t*buf,uint16_t bsz){
    if(!e||!buf||!bsz)return 0;uint16_t pos=0;
    for(uint16_t i=0;i<e->segment_count;i++){const cip_epath_segment_t*s=&e->segments[i];
        if(s->segment_type&0x80){/*symbolic*/if(pos+2+(uint16_t)strlen(s->segment_data.value_symbolic)>bsz)break;
            buf[pos++]=s->segment_type;buf[pos++]=(uint8_t)strlen(s->segment_data.value_symbolic);
            memcpy(buf+pos,s->segment_data.value_symbolic,strlen(s->segment_data.value_symbolic));
            if(strlen(s->segment_data.value_symbolic)%2)buf[pos+strlen(s->segment_data.value_symbolic)]=0;/*pad*/
            pos+=(uint16_t)strlen(s->segment_data.value_symbolic);}
        else{/*logical segment*/if(pos+4>bsz)break;buf[pos++]=s->segment_type|s->segment_format;
            switch(s->segment_format){case 0:buf[pos++]=(uint8_t)s->segment_data.value_8;pos++;break;/*pad*/
                case 1:buf[pos++]=(uint8_t)(s->segment_data.value_16&0xFF);buf[pos++]=(uint8_t)(s->segment_data.value_16>>8);break;
                case 2:buf[pos++]=(uint8_t)(s->segment_data.value_32&0xFF);buf[pos++]=0;buf[pos++]=0;break;}}}
    /* Pad to 16-bit boundary */if(pos%2)pos++;return pos;}
/* ---- Build Tag Read Request ---- */
bool cip_build_tag_read_request(const char*tn,uint16_t ec,uint8_t dtc,cip_request_t*r){
    if(!tn||!r)return 0;memset(r,0,sizeof(cip_request_t));r->service_code=CIP_SERVICE_READ_TAG;
    cip_epath_init(&r->request_path);cip_epath_add_symbol(&r->request_path,tn);
    r->request_data[0]=(uint8_t)(ec&0xFF);r->request_data[1]=(uint8_t)((ec>>8)&0xFF);
    r->request_data[2]=dtc;r->request_data_len=3;return 1;}
/* ---- Parse Tag Read Response ---- */
cip_status_code_t cip_parse_tag_read_response(const cip_response_t*r,uint8_t*data,uint16_t dsz,uint16_t*asz){
    if(!r){if(asz)*asz=0;return CIP_STATUS_INVALID_PARAMETER;}
    if(r->general_status!=0){if(asz)*asz=0;return(cip_status_code_t)r->general_status;}
    uint16_t dlen=(uint16_t)(r->response_data[1]|(r->response_data[2]<<8));
    if(!data||!dsz){if(asz)*asz=dlen;return CIP_STATUS_SUCCESS;}
    uint16_t cp=dlen<dsz?dlen:dsz;memcpy(data,r->response_data+3,cp);if(asz)*asz=cp;return CIP_STATUS_SUCCESS;}
/* ---- Build Tag Write Request ---- */
bool cip_build_tag_write_request(const char*tn,uint8_t dtc,const uint8_t*data,uint16_t dsz,cip_request_t*r){
    if(!tn||!data||!r)return 0;memset(r,0,sizeof(cip_request_t));r->service_code=CIP_SERVICE_WRITE_TAG;
    cip_epath_init(&r->request_path);cip_epath_add_symbol(&r->request_path,tn);
    r->request_data[0]=dtc;r->request_data[1]=(uint8_t)(1&0xFF);r->request_data[2]=(uint8_t)(dsz&0xFF);r->request_data[3]=(uint8_t)((dsz>>8)&0xFF);
    if(dsz<CIP_MAX_MESSAGE_SIZE-4){memcpy(r->request_data+4,data,dsz);r->request_data_len=4+dsz;}return 1;}
/* ---- CIP Request Serialization ---- */
uint16_t cip_request_serialize(const cip_request_t*r,uint8_t*buf,uint16_t bsz){
    if(!r||!buf||!bsz||bsz<6)return 0;uint16_t pos=0;
    buf[pos++]=r->service_code;uint16_t epath_len=cip_epath_serialize(&r->request_path,buf+pos,bsz-pos-2);
    if(!epath_len)return 0;buf[pos-1]|=(uint8_t)((epath_len/2)&0xFF);/*path size in words*/
    pos+=epath_len;if(r->request_data_len>0&&pos+r->request_data_len<bsz){memcpy(buf+pos,r->request_data,r->request_data_len);pos+=r->request_data_len;}
    return pos;}
/* ---- CIP Response Parsing ---- */
bool cip_response_parse(const uint8_t*buf,uint16_t bsz,cip_response_t*r){
    if(!buf||!r||bsz<4)return 0;memset(r,0,sizeof(cip_response_t));
    r->service_code=buf[0];r->reserved=buf[1];r->general_status=buf[2];r->extended_status_size=buf[3];
    uint16_t pos=4;if(r->extended_status_size>0&&r->extended_status_size<=4){for(uint8_t i=0;i<r->extended_status_size&&pos+2<=bsz;i++){r->extended_status[i]=(uint16_t)(buf[pos]|(buf[pos+1]<<8));pos+=2;}}
    if(pos<bsz){uint16_t rlen=bsz-pos;if(rlen>CIP_MAX_MESSAGE_SIZE)rlen=CIP_MAX_MESSAGE_SIZE;memcpy(r->response_data,buf+pos,rlen);r->response_data_len=rlen;}
    return 1;}
/* ---- Status Code to String ---- */
const char* cip_status_to_string(cip_status_code_t s){
    switch(s){case CIP_STATUS_SUCCESS:return"Success";case CIP_STATUS_CONNECTION_FAILURE:return"Connection failure";
        case CIP_STATUS_RESOURCE_UNAVAILABLE:return"Resource unavailable";case CIP_STATUS_INVALID_PARAMETER:return"Invalid parameter";
        case CIP_STATUS_PATH_SEGMENT_ERROR:return"Path segment error";case CIP_STATUS_PATH_DEST_UNKNOWN:return"Path destination unknown";
        case CIP_STATUS_SERVICE_NOT_SUPPORTED:return"Service not supported";case CIP_STATUS_INVALID_ATTRIBUTE:return"Invalid attribute";
        case CIP_STATUS_OBJECT_STATE_CONFLICT:return"Object state conflict";case CIP_STATUS_OBJECT_ALREADY_EXISTS:return"Object already exists";
        case CIP_STATUS_ATTRIBUTE_NOT_SETTABLE:return"Attribute not settable";case CIP_STATUS_PRIVILEGE_VIOLATION:return"Privilege violation";
        case CIP_STATUS_DEVICE_STATE_CONFLICT:return"Device state conflict";case CIP_STATUS_NOT_ENOUGH_DATA:return"Not enough data";
        case CIP_STATUS_ATTRIBUTE_NOT_SUPPORTED:return"Attribute not supported";case CIP_STATUS_TOO_MUCH_DATA:return"Too much data";
        case CIP_STATUS_OBJECT_DOES_NOT_EXIST:return"Object does not exist";default:return"Unknown error";}}
'''
w("src/plc_rockwell_cip.c", cip)

print("\n=== All source files written successfully ===")
