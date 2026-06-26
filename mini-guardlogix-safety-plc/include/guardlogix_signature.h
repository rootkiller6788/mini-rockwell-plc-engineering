/**
 * guardlogix_signature.h -- Safety Signature & CRC Validation
 *
 * Covers L1: Safety signature concept, CRC-32 for safety validation,
 * Safety Network Number (SNN), project signature vs partner signature.
 *
 * Covers L3: CRC computation structures, signature generation with
 * multi-segment data, safety configuration lock signatures.
 *
 * L5 Algorithm: CRC-32 with IEEE 802.3 polynomial for safety signatures.
 * GuardLogix uses CRC-32 to verify the integrity of the safety task
 * program, safety I/O configuration, and safety network configuration.
 * Any modification to safety parameters generates a new signature.
 *
 * Reference: CIP Safety Specification Vol. 8
 *            GuardLogix 5580 Safety Reference Manual 1756-RM099
 */

#ifndef GUARDLOGIX_SIGNATURE_H
#define GUARDLOGIX_SIGNATURE_H

#include <stdint.h>
#include <stddef.h>
#include "guardlogix_safety.h"

/* CRC-32 polynomial: IEEE 802.3 (Ethernet) = x^32 + x^26 + ... + x + 1 */
#define GLX_CRC32_POLYNOMIAL  0xEDB88320U

/* Safety signature magic number for GuardLogix */
#define GLX_SIGNATURE_MAGIC   0x474C5353U  /* "GLSS" */

typedef enum {
    GLX_SIG_SEGMENT_SAFETY_TASK    = 0,
    GLX_SIG_SEGMENT_SAFETY_IO      = 1,
    GLX_SIG_SEGMENT_SAFETY_NETWORK = 2,
    GLX_SIG_SEGMENT_SAFETY_DATA    = 3,
    GLX_SIG_SEGMENT_USER_DEFINED   = 4,
    GLX_SIG_SEGMENT_COUNT          = 5
} glx_signature_segment_t;

typedef struct {
    glx_signature_segment_t segment_type;
    uint32_t segment_offset;
    uint32_t segment_length;
    uint32_t segment_crc;
    uint8_t  segment_verified;
} glx_signature_segment_desc_t;

typedef struct {
    uint32_t magic;
    uint32_t total_signature_crc;
    uint32_t segment_count;
    glx_signature_segment_desc_t segments[GLX_SIG_SEGMENT_COUNT];
    uint32_t generation_timestamp;
    uint16_t controller_family;
    uint16_t controller_id;
    uint8_t  signature_locked;
    uint32_t partner_signature_crc;
    uint8_t  partner_signature_match;
} glx_signature_block_t;

uint32_t glx_crc32_compute(const uint8_t *data, size_t length,
                            uint32_t initial_crc);
uint32_t glx_crc32_update(uint32_t crc, const uint8_t *data, size_t length);
int glx_signature_block_init(glx_signature_block_t *block);
int glx_signature_add_segment(glx_signature_block_t *block,
                               glx_signature_segment_t type,
                               const uint8_t *data,
                               uint32_t length);
int glx_signature_finalize(glx_signature_block_t *block);
int glx_signature_verify(const glx_signature_block_t *block,
                          const glx_signature_block_t *expected);
int glx_signature_compare_partner(const glx_signature_block_t *local,
                                   const glx_signature_block_t *partner,
                                   uint8_t *mismatched_segments,
                                   uint8_t *mismatch_count);
int glx_generate_safety_signature(glx_safety_signature_t *sig,
                                   const uint8_t *safety_task_data,
                                   size_t task_data_len,
                                   const uint8_t *io_config_data,
                                   size_t io_config_len,
                                   uint16_t snn,
                                   uint32_t generation_time);
void glx_crc32_table_init(void);
uint32_t glx_crc32_continue(uint32_t crc, uint8_t byte);

#endif /* GUARDLOGIX_SIGNATURE_H */
