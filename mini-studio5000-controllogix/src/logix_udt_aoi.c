/**
 * @file    logix_udt_aoi.c
 * @brief   UDT and AOI Management Implementation
 * L1-L3
 * Reference: 1756-PM010
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "logix_udt_aoi.h"

/* ========================================================================
 * UDT Management
 * ======================================================================== */

bool logix_udt_create(logix_udt_t *udt, const char *name,
                       const char *description)
{
    if (!udt || !name) return false;

    memset(udt, 0, sizeof(*udt));
    strncpy(udt->name, name, 39);
    udt->name[39] = '\0';

    if (description) {
        strncpy(udt->description, description, 255);
        udt->description[255] = '\0';
    }

    return true;
}

bool logix_udt_add_member(logix_udt_t *udt, const char *name,
                            logix_atomic_type_t type,
                            uint32_t array_dim1,
                            uint32_t array_dim2,
                            uint32_t array_dim3)
{
    if (!udt || !name) return false;
    if (udt->member_count >= 200) return false;

    /* Check for duplicate member name */
    for (uint32_t i = 0; i < udt->member_count; i++) {
        if (strcmp(udt->members[i].name, name) == 0) return false;
    }

    logix_udt_member_t *member = &udt->members[udt->member_count];
    memset(member, 0, sizeof(*member));

    strncpy(member->name, name, 39);
    member->name[39] = '\0';
    member->type = type;
    member->is_required = false;
    member->is_visible = true;

    /* Set up array dimensions */
    if (array_dim1 > 0) {
        member->array_info.dim_size[0] = array_dim1;
        member->array_info.dim_count = 1;
        member->array_info.total_elements = array_dim1;

        if (array_dim2 > 0) {
            member->array_info.dim_size[1] = array_dim2;
            member->array_info.dim_count = 2;
            member->array_info.total_elements = array_dim1 * array_dim2;

            if (array_dim3 > 0) {
                member->array_info.dim_size[2] = array_dim3;
                member->array_info.dim_count = 3;
                member->array_info.total_elements = array_dim1 * array_dim2 * array_dim3;
            }
        }
    }

    uint32_t base_size = logix_type_size(type);
    member->size_bytes = base_size;
    member->array_info.element_size = base_size;

    /* Compute byte offset with proper alignment */
    uint32_t alignment = base_size;
    if (alignment > 8) alignment = 8;
    if (alignment == 0) alignment = 1;

    uint32_t current_offset = udt->total_size_bytes;
    uint32_t padding = (alignment - (current_offset % alignment)) % alignment;
    member->offset_bytes = current_offset + padding;

    /* Update UDT total size */
    uint32_t member_total_size = base_size;
    if (member->array_info.dim_count > 0) {
        member_total_size *= member->array_info.total_elements;
    }
    udt->total_size_bytes = member->offset_bytes + member_total_size;
    udt->member_count++;

    return true;
}

bool logix_udt_add_udt_member(logix_udt_t *udt, const char *name,
                                uint32_t member_udt_index)
{
    if (!udt || !name) return false;
    if (udt->member_count >= 200) return false;
    if (member_udt_index >= 200) return false;

    /* Essentially a struct-type member */
    logix_udt_member_t *member = &udt->members[udt->member_count];
    memset(member, 0, sizeof(*member));

    strncpy(member->name, name, 39);
    member->name[39] = '\0';
    member->type = LOGIX_TYPE_STRUCT;
    member->is_visible = true;

    /* Size is unknown at creation time — caller must recalculate */
    member->size_bytes = 0;
    member->offset_bytes = udt->total_size_bytes;

    udt->member_count++;
    return true;
}

/* ========================================================================
 * AOI Management
 * ======================================================================== */

bool logix_aoi_create(logix_aoi_t *aoi, const char *name,
                       const char *description)
{
    if (!aoi || !name) return false;

    memset(aoi, 0, sizeof(*aoi));
    strncpy(aoi->name, name, 39);
    aoi->name[39] = '\0';

    if (description) {
        strncpy(aoi->description, description, 255);
        aoi->description[255] = '\0';
    }

    aoi->major_revision = 1;
    aoi->minor_revision = 0;
    aoi->enable_in_required = false;

    return true;
}

bool logix_aoi_add_parameter(logix_aoi_t *aoi, const char *name,
                               aoi_param_direction_t direction,
                               logix_atomic_type_t data_type,
                               bool required, bool visible)
{
    if (!aoi || !name) return false;
    if (aoi->parameter_count >= 128) return false;

    /* Check duplicate parameter name */
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        if (strcmp(aoi->parameters[i].name, name) == 0) return false;
    }

    aoi_parameter_t *param = &aoi->parameters[aoi->parameter_count];
    memset(param, 0, sizeof(*param));

    strncpy(param->name, name, 39);
    param->name[39] = '\0';
    param->direction = direction;
    param->data_type = data_type;
    param->required = required;
    param->visible = visible;

    uint32_t size = logix_type_size(data_type);
    param->size_bytes = size;
    param->offset_bytes = aoi->context_size_bytes;

    uint32_t alignment = size;
    if (alignment > 8) alignment = 8;
    if (alignment == 0) alignment = 1;
    uint32_t padding = (alignment - (aoi->context_size_bytes % alignment)) % alignment;
    param->offset_bytes = aoi->context_size_bytes + padding;

    aoi->context_size_bytes = param->offset_bytes + size;
    aoi->parameter_count++;

    return true;
}

bool logix_aoi_add_routine(logix_aoi_t *aoi, const char *name,
                             logix_routine_language_t language,
                             bool is_enable_in_false,
                             bool is_prescan)
{
    if (!aoi || !name) return false;
    if (aoi->routine_count >= 4) return false;

    aoi_routine_t *routine = &aoi->routines[aoi->routine_count];
    memset(routine, 0, sizeof(*routine));

    strncpy(routine->name, name, 63);
    routine->name[63] = '\0';
    routine->language = language;
    routine->is_enable_in_false = is_enable_in_false;
    routine->is_prescan = is_prescan;

    aoi->routine_count++;
    return true;
}

uint32_t logix_aoi_validate(const logix_aoi_t *aoi)
{
    if (!aoi) return 1;
    uint32_t errors = 0;

    /* Name must be non-empty and valid */
    if (aoi->name[0] == '\0') errors++;

    /* At least one Output or InOut parameter */
    bool has_output = false;
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        if (aoi->parameters[i].direction == AOI_PARAM_OUTPUT ||
            aoi->parameters[i].direction == AOI_PARAM_INOUT) {
            has_output = true;
        }
    }
    if (!has_output) errors++;

    /* Required parameters cannot have default values set */
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        if (aoi->parameters[i].required &&
            aoi->parameters[i].default_value[0] != '\0') {
            errors++;
        }
    }

    /* Duplicate parameter names */
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        for (uint32_t j = i + 1; j < aoi->parameter_count; j++) {
            if (strcmp(aoi->parameters[i].name, aoi->parameters[j].name) == 0) {
                errors++;
            }
        }
    }

    return errors;
}

uint32_t logix_aoi_context_size(const logix_aoi_t *aoi)
{
    if (!aoi) return 0;
    return aoi->context_size_bytes;
}

bool logix_aoi_instantiate(const logix_aoi_t *aoi,
                             const char *instance_name,
                             aoi_instance_t *instance)
{
    if (!aoi || !instance_name || !instance) return false;
    if (aoi->context_size_bytes > 4096) return false;

    memset(instance, 0, sizeof(*instance));
    strncpy(instance->instance_name, instance_name, 39);
    instance->instance_name[39] = '\0';

    /* Initialize context data with defaults */
    /* In a real implementation, copy default values from parameter defs */
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        /* Default values would be copied into context_data at offset */
    }
    instance->context_size_used = aoi->context_size_bytes;

    return true;
}

void logix_aoi_generate_backing_udt(const logix_aoi_t *aoi,
                                      logix_udt_t *udt)
{
    if (!aoi || !udt) return;

    memset(udt, 0, sizeof(*udt));
    /* Name: AOI_name + "_Backing" */
    /* Generate backing UDT name: ensure truncation-safe */
    size_t name_len = strlen(aoi->name);
    size_t max_copy = (name_len < 31) ? name_len : 31;
    memcpy(udt->name, aoi->name, max_copy);
    memcpy(udt->name + max_copy, "_Backing", 9);
    udt->name[39] = '\0';

    /* Add a member for each AOI parameter */
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        logix_udt_add_member(udt,
                              aoi->parameters[i].name,
                              aoi->parameters[i].data_type,
                              0, 0, 0);
    }
}

bool logix_aoi_detect_circular_ref(const logix_aoi_t *aois,
                                     uint32_t aoi_count,
                                     aoi_call_graph_node_t *call_graph)
{
    if (!aois || !call_graph || aoi_count == 0) return false;

    /* Initialize call graph nodes */
    for (uint32_t i = 0; i < aoi_count; i++) {
        call_graph[i].aoi_index = i;
        call_graph[i].called_count = 0;
        call_graph[i].caller_count = 0;
        call_graph[i].has_circular_ref = false;
        call_graph[i].max_nest_depth = 0;
    }

    /* DFS-based cycle detection:
     * Colors: 0 = white (unvisited), 1 = gray (in stack), 2 = black (done)
     * Reserved for full DFS implementation */
    bool has_cycle = false;
    /* Simplified detection: check self-calls */
    for (uint32_t i = 0; i < aoi_count && i < 32; i++) {
        for (uint32_t j = 0; j < call_graph[i].called_count; j++) {
            if (call_graph[i].called_aoi_indices[j] == i) {
                call_graph[i].has_circular_ref = true;
                has_cycle = true;
            }
        }
    }

    return has_cycle;
}

void logix_aoi_update_signature(logix_aoi_t *aoi)
{
    if (!aoi) return;

    /* Generate a simple FNV-1a hash of AOI properties */
    uint32_t hash = 2166136261u;

    /* Hash name */
    for (const char *p = aoi->name; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619u;
    }

    /* Hash parameter info */
    hash ^= aoi->parameter_count;
    hash *= 16777619u;
    for (uint32_t i = 0; i < aoi->parameter_count; i++) {
        hash ^= (uint8_t)aoi->parameters[i].direction;
        hash ^= (uint8_t)aoi->parameters[i].data_type;
        hash *= 16777619u;
        for (const char *p = aoi->parameters[i].name; *p; p++) {
            hash ^= (uint8_t)*p;
            hash *= 16777619u;
        }
    }

    /* Hash revision */
    hash ^= aoi->major_revision;
    hash ^= aoi->minor_revision;
    hash *= 16777619u;

    snprintf(aoi->signature, sizeof(aoi->signature), "%08X", hash);
}