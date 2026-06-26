/**
 * guardlogix_signature.c -- CRC-32 and Safety Signature Implementation
 *
 * Knowledge points:
 *   L1: Safety signature concept -- project integrity verification
 *   L3: CRC-32 computation (IEEE 802.3 polynomial)
 *   L5: CRC lookup table generation and streaming computation
 *   L5: Multi-segment signature generation and verification
 *
 * The CRC-32 algorithm implements the standard 32-bit CRC with
 * polynomial 0xEDB88320 (reflected form of 0x04C11DB7).
 * This is the same CRC used in Ethernet, PNG, and ZIP.
 *
 * GuardLogix uses CRC-32 to compute safety signatures across
 * multiple memory segments (safety task, I/O config, network config).
 * Any modification changes the signature, which must match between
 * safety partners before the safety task can execute.
 */

#include "guardlogix_signature.h"
#include <string.h>
#include <stdlib.h>

/* CRC-32 lookup table -- computed once on first use */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

void glx_crc32_table_init(void)
{
    if (crc32_table_initialized) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ GLX_CRC32_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = 1;
}

uint32_t glx_crc32_compute(const uint8_t *data, size_t length,
                            uint32_t initial_crc)
{
    if (!data || length == 0) return initial_crc;

    glx_crc32_table_init();

    uint32_t crc = initial_crc ^ 0xFFFFFFFFU;
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (uint8_t)((crc ^ data[i]) & 0xFF);
        crc = (crc >> 8) ^ crc32_table[index];
    }
    return crc ^ 0xFFFFFFFFU;
}

uint32_t glx_crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    return glx_crc32_compute(data, length, crc);
}

uint32_t glx_crc32_continue(uint32_t crc, uint8_t byte)
{
    glx_crc32_table_init();
    uint32_t c = crc ^ 0xFFFFFFFFU;
    uint8_t index = (uint8_t)((c ^ byte) & 0xFF);
    c = (c >> 8) ^ crc32_table[index];
    return c ^ 0xFFFFFFFFU;
}

int glx_signature_block_init(glx_signature_block_t *block)
{
    if (!block) return -1;

    memset(block, 0, sizeof(*block));
    block->magic = GLX_SIGNATURE_MAGIC;
    block->segment_count = 0;
    block->signature_locked = 0;

    return 0;
}

int glx_signature_add_segment(glx_signature_block_t *block,
                               glx_signature_segment_t type,
                               const uint8_t *data,
                               uint32_t length)
{
    if (!block || !data) return -1;
    if (length == 0) return -1;
    if (block->segment_count >= GLX_SIG_SEGMENT_COUNT) return -1;

    /* Compute CRC-32 for this segment */
    uint32_t crc = glx_crc32_compute(data, length, 0);

    /* Store segment descriptor */
    uint32_t idx = block->segment_count;
    block->segments[idx].segment_type = type;
    block->segments[idx].segment_offset = idx; /* Logical offset */
    block->segments[idx].segment_length = length;
    block->segments[idx].segment_crc = crc;
    block->segments[idx].segment_verified = 0;

    block->segment_count++;
    return 0;
}

int glx_signature_finalize(glx_signature_block_t *block)
{
    if (!block) return -1;
    if (block->segment_count == 0) return -1;

    /* Compute combined CRC across all segment CRCs */
    uint32_t combined_crc = 0;
    for (uint32_t i = 0; i < block->segment_count; i++) {
        combined_crc = glx_crc32_update(
            combined_crc,
            (const uint8_t*)&block->segments[i].segment_crc,
            sizeof(uint32_t));
    }

    block->total_signature_crc = combined_crc;
    block->signature_locked = 1;
    return 0;
}

int glx_signature_verify(const glx_signature_block_t *block,
                          const glx_signature_block_t *expected)
{
    if (!block || !expected) return -1;

    /* Compare magic */
    if (block->magic != expected->magic) return -1;

    /* Compare total signature CRC */
    if (block->total_signature_crc != expected->total_signature_crc)
        return -1;

    /* Compare segment count */
    if (block->segment_count != expected->segment_count)
        return -1;

    /* Compare each segment CRC */
    for (uint32_t i = 0; i < block->segment_count; i++) {
        if (block->segments[i].segment_crc !=
            expected->segments[i].segment_crc) {
            return -1;
        }
        if (block->segments[i].segment_type !=
            expected->segments[i].segment_type) {
            return -1;
        }
    }

    return 0;
}

int glx_signature_compare_partner(const glx_signature_block_t *local,
                                   const glx_signature_block_t *partner,
                                   uint8_t *mismatched_segments,
                                   uint8_t *mismatch_count)
{
    if (!local || !partner) return -1;

    uint8_t count = 0;
    uint8_t mask = 0;

    if (local->total_signature_crc != partner->total_signature_crc) {
        /* Find which segments differ */
        uint32_t n = local->segment_count;
        if (partner->segment_count < n) n = partner->segment_count;

        for (uint32_t i = 0; i < n; i++) {
            if (local->segments[i].segment_crc !=
                partner->segments[i].segment_crc) {
                mask |= (1 << i);
                count++;
            }
        }

        /* Also flag if segment counts differ */
        if (local->segment_count != partner->segment_count) {
            mask |= (1 << 7); /* Highest bit = count mismatch */
            count++;
        }
    }

    if (mismatched_segments) *mismatched_segments = mask;
    if (mismatch_count) *mismatch_count = count;

    return (count == 0) ? 0 : -1;
}

int glx_generate_safety_signature(glx_safety_signature_t *sig,
                                   const uint8_t *safety_task_data,
                                   size_t task_data_len,
                                   const uint8_t *io_config_data,
                                   size_t io_config_len,
                                   uint16_t snn,
                                   uint32_t generation_time)
{
    if (!sig) return -1;

    memset(sig, 0, sizeof(*sig));

    /* Compute CRC-32 for safety task logic */
    if (safety_task_data && task_data_len > 0) {
        sig->safety_task_checksum =
            glx_crc32_compute(safety_task_data, task_data_len, 0);
    }

    /* Compute CRC-32 for safety I/O configuration */
    if (io_config_data && io_config_len > 0) {
        sig->safety_io_config_checksum =
            glx_crc32_compute(io_config_data, io_config_len, 0);
    }

    sig->safety_network_number = snn;
    sig->generation_time = generation_time;

    /* Overall signature = CRC of task CRC + IO CRC + SNN + timestamp */
    uint8_t combo[16];
    memcpy(combo, &sig->safety_task_checksum, 4);
    memcpy(combo + 4, &sig->safety_io_config_checksum, 4);
    memcpy(combo + 8, &snn, 2);
    memcpy(combo + 10, &generation_time, 4);
    combo[14] = 0;
    combo[15] = 0;

    sig->signature = ((uint64_t)glx_crc32_compute(combo, 16, 0) << 32) |
                     glx_crc32_compute(combo, 16, 0xFFFFFFFF);

    sig->partner_signature_matched = 0;
    return 0;
}
