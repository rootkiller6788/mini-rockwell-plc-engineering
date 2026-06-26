/**
 * @file    logix_tag_model.c
 * @brief   ControlLogix Tag Data Model Implementation
 * L1-L3, L7
 */

#include <string.h>
#include <ctype.h>
#include "logix_tag_model.h"

bool logix_tag_create_controller_scope(logix_tag_t *tag,
                                         const char *name,
                                         logix_atomic_type_t type)
{
    if (!tag || !name) return false;
    if (!logix_tag_validate_name(name)) return false;

    memset(tag, 0, sizeof(*tag));
    strncpy(tag->name, name, 39);
    tag->name[39] = '\0';
    tag->scope = TAG_SCOPE_CONTROLLER;
    tag->type = type;
    tag->access = ACCESS_READ_WRITE;

    return true;
}

bool logix_tag_create_program_scope(logix_tag_t *tag,
                                      const char *name,
                                      logix_atomic_type_t type)
{
    if (!tag || !name) return false;
    if (!logix_tag_validate_name(name)) return false;

    memset(tag, 0, sizeof(*tag));
    strncpy(tag->name, name, 39);
    tag->name[39] = '\0';
    tag->scope = TAG_SCOPE_PROGRAM;
    tag->type = type;
    tag->access = ACCESS_READ_WRITE;

    return true;
}

bool logix_tag_set_alias(logix_tag_t *tag, const char *base_tag_name)
{
    if (!tag || !base_tag_name) return false;
    /* Alias must not already be an alias to another alias */
    if (tag->is_produced || tag->is_consumed) return false;

    tag->type = LOGIX_TYPE_ALIAS;
    strncpy(tag->alias_target, base_tag_name, 39);
    tag->alias_target[39] = '\0';

    return true;
}

bool logix_tag_set_produced(logix_tag_t *tag, double rpi_ms)
{
    if (!tag) return false;
    /* Produced tags must be controller-scope */
    if (tag->scope != TAG_SCOPE_CONTROLLER) return false;
    if (tag->is_consumed || logix_tag_is_alias(tag)) return false;
    if (rpi_ms < 0.5 || rpi_ms > 750.0) return false;

    tag->is_produced = true;
    tag->produced_connection_id = (uint32_t)(rpi_ms * 1000.0);

    return true;
}

bool logix_tag_set_consumed(logix_tag_t *tag, double rpi_ms)
{
    if (!tag) return false;
    if (tag->is_produced) return false;
    if (rpi_ms < 0.5 || rpi_ms > 750.0) return false;

    tag->is_consumed = true;
    tag->consumed_connection_id = (uint32_t)(rpi_ms * 1000.0);

    return true;
}

bool logix_tag_write(logix_tag_t *tag, const void *source)
{
    if (!tag || !source) return false;
    if (tag->is_constant) return false;
    if (tag->access == ACCESS_READ_ONLY) return false;

    uint32_t size = logix_type_size(tag->type);
    if (size == 0 && tag->type != LOGIX_TYPE_STRUCT &&
        tag->type != LOGIX_TYPE_ARRAY) return false;

    if (tag->type <= LOGIX_TYPE_ULINT) {
        memcpy(&tag->value, source, size);
    }
    return true;
}

uint32_t logix_tag_read(const logix_tag_t *tag, void *dest)
{
    if (!tag || !dest) return 0;

    uint32_t size = logix_type_size(tag->type);
    if (size == 0) return 0;

    memcpy(dest, &tag->value, size);
    return size;
}

uint32_t logix_udt_calculate_size(const logix_udt_t *udt)
{
    if (!udt) return 0;

    uint32_t total = 0;
    for (uint32_t i = 0; i < udt->member_count; i++) {
        uint32_t member_size = logix_type_size(udt->members[i].type);

        /* Handle array members */
        if (udt->members[i].array_info.dim_count > 0) {
            member_size *= udt->members[i].array_info.total_elements;
        }

        /* Alignment: align to member size boundary (up to 8 bytes) */
        uint32_t alignment = member_size;
        if (alignment > 8) alignment = 8;
        if (alignment == 0) alignment = 1;

        uint32_t padding = (alignment - (total % alignment)) % alignment;
        total += padding + member_size;
    }

    return total;
}

bool logix_tag_validate_name(const char *name)
{
    if (!name) return false;

    size_t len = strlen(name);
    if (len == 0 || len > 40) return false;

    /* Must start with letter or underscore */
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return false;

    /* Only A-Z, a-z, 0-9, underscore */
    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if (!isalnum((unsigned char)c) && c != '_') return false;
    }

    return true;
}

uint32_t logix_type_size(logix_atomic_type_t type)
{
    switch (type) {
        case LOGIX_TYPE_BOOL:   return 1;  /* 1 bit stored as 8 bits */
        case LOGIX_TYPE_SINT:   return 1;
        case LOGIX_TYPE_INT:    return 2;
        case LOGIX_TYPE_DINT:   return 4;
        case LOGIX_TYPE_LINT:   return 8;
        case LOGIX_TYPE_REAL:   return 4;
        case LOGIX_TYPE_LREAL:  return 8;
        case LOGIX_TYPE_USINT:  return 1;
        case LOGIX_TYPE_UINT:   return 2;
        case LOGIX_TYPE_UDINT:  return 4;
        case LOGIX_TYPE_ULINT:  return 8;
        case LOGIX_TYPE_STRING: return 84;  /* 2-byte length + 82 chars */
        default: return 0;
    }
}

const char *logix_type_name(logix_atomic_type_t type)
{
    switch (type) {
        case LOGIX_TYPE_BOOL:   return "BOOL";
        case LOGIX_TYPE_SINT:   return "SINT";
        case LOGIX_TYPE_INT:    return "INT";
        case LOGIX_TYPE_DINT:   return "DINT";
        case LOGIX_TYPE_LINT:   return "LINT";
        case LOGIX_TYPE_REAL:   return "REAL";
        case LOGIX_TYPE_LREAL:  return "LREAL";
        case LOGIX_TYPE_USINT:  return "USINT";
        case LOGIX_TYPE_UINT:   return "UINT";
        case LOGIX_TYPE_UDINT:  return "UDINT";
        case LOGIX_TYPE_ULINT:  return "ULINT";
        case LOGIX_TYPE_STRING: return "STRING";
        case LOGIX_TYPE_STRING_N: return "STRING[N]";
        case LOGIX_TYPE_ARRAY:  return "ARRAY";
        case LOGIX_TYPE_STRUCT: return "STRUCT";
        case LOGIX_TYPE_ALIAS:  return "ALIAS";
        case LOGIX_TYPE_MODULE: return "MODULE";
        default: return "UNKNOWN";
    }
}

bool logix_tag_is_alias(const logix_tag_t *tag)
{
    if (!tag) return false;
    return tag->type == LOGIX_TYPE_ALIAS;
}

void logix_memory_update_stats(const logix_tag_t *tags,
                                uint32_t tag_count,
                                logix_memory_stats_t *stats)
{
    if (!tags || !stats) return;

    memset(stats, 0, sizeof(*stats));
    stats->tag_count_controller_scope = 0;
    stats->tag_count_program_scope = 0;

    for (uint32_t i = 0; i < tag_count; i++) {
        uint32_t size = logix_type_size(tags[i].type);

        if (tags[i].scope == TAG_SCOPE_CONTROLLER) {
            stats->tag_count_controller_scope++;
        } else {
            stats->tag_count_program_scope++;
        }

        if (tags[i].array_desc.dim_count > 0) {
            size *= tags[i].array_desc.total_elements;
        }

        stats->data_memory_used += size;
    }

    stats->max_tag_name_length = 40;
}

int32_t logix_array_offset(const logix_array_desc_t *desc,
                             uint32_t i1, uint32_t i2, uint32_t i3)
{
    if (!desc) return -1;
    if (desc->dim_count == 0) return -1;

    switch (desc->dim_count) {
        case 1:
            if (i1 >= desc->dim_size[0]) return -1;
            return (int32_t)(i1 * desc->element_size);
        case 2:
            if (i1 >= desc->dim_size[0] || i2 >= desc->dim_size[1]) return -1;
            return (int32_t)((i1 * desc->dim_size[1] + i2) * desc->element_size);
        case 3:
            if (i1 >= desc->dim_size[0] || i2 >= desc->dim_size[1] ||
                i3 >= desc->dim_size[2]) return -1;
            return (int32_t)((i1 * desc->dim_size[1] * desc->dim_size[2] +
                              i2 * desc->dim_size[2] + i3) * desc->element_size);
        default:
            return -1;
    }
}