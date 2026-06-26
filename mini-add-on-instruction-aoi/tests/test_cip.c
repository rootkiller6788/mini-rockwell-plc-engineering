#include "plc_rockwell_cip.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
static int tr=0,tp=0;
void tlog(const char*n,int ok){tr++;if(ok){printf("  PASS: %s\n",n);tp++;}else printf("  FAIL: %s\n",n);}
int main(void){printf("=== CIP Protocol Tests ===\n");
    cip_identity_t*id=cip_identity_create(1,14,100,"1756-L85E");
    tlog("Identity create",id&&id->vendor_id==1);
    cip_request_t req;
    tlog("Build identity req",cip_build_identity_request(&req)&&req.service_code==0x01);
    uint8_t resp_data[32]={1,0,14,0,100,0,1,0,0,0,0xEF,0xBE,0xAD,0xDE,'1','7','5','6','-','L','8','5','E',0,0,0,0,0,0,0,0,0};
    cip_response_t resp2;resp2.general_status=0;resp2.response_data_len=32;
    memcpy(resp2.response_data,resp_data,32);
    cip_identity_t parsed;memset(&parsed,0,sizeof(parsed));
    tlog("Parse identity resp",cip_parse_identity_response(&resp2,&parsed)&&parsed.vendor_id==1&&parsed.device_type==14);
    cip_epath_t ep;cip_epath_init(&ep);
    tlog("EPATH add class",cip_epath_add_class(&ep,0x01)&&ep.segment_count==1);
    tlog("EPATH add inst",cip_epath_add_instance(&ep,1)&&ep.segment_count==2);
    tlog("EPATH add attr",cip_epath_add_attribute(&ep,3)&&ep.segment_count==3);
    uint8_t ebuf[128];uint16_t elen=cip_epath_serialize(&ep,ebuf,128);
    tlog("EPATH serialize",elen>0);
    cip_connection_manager_t cm;cip_conn_mgr_init(&cm);
    tlog("Conn mgr init",cm.connection_count==0);
    cip_forward_open_params_t fop;memset(&fop,0,sizeof(fop));
    fop.o_to_t_rpi_ns=10000000;fop.o_to_t_size=8;fop.t_to_o_size=8;
    cip_connection_t*conn=0;
    tlog("Forward open",cip_conn_mgr_forward_open(&cm,&fop,&conn)==CIP_STATUS_SUCCESS&&conn);
    tlog("Find conn",cip_conn_mgr_find(&cm,conn->connection_id)==conn);
    tlog("Forward close",cip_conn_mgr_forward_close(&cm,conn->connection_id)==CIP_STATUS_SUCCESS);
    tlog("Tag read req",cip_build_tag_read_request("MyTag",1,0xC4,&req)&&req.service_code==0x4C);
    uint8_t wdata[4]={42,0,0,0};
    tlog("Tag write req",cip_build_tag_write_request("MyTag",0xC4,wdata,4,&req));
    uint8_t sbuf[256];uint16_t slen=cip_request_serialize(&req,sbuf,256);
    tlog("Req serialize",slen>0);
    cip_response_t resp;memset(&resp,0,sizeof(resp));
    tlog("Resp parse",cip_response_parse(sbuf,slen,&resp));
    tlog("Status string",strcmp(cip_status_to_string(0),"Success")==0);
    tlog("EPATH add symbol",cip_epath_add_symbol(&ep,"TestTag")&&ep.segment_count==4);
    cip_identity_free(id);
    printf("\n=== %d/%d tests passed ===\n",tp,tr);
    return(tp==tr)?0:1;}
