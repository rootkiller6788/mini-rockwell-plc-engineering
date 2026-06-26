/**
 * @file plc_rockwell_aoi.c
 * @brief Add-On Instruction Core Implementation (Rockwell Studio 5000/RSLogix 5000)
 * Knowledge Coverage: L1 Definitions ¡¤ L2 Core Concepts ¡¤ L3 Engineering Structures
 *   L4 Engineering Laws (IEC 61508 safety AOI) ¡¤ L5 Algorithms (SHA-1, CSP)
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
