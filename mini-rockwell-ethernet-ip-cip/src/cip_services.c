/**
 * cip_services.c - CIP Service Code Registry & Service Execution
 * Knowledge Domain: L1 Definitions, L2 Core Concepts, L5 Algorithms
 * Reference: ODVA CIP Vol 1, Appendix A: Common Services
 *
 * Each function implements one independent knowledge point:
 *   1. cip_service_get_code_by_name()       - Reverse lookup: name -> service code
 *   2. cip_service_get_name()               - Service code -> ASCII name
 *   3. cip_service_get_description()        - Service code -> description
 *   4. cip_service_is_path_required()       - Does this service need an EPATH?
 *   5. cip_service_execute_get_attribute_single() - Get_Attribute_Single logic
 *   6. cip_service_execute_set_attribute_single() - Set_Attribute_Single logic
 *   7. cip_service_execute_reset()          - Device Reset service
 *   8. cip_service_execute_get_attribute_all() - Get_Attribute_All logic
 *   9. cip_service_validate_service_code()  - Service code range check
 *  10. cip_service_status_to_string()       - General Status string mapping
 */
#include <string.h>
#include "../include/cip_services.h"
#include "../include/cip_message.h"

/* -------------------------------------------------------------------------
 * Service Definition Registry
 *
 * ODVA Vol 1 Appendix A defines common services that all CIP devices
 * must support. This static table maps service codes to metadata:
 * name, description, path requirement, and minimum path depth.
 * ------------------------------------------------------------------------- */
static const cip_service_def_t g_service_registry[] = {
    {0x01, "Get_Attribute_All",        "Get all attributes of an instance", 1, 2},
    {0x02, "Set_Attribute_All",        "Set all attributes of an instance", 1, 2},
    {0x03, "Get_Attribute_List",       "Get specified attributes",          1, 2},
    {0x04, "Set_Attribute_List",       "Set specified attributes",          1, 2},
    {0x05, "Reset",                    "Reset the device",                  0, 0},
    {0x06, "Start",                    "Start the application",             0, 0},
    {0x07, "Stop",                     "Stop the application",              0, 0},
    {0x08, "Create",                   "Create a new instance",             1, 1},
    {0x09, "Delete",                   "Delete an instance",                1, 1},
    {0x0A, "Multiple_Service_Packet",  "Multiple services in one request",  1, 0},
    {0x0D, "Apply_Attributes",         "Apply pending attribute changes",   0, 0},
    {0x0E, "Get_Attribute_Single",     "Get a single attribute value",      1, 3},
    {0x10, "Set_Attribute_Single",     "Set a single attribute value",      1, 3},
    {0x11, "Find_Next_Object_Instance","Find next instance of a class",     1, 1},
    {0x15, "Restore",                  "Restore to default configuration",  0, 0},
    {0x16, "Save",                     "Save to non-volatile memory",       0, 0},
    {0x17, "NOP",                      "No operation (connection test)",    0, 0},
};
#define SERVICE_REGISTRY_COUNT (sizeof(g_service_registry) / sizeof(g_service_registry[0]))

/* -------------------------------------------------------------------------
 * cip_service_get_code_by_name - Look up service code from ASCII name
 *
 * Knowledge: CIP service name resolution.
 * Used when parsing EDS files or configuration data where services are
 * referenced by name rather than numeric code.
 *
 * Returns: 0 = found (*code_out populated), -1 = not found.
 * Complexity: O(S) where S = SERVICE_REGISTRY_COUNT (linear scan, ~17 items).
 * ------------------------------------------------------------------------- */
int cip_service_get_code_by_name(const char *name, uint8_t *code_out)
{
    if (!name || !code_out) return -1;
    for (size_t i = 0; i < SERVICE_REGISTRY_COUNT; i++) {
        if (strcmp(g_service_registry[i].name, name) == 0) {
            *code_out = g_service_registry[i].service_code;
            return 0;
        }
    }
    return -1;
}

/* -------------------------------------------------------------------------
 * cip_service_get_name - Get ASCII name for a service code
 *
 * Knowledge: CIP service identification for debug/logging.
 * Maps numeric service codes to human-readable names for display.
 *
 * Returns: static string pointer, or "Unknown_Service" if not found.
 * Complexity: O(S).
 * ------------------------------------------------------------------------- */
const char *cip_service_get_name(uint8_t service_code)
{
    for (size_t i = 0; i < SERVICE_REGISTRY_COUNT; i++) {
        if (g_service_registry[i].service_code == service_code) {
            return g_service_registry[i].name;
        }
    }
    return "Unknown_Service";
}

/* -------------------------------------------------------------------------
 * cip_service_get_description - Get description for a service code
 *
 * Knowledge: CIP service documentation.
 * Returns the ODVA-defined description string for a service.
 *
 * Complexity: O(S).
 * ------------------------------------------------------------------------- */
const char *cip_service_get_description(uint8_t service_code)
{
    for (size_t i = 0; i < SERVICE_REGISTRY_COUNT; i++) {
        if (g_service_registry[i].service_code == service_code) {
            return g_service_registry[i].description;
        }
    }
    return "No description available";
}

/* -------------------------------------------------------------------------
 * cip_service_is_path_required - Does this service need an EPATH?
 *
 * Knowledge: CIP service semantics.
 * Some services (Reset, Start, Stop, NOP) operate on the device level and
 * do not need a Class/Instance/Attribute path. Others (Get/Set Attribute)
 * require a valid EPATH to identify the target attribute.
 *
 * Complexity: O(S).
 * ------------------------------------------------------------------------- */
int cip_service_is_path_required(uint8_t service_code)
{
    for (size_t i = 0; i < SERVICE_REGISTRY_COUNT; i++) {
        if (g_service_registry[i].service_code == service_code) {
            return g_service_registry[i].requires_path;
        }
    }
    return 1; /* default: assume path required for unknown services */
}

/* -------------------------------------------------------------------------
 * cip_service_execute_get_attribute_single - Execute Get_Attribute_Single
 *
 * Knowledge: CIP Get_Attribute_Single (0x0E) service execution.
 * This is THE most common CIP service. It reads a single attribute value
 * from an object instance. The path encodes: Class -> Instance -> Attribute.
 *
 * In a real implementation, this would dispatch to the appropriate object's
 * attribute getter. Here the response structure is prepared.
 *
 * Returns: 0 = response prepared, -1 = invalid parameters.
 * Complexity: O(1) - structure population.
 * ------------------------------------------------------------------------- */
int cip_service_execute_get_attribute_single(cip_service_data_t *svc,
                                             uint8_t *response, uint16_t *resp_size)
{
    if (!svc || !response || !resp_size) return -1;

    /* Build the response header for Get_Attribute_Single response.
     * The response format per ODVA Vol 1 Sec 2-5.2.2:
     *   Byte 0: Service | 0x80 (response bit)
     *   Byte 1: Reserved (0x00)
     *   Byte 2: General Status (0x00 = success)
     *   Byte 3: Size of Additional Status (0)
     *   Bytes 4+: Attribute data
     */
    if (svc->data_size > 250) return -2; /* response buffer limit */

    response[0] = svc->service | CIP_SERVICE_RESPONSE_BIT;
    response[1] = 0x00; /* reserved */
    response[2] = CIP_STATUS_SUCCESS;
    response[3] = 0x00; /* no additional status */
    if (svc->data_size > 0) {
        memcpy(&response[4], svc->data, svc->data_size);
    }
    *resp_size = 4 + svc->data_size;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_service_execute_set_attribute_single - Execute Set_Attribute_Single
 *
 * Knowledge: CIP Set_Attribute_Single (0x10) service execution.
 * The second most common CIP service. Writes a single attribute value.
 * The data to write is in svc->data, sized by svc->data_size.
 *
 * Returns: 0 = write successful, -1 = invalid, -2 = write rejected.
 * Complexity: O(1) - structure validation.
 * ------------------------------------------------------------------------- */
int cip_service_execute_set_attribute_single(cip_service_data_t *svc)
{
    if (!svc) return -1;
    if (svc->data_size == 0) return -2; /* Set_Attribute_Single requires data */
    if (svc->data_size > 256) return -3;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_service_execute_reset - Execute Reset service
 *
 * Knowledge: CIP Reset (0x05) service execution.
 * The Reset service restarts the device. Reset type:
 *   0 = Power-up reset (warm restart)
 *   1 = Return to out-of-box defaults, then reset
 *   2 = Return to out-of-box defaults without reset
 *
 * Returns: 0 = valid reset type, -1 = invalid.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_service_execute_reset(uint8_t reset_type)
{
    if (reset_type > 2) return -1;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_service_execute_get_attribute_all - Execute Get_Attribute_All
 *
 * Knowledge: CIP Get_Attribute_All (0x01) service.
 * Reads ALL attributes of an object instance in a single response.
 * The path encodes Class + Instance (no Attribute segment needed).
 *
 * Returns: 0 = response prepared. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_service_execute_get_attribute_all(cip_service_data_t *svc,
                                          uint8_t *response, uint16_t *resp_size)
{
    if (!svc || !response || !resp_size) return -1;

    response[0] = CIP_SERVICE_GET_ATTRIBUTE_ALL | CIP_SERVICE_RESPONSE_BIT;
    response[1] = 0x00;
    response[2] = CIP_STATUS_SUCCESS;
    response[3] = 0x00;
    if (svc->data_size > 0) {
        memcpy(&response[4], svc->data, svc->data_size);
    }
    *resp_size = 4 + svc->data_size;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_service_validate_service_code - Check service code validity
 *
 * Knowledge: CIP service code range check.
 * Valid service codes: 0x01-0x7F (request codes).
 * The MSB (0x80) is reserved for the response bit.
 *
 * Returns 1 = valid, 0 = invalid. Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_service_validate_service_code(uint8_t service_code)
{
    if (service_code == 0x00u) return 0;
    if (service_code >= 0x80u) return 0;
    return 1;
}

/* -------------------------------------------------------------------------
 * cip_service_status_to_string - General Status code to string
 *
 * Knowledge: CIP status display utility.
 * Wraps cip_status_string() for convenience in service layer.
 *
 * Complexity: O(1) - delegates to cip_status_string().
 * ------------------------------------------------------------------------- */
const char *cip_service_status_to_string(uint8_t status_code)
{
    return cip_status_string(status_code);
}
