/**
 * @file plc_rockwell_tag.c
 * @brief Rockwell Tag System Implementation - Tag Database, UDTs, Memory Model
 * Knowledge: L1-L5. Ref: 1756-PM004, IEC 61131-3
 */
#include "plc_rockwell_tag.h"
#include "plc_rockwell_aoi.h"
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

/* =================================================================
 * Extended Tag Database Operations
 * ================================================================= */

/* Lookup tag by name and scope */
plc_tag_t* plc_tag_get_by_name(plc_tag_database_t*db,const char*n,plc_tag_scope_t sc){
    if(!db||!n)return 0;
    for(uint16_t i=0;i<db->tag_count;i++){plc_tag_t*t=&db->tags[i];
        if(!scmp2(t->name,n)&&t->scope==sc)return t;}
    return 0;}

/* Set tag display style (radix) */
bool plc_tag_set_style(plc_tag_t*t,plc_tag_style_t s){if(!t)return 0;t->style=s;return 1;}

/* Mark tag as produced (sent to consumers via CIP) */
bool plc_tag_set_produced(plc_tag_t*t,uint16_t max_consumers){
    if(!t)return 0;t->tag_class=TAG_CLASS_PRODUCED;t->external_access=TAG_EXTERNAL_READWRITE;return 1;}

/* Mark tag as consumed (received from producer) */
bool plc_tag_set_consumed(plc_tag_t*t,const char*producer_ip){
    if(!t)return 0;t->tag_class=TAG_CLASS_CONSUMED;t->external_access=TAG_EXTERNAL_READONLY;return 1;}

/* Set tag as safety (GuardLogix only) */
bool plc_tag_set_safety(plc_tag_t*t){if(!t)return 0;
    t->tag_class=TAG_CLASS_SAFETY;t->retain_on_powerup=1;return 1;}

/* Enable retain-on-powerup for a tag */
bool plc_tag_set_retain_on_powerup(plc_tag_t*t,bool en){if(!t)return 0;t->retain_on_powerup=en;return 1;}

/* Check tag memory size based on type and array dimensions */
uint32_t plc_tag_compute_total_size(const plc_tag_t*t){
    if(!t)return 0;uint32_t base=plc_type_atomic_size(t->type_code);if(!t->is_array)return base;
    uint32_t elems=1;for(uint8_t d=0;d<t->array_dims_count;d++)elems*=t->array_dim[d];return base*elems;}

/* Enumerate all tags of a given scope */
uint16_t plc_tag_enumerate_scope(const plc_tag_database_t*db,plc_tag_scope_t sc,plc_tag_t**out,uint16_t max){
    if(!db||!out||!max)return 0;uint16_t cnt=0;
    for(uint16_t i=0;i<db->tag_count&&cnt<max;i++){if(db->tags[i].scope==sc)out[cnt++]=&db->tags[i];}
    return cnt;}

/* Create an alias tag (points to another tag in memory) */
plc_tag_t* plc_tag_database_create_alias(plc_tag_database_t*db,const char*alias_name,const char*target_name){
    if(!db||!alias_name||!target_name)return 0;
    plc_tag_t*target=plc_tag_get_by_name(db,target_name,TAG_SCOPE_CONTROLLER);
    if(!target)target=plc_tag_get_by_name(db,target_name,TAG_SCOPE_PROGRAM);
    if(!target)return 0;
    plc_tag_t*alias=plc_tag_database_create_tag(db,alias_name,TAG_SCOPE_CONTROLLER,PLC_TYPE_ALIAS,target->type_code,0);
    if(!alias)return 0;alias->memory_offset=target->memory_offset;alias->memory_size=target->memory_size;return alias;}

/* Get the total memory used by all tags in the database */
uint32_t plc_tag_database_get_memory_usage(const plc_tag_database_t*db){return db?db->memory_used:0;}

/* Get the number of tags of each scope */
void plc_tag_database_scope_counts(const plc_tag_database_t*db,uint16_t*ctl,uint16_t*prog,uint16_t*loc){
    if(ctl)*ctl=0;if(prog)*prog=0;if(loc)*loc=0;if(!db)return;
    for(uint16_t i=0;i<db->tag_count;i++){switch(db->tags[i].scope){
        case TAG_SCOPE_CONTROLLER:if(ctl)(*ctl)++;break;
        case TAG_SCOPE_PROGRAM:if(prog)(*prog)++;break;
        case TAG_SCOPE_LOCAL:if(loc)(*loc)++;break;}}}

/* Find tags by type code */
uint16_t plc_tag_find_by_type(const plc_tag_database_t*db,uint16_t tc,plc_tag_t**out,uint16_t max){
    if(!db||!out||!max)return 0;uint16_t cnt=0;
    for(uint16_t i=0;i<db->tag_count&&cnt<max;i++){if(db->tags[i].type_code==tc)out[cnt++]=&db->tags[i];}
    return cnt;}

/* Check if a tag name is valid in Logix 5000 naming convention */
bool plc_tag_validate_name(const char*n){
    if(!n||!*n)return 0;size_t len=strlen(n);if(len>63||len<1)return 0;
    /* Must start with letter or underscore */
    if(!((*n>='A'&&*n<='Z')||(*n>='a'&&*n<='z')||*n=='_'))return 0;
    /* Valid chars: alphanumeric, underscore, but no spaces */
    for(size_t i=0;i<len;i++){char c=n[i];if(c==' ')return 0;
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'))return 0;}
    return 1;}

/* Get the IEC 61131-3 type name for a Rockwell type code */
const char* plc_tag_type_to_string(uint16_t tc){
    switch(tc){case 0:return"BOOL";case 1:return"SINT";case 2:return"INT";
        case 3:return"DINT";case 4:return"LINT";case 5:return"REAL";
        case 6:return"LREAL";case 8:return"DWORD";case 9:return"LWORD";
        case 10:return"COUNTER";case 11:return"TIMER";case 12:return"UDT";
        default:return"UNKNOWN";}
}

/* =================================================================
 * UDT Signature Computation & Extended UDT Operations
 * ================================================================= */

/* Compute a structural hash of a UDT for change detection */
uint32_t plc_udt_compute_signature_hash(const plc_udt_t*u){
    if(!u||!u->member_count)return 0;uint32_t h=5381;
    for(uint16_t i=0;i<u->member_count;i++){const plc_udt_member_t*m=&u->members[i];
        for(const char*s=m->name;*s;s++)h=((h<<5)+h)+*s;
        h=((h<<5)+h)+(uint8_t)m->type_class;h=((h<<5)+h)+m->type_code;}
    return h;}

/* Get total number of UDTs in the database */
uint16_t plc_udt_get_count(const plc_tag_database_t*db){return db?db->udt_count:0;}

/* Find all tags that use a specific UDT */
uint16_t plc_tag_find_by_udt(const plc_tag_database_t*db,const char*udt_name,plc_tag_t**out,uint16_t max){
    if(!db||!udt_name||!out||!max)return 0;uint16_t cnt=0;
    for(uint16_t i=0;i<db->tag_count&&cnt<max;i++){plc_tag_t*t=&db->tags[i];
        if(t->type_class==PLC_TYPE_UDT&&!strcmp(t->udt_type_name,udt_name))out[cnt++]=t;}return cnt;}

/* Get UDT member by name */
plc_udt_member_t* plc_udt_get_member(plc_udt_t*u,const char*n){
    if(!u||!n)return 0;for(uint16_t i=0;i<u->member_count;i++)if(!scmp2(u->members[i].name,n))return &u->members[i];return 0;}

/* Remove a member from a UDT (reorders array) */
bool plc_udt_remove_member(plc_udt_t*u,uint16_t idx){
    if(!u||idx>=u->member_count)return 0;u->member_count--;if(idx<u->member_count)u->members[idx]=u->members[u->member_count];
    /* Recompute byte offsets */u->total_byte_size=0;for(uint16_t i=0;i<u->member_count;i++){
        u->members[i].byte_offset=u->total_byte_size;u->total_byte_size+=u->members[i].byte_size;}return 1;}

/* Export all tags as a formatted table (for diagnostics) */
uint32_t plc_tag_database_export_table(const plc_tag_database_t*db,char*buf,uint32_t bsz){
    if(!db||!buf||!bsz)return 0;uint32_t pos=snprintf(buf,bsz,"%-20s %-12s %-10s %8s\n","Name","Scope","Type","Size");
    for(uint16_t i=0;i<db->tag_count&&pos+80<bsz;i++){plc_tag_t*t=&db->tags[i];
        pos+=snprintf(buf+pos,bsz-pos,"%-20s %-12s %-10s %8u\n",
            t->name,t->scope==TAG_SCOPE_CONTROLLER?"Global":(t->scope==TAG_SCOPE_PROGRAM?"Program":"Local"),
            plc_tag_type_to_string(t->type_code),t->memory_size);}
    return pos;}

/* Get UDT nesting depth (recursive check for nested UDTs) */
uint8_t plc_udt_get_nesting_depth(const plc_tag_database_t*db,const plc_udt_t*u){
    if(!db||!u)return 0;uint8_t max_depth=0;
    for(uint16_t i=0;i<u->member_count;i++){plc_udt_member_t*m=&u->members[i];
        if(m->type_class==PLC_TYPE_UDT&&m->nested_udt_name[0]){plc_udt_t*nested=plc_udt_find((plc_tag_database_t*)db,m->nested_udt_name);
            if(nested){uint8_t d=1+plc_udt_get_nesting_depth(db,nested);if(d>max_depth)max_depth=d;}}}
    return max_depth;}
