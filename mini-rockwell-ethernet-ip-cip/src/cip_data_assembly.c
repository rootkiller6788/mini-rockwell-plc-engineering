/**
 * cip_data_assembly.c - CIP Data Assembly Object Implementation
 * Knowledge Domain: L3 Engineering Structures, L5 Algorithms
 * Reference: ODVA CIP Vol 1, Chapter 5-4: Assembly Object
 *
 * Each function implements one independent knowledge point:
 *   1. cip_da_init_template()        - Assembly member template definition
 *   2. cip_da_add_member()           - Add data member to assembly map
 *   3. cip_da_calculate_size()       - Compute total assembly byte size
 *   4. cip_da_pack_data()            - Pack application data into assembly image
 *   5. cip_da_unpack_data()          - Unpack assembly image to application data
 *   6. cip_da_get_member_offset()    - Find byte offset of a member by index
 *   7. cip_da_byte_swap_16()         - Little-endian to big-endian conversion
 *   8. cip_da_byte_swap_32()         - 32-bit byte order swap
 *
 * School: RWTH Aachen, Purdue ME 575 - Industrial data modeling
 */
#include <string.h>
#include <stddef.h>
#include "../include/cip_object.h"
/* Types cip_da_member_t and cip_da_template_t are defined in cip_object.h */

/* -------------------------------------------------------------------------
 * cip_da_init_template - Initialize empty assembly template
 *
 * Knowledge: CIP Assembly data mapping.
 * An assembly template defines how application data maps to the flat
 * byte array sent over the I/O connection. Each member has a CIP data
 * type, byte offset, and optional bit offset for BOOL members.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
void cip_da_init_template(cip_da_template_t *tmpl, uint32_t instance)
{
    if (!tmpl) return;
    memset(tmpl, 0, sizeof(cip_da_template_t));
    tmpl->assembly_instance = instance;
}

/* -------------------------------------------------------------------------
 * cip_da_add_member - Add a data member to the assembly template
 *
 * Knowledge: CIP Assembly member definition.
 * Each member represents one logical data item within the assembly.
 * The byte_offset is auto-computed as the running sum of previous sizes.
 * For BOOL types, bit_offset and bit_size allow packing multiple bits
 * into a single byte (e.g., 8 digital inputs in 1 byte).
 *
 * Returns: 0 = added, -1 = template full, -2 = invalid type.
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
int cip_da_add_member(cip_da_template_t *tmpl, uint16_t member_id,
                      uint8_t data_type, uint16_t byte_size,
                      uint8_t bit_offset, uint8_t bit_size,
                      const char *name)
{
    if (!tmpl) return -1;
    if (tmpl->member_count >= CIP_DA_MAX_MEMBERS) return -2;

    cip_da_member_t *m = &tmpl->members[tmpl->member_count];
    m->member_id   = member_id;
    m->data_type   = data_type;
    m->byte_offset = tmpl->total_size;
    m->byte_size   = byte_size;
    m->bit_offset  = bit_offset;
    m->bit_size    = bit_size;

    if (name) {
        size_t len = strlen(name);
        if (len > 31) len = 31;
        memcpy(m->member_name, name, len);
        m->member_name[len] = '\0';
    }

    tmpl->total_size += byte_size;
    tmpl->member_count++;
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_da_calculate_size - Calculate total assembly data size
 *
 * Knowledge: Assembly sizing for connection establishment.
 * The total_size is the sum of all member byte_sizes and is used in
 * the Forward Open request to specify O->T and T->O connection sizes.
 *
 * Returns: total size in bytes. Complexity: O(1) (returns cached value).
 * ------------------------------------------------------------------------- */
uint16_t cip_da_calculate_size(const cip_da_template_t *tmpl)
{
    if (!tmpl) return 0;
    return tmpl->total_size;
}

/* -------------------------------------------------------------------------
 * cip_da_pack_data - Pack application data into assembly byte image
 *
 * Knowledge: CIP data marshaling for I/O production.
 * Converts typed application data (DINTs, REALs, BOOLs) into the flat
 * little-endian byte array sent over the network. CIP uses little-endian
 * byte order for multi-byte types.
 *
 * The caller provides a buffer of values and lengths; this function
 * writes them at the correct offsets per the assembly template.
 *
 * Returns: 0 = success, -1 = buffer too small.
 * Complexity: O(n*m) where n = member count, m = total data size.
 * ------------------------------------------------------------------------- */
int cip_da_pack_data(const cip_da_template_t *tmpl,
                     const uint8_t **member_data,
                     const uint16_t *member_data_sizes,
                     uint8_t *assembly_buffer, uint16_t buffer_size)
{
    if (!tmpl || !member_data || !member_data_sizes || !assembly_buffer)
        return -1;
    if (buffer_size < tmpl->total_size) return -2;

    memset(assembly_buffer, 0, buffer_size);

    for (uint8_t i = 0; i < tmpl->member_count; i++) {
        const cip_da_member_t *m = &tmpl->members[i];
        if (m->byte_size > member_data_sizes[i]) return -3;
        if (member_data[i]) {
            memcpy(assembly_buffer + m->byte_offset,
                   member_data[i], m->byte_size);
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_da_unpack_data - Unpack assembly byte image to application data
 *
 * Knowledge: CIP data unmarshaling for I/O consumption.
 * The inverse of pack: extracts typed data from the incoming flat byte
 * array received over the I/O connection.
 *
 * Returns: 0 = success, -1 = invalid pointer.
 * Complexity: O(n*m).
 * ------------------------------------------------------------------------- */
int cip_da_unpack_data(const cip_da_template_t *tmpl,
                       const uint8_t *assembly_buffer, uint16_t buffer_size,
                       uint8_t **member_buffers,
                       uint16_t *member_buffer_sizes)
{
    if (!tmpl || !assembly_buffer || !member_buffers || !member_buffer_sizes)
        return -1;
    if (buffer_size < tmpl->total_size) return -2;

    for (uint8_t i = 0; i < tmpl->member_count; i++) {
        const cip_da_member_t *m = &tmpl->members[i];
        if (m->byte_size > member_buffer_sizes[i]) return -3;
        if (member_buffers[i]) {
            memcpy(member_buffers[i],
                   assembly_buffer + m->byte_offset, m->byte_size);
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * cip_da_get_member_offset - Find byte offset of a member by index
 *
 * Knowledge: Assembly introspection for runtime access.
 * Returns the byte offset of member index 'idx' within the assembly.
 * Useful for direct memory-mapped I/O access in PLC firmware.
 *
 * Returns: offset, or 0xFFFF if index out of range.
 * Complexity: O(1) - array lookup.
 * ------------------------------------------------------------------------- */
uint16_t cip_da_get_member_offset(const cip_da_template_t *tmpl, uint8_t idx)
{
    if (!tmpl || idx >= tmpl->member_count) return 0xFFFFu;
    return tmpl->members[idx].byte_offset;
}

/* -------------------------------------------------------------------------
 * cip_da_byte_swap_16 - 16-bit byte order swap
 *
 * Knowledge: CIP endianness: CIP uses little-endian.
 * Network byte order conversion for 16-bit values (INT, UINT).
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint16_t cip_da_byte_swap_16(uint16_t value)
{
    return (uint16_t)(((value & 0xFF00u) >> 8) | ((value & 0x00FFu) << 8));
}

/* -------------------------------------------------------------------------
 * cip_da_byte_swap_32 - 32-bit byte order swap
 *
 * Knowledge: CIP endianness for 32-bit values (DINT, REAL, UDINT).
 * Complexity: O(1).
 * ------------------------------------------------------------------------- */
uint32_t cip_da_byte_swap_32(uint32_t value)
{
    return ((value & 0xFF000000u) >> 24)
         | ((value & 0x00FF0000u) >> 8)
         | ((value & 0x0000FF00u) << 8)
         | ((value & 0x000000FFu) << 24);
}
