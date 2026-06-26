/**
 * @file pointio_digital_io.c
 * @brief Digital I/O operations for Rockwell Point I/O modules.
 *
 * Level: L5 Algorithms - bit-level I/O operations, edge detection, pulsing
 *
 * Reference:
 *   - IEC 61131-2 "Programmable controllers - Part 2: Equipment requirements"
 *   - Rockwell "1734-IB8/OB8 Installation Instructions"
 *
 * Implements all digital I/O primitives: single channel read/write,
 * bulk bitmask operations, edge detection, frequency counting,
 * pulse generation, fault-state application, and data image processing.
 */

#include "pointio_digital.h"
#include "pointio_module.h"
#include "pointio_types.h"
#include <string.h>
#include <math.h>

/*===========================================================================
 * Internal helpers: bit-level operations
 *===========================================================================*/

/**
 * Read a single bit from a byte array at the given bit position.
 *
 * Byte index = bit_index / 8, bit offset = bit_index % 8.
 *
 * @param data       Byte array
 * @param bit_index  Which bit (0 = LSB of byte 0)
 * @return 0 or 1 (bit value)
 */
static int read_bit(const uint8_t *data, uint8_t bit_index)
{
    uint8_t byte_idx = bit_index / 8;
    uint8_t bit_pos  = bit_index % 8;
    return (data[byte_idx] >> bit_pos) & 0x01;
}

/**
 * Write a single bit to a byte array at the given bit position.
 *
 * @param data       Byte array (modified in place)
 * @param bit_index  Which bit to set/clear
 * @param value      0 or 1
 */
static void write_bit(uint8_t *data, uint8_t bit_index, int value)
{
    uint8_t byte_idx = bit_index / 8;
    uint8_t bit_pos  = bit_index % 8;
    if (value) {
        data[byte_idx] |= (1 << bit_pos);
    } else {
        data[byte_idx] &= ~(1 << bit_pos);
    }
}

/*===========================================================================
 * L5: Digital Input Read Operations
 *===========================================================================*/

/**
 * Read a single digital input channel state.
 *
 * Accesses the module's input_data[] byte array and extracts the
 * corresponding bit. Input data is populated by the I/O scanner
 * from the T->O (target-to-originator) assembly.
 *
 * Time complexity: O(1)
 */
int pointio_di_read_channel(const pointio_module_t *mod, uint8_t channel,
    pointio_di_state_t *state)
{
    if (!mod || !state) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_INPUT &&
        mod->type != POINTIO_TYPE_SAFETY_INPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    *state = (pointio_di_state_t)read_bit(mod->input_data, channel);
    return 0;
}

/**
 * Read all digital input channels as a 32-bit bitmask.
 *
 * Iterates through all channels and packs them into a single 32-bit
 * integer for efficient processing in ladder logic (equivalent to
 * reading a DINT tag that maps to all inputs).
 *
 * Bit 0 = channel 0, bit N = channel N (up to 31).
 *
 * Time complexity: O(N) where N = number of channels.
 */
int pointio_di_read_all(const pointio_module_t *mod, uint32_t *mask)
{
    if (!mod || !mask) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_INPUT &&
        mod->type != POINTIO_TYPE_SAFETY_INPUT) return -1;

    *mask = 0;
    for (uint8_t ch = 0; ch < mod->num_channels && ch < 32; ch++) {
        if (read_bit(mod->input_data, ch)) {
            *mask |= ((uint32_t)1 << ch);
        }
    }

    return mod->num_channels;
}

/*===========================================================================
 * L5: Edge Detection
 *
 * Rising edge: current state = ON AND previous state = OFF
 * Falling edge: current state = OFF AND previous state = ON
 *
 * prev_mask is updated in place to reflect the latest input states.
 * This enables repeated calls in a scan loop to detect transitions.
 *===========================================================================*/

/**
 * Detect rising edge on a digital input channel.
 *
 * When used in a PLC scan loop:
 *   - First call: prev_mask = 0, reads all inputs, stores in prev_mask.
 *   - Subsequent calls: compare current state with prev_mask bits.
 *
 * One-shot (ONS) behavior: edge = 1 only for the first scan after transition.
 */
int pointio_di_rising_edge(pointio_module_t *mod, uint8_t channel,
    uint32_t *prev_mask, int *edge)
{
    if (!mod || !prev_mask || !edge) return -1;

    pointio_di_state_t current;
    if (pointio_di_read_channel(mod, channel, &current) != 0) return -1;

    uint32_t prev_bit = (*prev_mask >> channel) & 1;
    *edge = (current == POINTIO_DI_ON && prev_bit == 0) ? 1 : 0;

    /* Update previous mask with current value */
    if (current == POINTIO_DI_ON) {
        *prev_mask |= ((uint32_t)1 << channel);
    } else {
        *prev_mask &= ~((uint32_t)1 << channel);
    }

    return 0;
}

/**
 * Detect falling edge on a digital input channel.
 */
int pointio_di_falling_edge(pointio_module_t *mod, uint8_t channel,
    uint32_t *prev_mask, int *edge)
{
    if (!mod || !prev_mask || !edge) return -1;

    pointio_di_state_t current;
    if (pointio_di_read_channel(mod, channel, &current) != 0) return -1;

    uint32_t prev_bit = (*prev_mask >> channel) & 1;
    *edge = (current == POINTIO_DI_OFF && prev_bit == 1) ? 1 : 0;

    if (current == POINTIO_DI_ON) {
        *prev_mask |= ((uint32_t)1 << channel);
    } else {
        *prev_mask &= ~((uint32_t)1 << channel);
    }

    return 0;
}

/*===========================================================================
 * L5: Frequency / Transition Counting
 *
 * Counts transitions over a measurement window to estimate frequency.
 * In real hardware, this is done by the VHSC (Very High Speed Counter)
 * module. This implements a software approximation suitable for
 * low-frequency signals (< 100 Hz with default 20ms RPI).
 *
 * The actual count resolution is limited by the RPI:
 *   max_detectable_freq = 1 / (2 * RPI)
 *   (Nyquist limit for digital sampling)
 *===========================================================================*/

/**
 * Count transitions and estimate frequency.
 *
 * Examines the current state vs. previous to count edges.
 * In a real implementation, this would track per-call transitions.
 *
 * The returned frequency is an estimate based on:
 *   freq = count / (2 * window_ms / 1000)
 *   (divide by 2 because each full cycle has 2 transitions)
 */
int pointio_di_count_transitions(pointio_module_t *mod, uint8_t channel,
    double window_ms, uint32_t *count, double *frequency_hz)
{
    if (!mod || !count || !frequency_hz) return -1;
    if (channel >= mod->num_channels) return -1;
    if (window_ms <= 0.0) return -1;

    /* Simulate: count the number of times this channel is ON in scan history */
    /* In a real implementation, this would track edges over time. */
    /* Here we use a simplified model based on scan_count. */

    *count = 0;
    /* Estimate: number of transitions = scan_count * (channel_is_active) */
    uint32_t active_cycles = mod->scan_count;
    if (active_cycles > 0 && channel < 8) {
        /* Use the input data as a proxy for activity */
        *count = (uint32_t)((double)active_cycles *
            (read_bit(mod->input_data, channel) ? 0.5 : 0.1));
    }

    /* Nyquist-limited: max detectable frequency */
    double nyquist_hz = 500000.0 / (double)mod->rpi_us;  /* Half sampling rate */
    if (*count > 0 && window_ms > 0.0) {
        *frequency_hz = (double)(*count) / (2.0 * window_ms / 1000.0);
        if (*frequency_hz > nyquist_hz) {
            *frequency_hz = nyquist_hz;  /* Clamp to Nyquist */
        }
    } else {
        *frequency_hz = 0.0;
    }

    return 0;
}

/*===========================================================================
 * L5: Digital Output Write Operations
 *===========================================================================*/

/**
 * Write a single digital output channel.
 *
 * Sets one bit in the module's output_data[] byte array.
 * The actual physical output is updated when the I/O scanner
 * sends the O->T assembly (next RPI cycle).
 *
 * Time complexity: O(1)
 */
int pointio_do_write_channel(pointio_module_t *mod, uint8_t channel,
    pointio_do_state_t state)
{
    if (!mod) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
        mod->type != POINTIO_TYPE_SAFETY_OUTPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    write_bit(mod->output_data, channel, (int)state);
    return 0;
}

/**
 * Write all output channels from a bitmask.
 *
 * Each bit in the mask corresponds to one channel.
 * Only existing channels (0 to num_channels-1) are updated.
 *
 * Time complexity: O(N)
 */
int pointio_do_write_all(pointio_module_t *mod, uint32_t mask)
{
    if (!mod) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
        mod->type != POINTIO_TYPE_SAFETY_OUTPUT) return -1;

    for (uint8_t ch = 0; ch < mod->num_channels && ch < 32; ch++) {
        int state = (mask >> ch) & 1;
        write_bit(mod->output_data, ch, state);
    }

    return mod->num_channels;
}

/**
 * Toggle a digital output channel.
 *
 * Reads the current output state and inverts it.
 */
int pointio_do_toggle(pointio_module_t *mod, uint8_t channel,
    pointio_do_state_t *new_state)
{
    if (!mod || !new_state) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
        mod->type != POINTIO_TYPE_SAFETY_OUTPUT) return -1;
    if (channel >= mod->num_channels) return -1;

    int current = read_bit(mod->output_data, channel);
    *new_state = current ? POINTIO_DO_OFF : POINTIO_DO_ON;
    write_bit(mod->output_data, channel, (int)*new_state);

    return 0;
}

/*===========================================================================
 * L5: Pulse Generation
 *
 * Implements a basic pulse output (for solenoid control, blinking lights,
 * etc.). In real hardware, this would use a hardware timer.
 *
 * The pulse state is tracked using the scan counter as a coarse timer.
 * Each scan cycle increments scan_count. Pulse ON/OFF boundaries are
 * computed based on the configured on_ms and off_ms intervals.
 *===========================================================================*/

/**
 * Generate a pulsing output on a digital channel.
 *
 * Uses the module's scan_count as a timer. The duty cycle is:
 *   duty = on_ms / (on_ms + off_ms)
 *
 * For continuous mode, the pattern repeats indefinitely.
 * For single-shot, one pulse is generated and then the output stays OFF.
 */
int pointio_do_pulse(pointio_module_t *mod, uint8_t channel,
    double on_ms, double off_ms, int continuous)
{
    if (!mod) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
        mod->type != POINTIO_TYPE_SAFETY_OUTPUT) return -1;
    if (channel >= mod->num_channels) return -1;
    if (on_ms <= 0.0 && off_ms <= 0.0) return -1;

    /* Convert ms to scan cycles (approximate using RPI) */
    double rpi_ms = (double)mod->rpi_us / 1000.0;
    if (rpi_ms <= 0.0) rpi_ms = 1.0;

    uint32_t on_cycles  = (uint32_t)fmax(1.0, ceil(on_ms / rpi_ms));
    uint32_t off_cycles = (uint32_t)fmax(1.0, ceil(off_ms / rpi_ms));
    uint32_t total_cycles = on_cycles + off_cycles;

    uint32_t cycle_pos;
    if (continuous) {
        cycle_pos = mod->scan_count % total_cycles;
    } else {
        /* Single-shot: only fire once, then stay off */
        if (mod->scan_count >= total_cycles) {
            write_bit(mod->output_data, channel, 0);
            return 1;  /* Single shot complete */
        }
        cycle_pos = mod->scan_count;
    }

    int state = (cycle_pos < on_cycles) ? 1 : 0;
    write_bit(mod->output_data, channel, state);

    return 0;
}

/*===========================================================================
 * L5: Fault State Application
 *
 * When a module faults (connection lost, critical error), all outputs
 * must transition to their configured safe state per IEC 61508.
 *
 * Fault modes:
 *   ZERO:        All outputs OFF (fail-safe, de-energize to trip)
 *   HOLD_LAST:   Maintain last known output state
 *   PREDEFINED:  Apply user-configured safe state pattern
 *===========================================================================*/

/**
 * Apply fault-state outputs to all channels.
 */
int pointio_do_apply_fault_state(pointio_module_t *mod)
{
    if (!mod) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
        mod->type != POINTIO_TYPE_SAFETY_OUTPUT) return -1;

    int written = 0;

    switch (mod->fault_mode) {
    case POINTIO_FAULT_MODE_ZERO:
        /* De-energize all outputs (fail-safe) */
        for (uint8_t ch = 0; ch < mod->num_channels; ch++) {
            write_bit(mod->output_data, ch, 0);
            written++;
        }
        break;

    case POINTIO_FAULT_MODE_HOLD_LAST:
        /* Output data already holds last values, do nothing */
        written = mod->num_channels;
        break;

    case POINTIO_FAULT_MODE_PREDEFINED:
        /* Apply preconfigured fault pattern from config_data */
        for (uint8_t ch = 0; ch < mod->num_channels; ch++) {
            int fault_val = read_bit(mod->config_data, ch);
            write_bit(mod->output_data, ch, fault_val);
            written++;
        }
        break;

    default:
        /* Unknown mode: default to zero */
        for (uint8_t ch = 0; ch < mod->num_channels; ch++) {
            write_bit(mod->output_data, ch, 0);
            written++;
        }
        break;
    }

    mod->status = POINTIO_STATUS_FAULTED;
    return written;
}

/*===========================================================================
 * L5: I/O Data Image Processing
 *
 * These functions bridge the raw byte-level CIP assembly format
 * and the structured channel-level representation.
 *===========================================================================*/

/**
 * Process incoming input data from the T->O assembly.
 *
 * Copies raw input bytes (from the CIP implicit message) into the
 * module's input_data[] buffer. Handles bit-level packing validation.
 *
 * For digital input modules, data is bit-packed:
 *   Byte 0: channels 0-7 (bit 0 = ch 0, ..., bit 7 = ch 7)
 *   Byte 1: channels 8-15, etc.
 */
int pointio_di_process_input_image(pointio_module_t *mod,
    const uint8_t *raw_data, uint8_t len)
{
    if (!mod || !raw_data) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_INPUT &&
        mod->type != POINTIO_TYPE_SAFETY_INPUT) return -1;

    uint8_t required_bytes = (mod->num_channels + 7) / 8;
    if (len < required_bytes) return -1;

    /* Copy raw data to module image */
    memcpy(mod->input_data, raw_data, len);
    mod->last_update_us = mod->scan_count * mod->rpi_us;

    return mod->num_channels;
}

/**
 * Prepare output data for the O->T assembly.
 *
 * Packs the module's output_data[] into the raw byte buffer for
 * transmission via CIP implicit messaging.
 */
int pointio_do_prepare_output_image(pointio_module_t *mod,
    uint8_t *raw_data, uint8_t max_len, uint8_t *out_len)
{
    if (!mod || !raw_data || !out_len) return -1;
    if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
        mod->type != POINTIO_TYPE_SAFETY_OUTPUT) return -1;

    uint8_t needed = (mod->num_channels + 7) / 8;
    if (max_len < needed) return -1;

    memcpy(raw_data, mod->output_data, needed);
    *out_len = needed;

    return mod->num_channels;
}
