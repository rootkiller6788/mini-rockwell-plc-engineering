/**
 * cip_services.h - CIP Service Codes & Service Execution
 * Knowledge Domain: L1 Definitions, L2 Core Concepts
 * Reference: ODVA CIP Vol 1, Appendix A: Common Services
 *
 * Concepts: CIP service code registry, service path requirements,
 *           Get/Set Attribute Single execution flow, service validation
 * School: RWTH Aachen, ISA/IEC 61131-3 Communication FBs
 */
#ifndef CIP_SERVICES_H
#define CIP_SERVICES_H
#include <stdint.h>
#include "ethernet_ip_cip.h"

typedef struct {
    uint8_t  service_code;
    const char *name;
    const char *description;
    uint8_t  requires_path;
    uint8_t  min_path_depth;
} cip_service_def_t;

typedef struct {
    uint8_t  service;
    uint8_t  class_id;
    uint8_t  instance_id;
    uint8_t  attribute_id;
    uint8_t  data[256];
    uint16_t data_size;
} cip_service_data_t;

int cip_service_get_code_by_name(const char *name, uint8_t *code_out);
const char *cip_service_get_name(uint8_t service_code);
const char *cip_service_get_description(uint8_t service_code);
int cip_service_is_path_required(uint8_t service_code);
int cip_service_execute_get_attribute_single(cip_service_data_t *svc, uint8_t *response, uint16_t *resp_size);
int cip_service_execute_set_attribute_single(cip_service_data_t *svc);
int cip_service_execute_reset(uint8_t reset_type);
int cip_service_execute_get_attribute_all(cip_service_data_t *svc, uint8_t *response, uint16_t *resp_size);
int cip_service_validate_service_code(uint8_t service_code);
const char *cip_service_status_to_string(uint8_t status_code);
#endif /* CIP_SERVICES_H */
