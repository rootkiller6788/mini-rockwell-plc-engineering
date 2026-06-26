/** @file example_aoi_library.c
 *  @brief L6: AOI Library management and signature-based version control
 *  Knowledge: L1-L6, library versioning, signature integrity, CSP */
#include "plc_rockwell_aoi.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
int main(void){printf("=== AOI Library with Signature Verification ===\n");
    aoi_library_t*lib=aoi_library_create("ChemicalPlant","1756-L85E",36,11);assert(lib);
    /* Register 3 AOIs */
    aoi_definition_t*aoi1=aoi_definition_create("ValveCtrl","Valve Open/Close");assert(aoi1);
    aoi_definition_add_parameter(aoi1,"OpenCmd",0,0,1);aoi_definition_add_parameter(aoi1,"ClosedLS",0,0,1);
    aoi_definition_add_parameter(aoi1,"ValvePos",1,0,0);
    aoi_library_add_aoi(lib,aoi1);aoi_definition_free(aoi1);
    aoi_definition_t*aoi2=aoi_definition_create("PumpCtrl","Pump Start/Stop");assert(aoi2);
    aoi_definition_add_parameter(aoi2,"Start",0,0,1);aoi_definition_add_parameter(aoi2,"Running",0,0,0);
    aoi_definition_add_parameter(aoi2,"Fault",1,0,0);aoi_library_add_aoi(lib,aoi2);aoi_definition_free(aoi2);
    aoi_definition_t*aoi3=aoi_definition_create("TempLoop","Temperature PID");assert(aoi3);
    aoi_definition_add_parameter(aoi3,"SP",0,5,1);aoi_definition_add_parameter(aoi3,"PV",0,5,1);
    aoi_definition_add_parameter(aoi3,"CV",1,5,0);aoi_library_add_aoi(lib,aoi3);aoi_definition_free(aoi3);
    assert(aoi_library_count(lib)==3);
    printf("Library has %d AOIs\n",aoi_library_count(lib));
    /* Compute project-level signature */
    aoi_library_compute_signature(lib);
    printf("Library signature: %02x%02x%02x%02x%02x...\n",
        lib->library_signature.digest[0],lib->library_signature.digest[1],
        lib->library_signature.digest[2],lib->library_signature.digest[3],
        lib->library_signature.digest[4]);
    /* Verify each AOI */
    aoi_definition_t*f=aoi_library_find_aoi(lib,"TempLoop");assert(f);
    printf("Found AOI: %s (rev %d.%d)\n",f->name,f->revision.major_version,f->revision.minor_version);
    /* Context-sensitive parameter demonstration */
    aoi_context_param_t cp;memset(&cp,0,sizeof(cp));cp.context=AOI_CTX_SAFETY_TASK;cp.param_index=0;
    cp.override_required=1;cp.new_required=1;
    aoi_definition_add_context_override(f,&cp);
    bool rq,vs;aoi_definition_resolve_context(f,0,AOI_CTX_SAFETY_TASK,&rq,&vs);
    printf("CSP: SP.Required in safety=%s, in standard=",rq?"true":"false");
    aoi_definition_resolve_context(f,0,AOI_CTX_STANDARD_TASK,&rq,&vs);
    printf("%s\n",rq?"true":"false");
    /* Signature verification */
    aoi_signature_t s1,s2;aoi_compute_signature(f,&s1);
    aoi_definition_add_local_tag(f,"NewTag",AOI_TYPE_DINT,0);aoi_compute_signature(f,&s2);
    printf("Signatures equal after edit? %s (expected: false)\n",aoi_signature_equal(&s1,&s2)?"true":"false");
    printf("=== AOI Library Example Complete ===\n");aoi_library_free(lib);return 0;}
