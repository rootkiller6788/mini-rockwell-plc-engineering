/**
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

/* =================================================================
 * Extended CIP Operations
 * ================================================================= */

/* Lookup CIP data type code from Rockwell type code */
uint8_t cip_get_data_type_code(uint16_t rockwell_tc){
    switch(rockwell_tc){case 0:return 0xC1;case 1:return 0xC2;case 2:return 0xC3;
        case 3:return 0xC4;case 4:return 0xC5;case 5:return 0xCA;case 6:return 0xCB;
        case 8:return 0xC8;case 9:return 0xC9;case 11:return 0xDA;default:return 0xC4;}}

/* Get size in bytes for a CIP data type code */
uint16_t cip_get_type_size(uint8_t dtc){
    switch(dtc){case 0xC1:return 1;case 0xC2:return 1;case 0xC3:return 2;case 0xC4:return 4;
        case 0xC5:return 8;case 0xC6:return 1;case 0xC7:return 2;case 0xC8:return 4;
        case 0xC9:return 8;case 0xCA:return 4;case 0xCB:return 8;case 0xCC:return 8;
        case 0xCD:return 8;case 0xDA:return 80;case 0xDB:return 80;default:return 4;}}

/* Build CIP Multiple Service Packet (bundles multiple requests) */
bool cip_build_multiple_service_request(cip_request_t**reqs,uint16_t req_count,cip_request_t*out){
    if(!reqs||!req_count||!out)return 0;memset(out,0,sizeof(cip_request_t));
    out->service_code=CIP_SERVICE_MULTIPLE_SERVICE;uint8_t*p=out->request_data;uint16_t pos=0;
    *(p+pos++)=(uint8_t)(req_count&0xFF);*(p+pos++)=(uint8_t)((req_count>>8)&0xFF);/*count*/
    for(uint16_t i=0;i<req_count;i++){if(!reqs[i])continue;
        uint16_t rsz=cip_request_serialize(reqs[i],p+pos+2,CIP_MAX_MESSAGE_SIZE-pos-2);
        *(p+pos)=0x0A;/*service offset*/ *(p+pos+1)=(uint8_t)rsz;pos+=2+rsz;}
    out->request_data_len=pos;return 1;}

/* Parse CIP Read-Modify-Write response */
cip_status_code_t cip_parse_rmw_response(const cip_response_t*r,uint8_t*old_data,uint16_t old_sz){
    if(!r||!old_data||!old_sz)return CIP_STATUS_INVALID_PARAMETER;
    if(r->general_status!=0)return(cip_status_code_t)r->general_status;
    uint16_t cp=r->response_data_len<old_sz?r->response_data_len:old_sz;memcpy(old_data,r->response_data,cp);return CIP_STATUS_SUCCESS;}

/* Validate a CIP electronic path */
bool cip_epath_validate(const cip_epath_t*e){if(!e||!e->segment_count)return 0;
    for(uint16_t i=0;i<e->segment_count;i++){uint8_t st=e->segments[i].segment_type;
        if(st!=0x20&&st!=0x24&&st!=0x30&&!(st&0x80)&&!(st&0x40))return 0;}return 1;}

/* Convert CIP connection transport class to string */
const char* cip_transport_class_to_string(cip_transport_class_t tc){
    switch(tc){case CIP_TRANSPORT_CLASS_0:return"Class 0 (pure I/O)";
        case CIP_TRANSPORT_CLASS_1:return"Class 1 (I/O with data)";
        case CIP_TRANSPORT_CLASS_2:return"Class 2 (explicit TCP)";
        case CIP_TRANSPORT_CLASS_3:return"Class 3 (connected explicit)";
        default:return"Unknown";}}

/* Check if connection is healthy */
bool cip_connection_is_healthy(const cip_connection_t*c,uint32_t now_ms){
    if(!c)return 0;return(c->state==CIP_CONN_STATE_ESTABLISHED&&(now_ms-c->last_packet_time_ms)<c->watchdog_timeout);}

/* Get connection statistics */
void cip_connection_get_stats(const cip_connection_t*c,uint64_t*packets_sent,uint64_t*packets_rcvd,uint32_t*timeouts){
    if(!c){if(packets_sent)*packets_sent=0;if(packets_rcvd)*packets_rcvd=0;if(timeouts)*timeouts=0;return;}
    if(packets_sent)*packets_sent=c->packets_sent;if(packets_rcvd)*packets_rcvd=c->packets_received;if(timeouts)*timeouts=c->timeout_count;}

/* Reset connection statistics */
void cip_connection_reset_stats(cip_connection_t*c){if(!c)return;c->packets_sent=0;c->packets_received=0;c->timeout_count=0;c->last_packet_time_ms=0;}

/* Get ODVA vendor name from vendor ID */
const char* cip_get_vendor_name(uint16_t vid){
    switch(vid){case 1:return"Rockwell Automation";case 2:return"Schneider Electric";
        case 3:return"Omron";case 4:return"Siemens";case 5:return"ABB";
        case 50:return"Phoenix Contact";case 51:return"WAGO";
        case 100:return"Balluff";case 101:return"ifm electronic";
        case 256:return"Mitsubishi Electric";case 512:return"Beckhoff Automation";
        case 513:return"Bosch Rexroth";default:return"Unknown vendor";}}

/* Compute a simple CRC-16 (used by some CIP transports) */
static uint16_t crc16_update(uint16_t crc,uint8_t byte){
    crc^=(uint16_t)byte<<8;for(int i=0;i<8;i++){if(crc&0x8000)crc=(crc<<1)^0x1021;else crc<<=1;}return crc;}
uint16_t cip_crc16(const uint8_t*data,uint16_t len){
    uint16_t crc=0;if(!data)return 0;for(uint16_t i=0;i<len;i++)crc=crc16_update(crc,data[i]);return crc;}

/* Build a Forward Open service request */
bool cip_build_forward_open_request(const cip_forward_open_params_t*p,cip_request_t*r){
    if(!p||!r)return 0;memset(r,0,sizeof(cip_request_t));r->service_code=CIP_SERVICE_FORWARD_OPEN;
    cip_epath_init(&r->request_path);cip_epath_add_class(&r->request_path,CIP_CLASS_CONNECTION_MANAGER);cip_epath_add_instance(&r->request_path,1);
    uint8_t*d=r->request_data;d[0]=p->priority;d[1]=p->tick_time;d[2]=p->timeout_ticks;
    d[3]=(uint8_t)(p->o_to_t_network_conn_id&0xFF);d[4]=(uint8_t)((p->o_to_t_network_conn_id>>8)&0xFF);
    d[5]=(uint8_t)((p->o_to_t_network_conn_id>>16)&0xFF);d[6]=(uint8_t)((p->o_to_t_network_conn_id>>24)&0xFF);
    r->request_data_len=28+6;return 1;}

/* Build a Forward Close service request */
bool cip_build_forward_close_request(uint32_t connection_id,uint8_t priority,cip_request_t*r){
    if(!r)return 0;memset(r,0,sizeof(cip_request_t));r->service_code=CIP_SERVICE_FORWARD_CLOSE;
    cip_epath_init(&r->request_path);cip_epath_add_class(&r->request_path,CIP_CLASS_CONNECTION_MANAGER);cip_epath_add_instance(&r->request_path,1);
    uint8_t*d=r->request_data;d[0]=priority;d[1]=(uint8_t)(connection_id&0xFF);d[2]=(uint8_t)((connection_id>>8)&0xFF);
    d[3]=(uint8_t)((connection_id>>16)&0xFF);d[4]=(uint8_t)((connection_id>>24)&0xFF);r->request_data_len=6;return 1;}
