#include "plc_rockwell_tag.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
static int tr=0,tp=0;
void tlog(const char*n,int ok){tr++;if(ok){printf("  PASS: %s\n",n);tp++;}else printf("  FAIL: %s\n",n);}
int main(void){printf("=== Tag System Tests ===\n");
    plc_tag_database_t db;plc_tag_database_init(&db,65536);
    tlog("Init",db.tag_count==0&&db.memory_pool_size==65536);
    plc_tag_t*t=plc_tag_database_create_tag(&db,"MyDINT",TAG_SCOPE_CONTROLLER,PLC_TYPE_ATOMIC,3,0);
    tlog("Create tag",t&&db.tag_count==1);
    plc_tag_t*t2=plc_tag_database_create_tag(&db,"ProgTag",TAG_SCOPE_PROGRAM,PLC_TYPE_ATOMIC,3,"Main");
    tlog("Create program tag",t2!=0);
    tlog("Dup name",!plc_tag_database_create_tag(&db,"MyDINT",TAG_SCOPE_CONTROLLER,PLC_TYPE_ATOMIC,3,0));
    plc_tag_search_result_t r;plc_tag_resolve(&db,"MyDINT","Main",&r);
    tlog("Resolve global",r.found&&!r.is_program_scoped);
    plc_tag_resolve(&db,"ProgTag","Main",&r);
    tlog("Resolve program",r.found&&r.is_program_scoped);
    plc_tag_resolve(&db,"NoTag","Main",&r);
    tlog("Resolve missing",!r.found);
    tlog("Type size DINT",plc_type_atomic_size(3)==4);
    tlog("Type size BOOL",plc_type_atomic_size(0)==4);
    tlog("Type size REAL",plc_type_atomic_size(5)==4);
    plc_udt_t*u=plc_udt_create(&db,"MotorUDT","Motor data");
    tlog("UDT create",u&&db.udt_count==1);
    tlog("UDT add member",plc_udt_add_member(u,"Speed",PLC_TYPE_ATOMIC,5)&&plc_udt_add_member(u,"Status",PLC_TYPE_ATOMIC,0)&&u->member_count==2);
    tlog("UDT find",plc_udt_find(&db,"MotorUDT")==u);
    tlog("UDT not found",!plc_udt_find(&db,"NoUDT"));
    uint32_t sz=plc_udt_compute_byte_size(u);
    tlog("UDT byte size",sz>=8);
    tlog("Delete tag",plc_tag_database_delete_tag(&db,"ProgTag")&&db.tag_count==1);
    char buf[1024];uint32_t n=plc_tag_database_export_l5k(&db,buf,1024);
    tlog("L5K export",n>0);
    printf("\n=== %d/%d tests passed ===\n",tp,tr);
    return(tp==tr)?0:1;}
