/**
 * @file pointio_digital.h
 * @brief Digital I/O operations for Rockwell Point I/O modules.
 *
 * Level: L2 Core Concepts + L5 Algorithms
 * Reference:
 *   - 1734-IB8/OB8 Installation Instructions
 *   - IEC 61131-2 (Programmable Controllers - Equipment Requirements)
 */

#ifndef POINTIO_DIGITAL_H
#define POINTIO_DIGITAL_H

#include "pointio_types.h"
#include "pointio_module.h"

/*===========================================================================
 * L1: Digital I/O Channel States
 *===========================================================================*/

typedef enum {
    POINTIO_DI_OFF      = 0,
    POINTIO_DI_ON       = 1
} pointio_di_state_t;

typedef enum {
    POINTIO_DO_OFF      = 0,
    POINTIO_DO_ON       = 1
} pointio_do_state_t;

/**
 * @brief Digital input channel configuration.
 */
typedef struct {
    uint8_t                 channel;
    pointio_input_filter_t  filter;
    int                     invert;         /* 1 = invert logic */
    int                     enable_diag;    /* 1 = enable open-wire detect */
    int                     enable_cos;     /* 1 = enable Change-of-State */
    int                     latch_enable;   /* 1 = latch on transition */
    char                    alias[32];      /* User-friendly channel name */
} pointio_di_config_t;

/**
 * @brief Digital output channel configuration.
 */
typedef struct {
    uint8_t                 channel;
    pointio_fault_mode_t    fault_value;    /* State during fault */
    pointio_prog_mode_t     prog_value;     /* State during program mode */
    int                     enable_diag;    /* 1 = enable diagnostics */
    int                     pulse_test;     /* 1 = enable pulse testing */
    double                  pulse_on_time_ms;  /* Pulse on duration */
    double                  pulse_off_time_ms; /* Pulse off duration */
    char                    alias[32];
} pointio_do_config_t;

/**
 * @brief Digital I/O data image descriptor.
 *
 * Handles the mapping between bits and bytes for Point I/O modules.
 */
typedef struct {
    uint8_t *data;          /* Pointer to raw data array */
    uint8_t  num_channels;  /* Number of channels */
    uint8_t  byte_offset;   /* Offset into module data image */
} pointio_digital_data_t;

/*===========================================================================
 * L5: Digital Input Operations
 *===========================================================================*/

/**
 * @brief Read the state of a single digital input channel.
 *
 * Reads one bit from the module's input data image. Handles bit packing
 * for all Point I/O digital input modules (IB2 through IB16).
 *
 * Theorem: For a module with N channels, bit position = channel % 8,
 *          byte position = offset + channel / 8.
 *
 * @param mod      The digital input module
 * @param channel  Channel number (0-based)
 * @param state    Output: POINTIO_DI_ON or POINTIO_DI_OFF
 * @return 0 on success, -1 if module not configured as DI or channel out of range
 */
int pointio_di_read_channel(const pointio_module_t *mod, uint8_t channel,
    pointio_di_state_t *state);

/**
 * @brief Read all digital input channels as a bitmask.
 *
 * Efficient bulk read operation. Returns a 32-bit mask where bit N
 * corresponds to channel N.
 *
 * @param mod      The digital input module
 * @param mask     Output: bitmask of all channels
 * @return Number of channels read, -1 on error
 */
int pointio_di_read_all(const pointio_module_t *mod, uint32_t *mask);

/**
 * @brief Detect rising edge (OFF->ON) on a digital input channel.
 *
 * Requires previous state tracking via prev_mask parameter.
 *
 * @param mod         The digital input module
 * @param channel     Channel to check
 * @param prev_mask   Previous state mask (in/out: updated with current)
 * @param edge        Output: 1 if rising edge detected
 * @return 0 on success, -1 on error
 */
int pointio_di_rising_edge(pointio_module_t *mod, uint8_t channel,
    uint32_t *prev_mask, int *edge);

/**
 * @brief Detect falling edge (ON->OFF) on a digital input channel.
 *
 * @param mod         The digital input module
 * @param channel     Channel to check
 * @param prev_mask   Previous state mask (in/out: updated with current)
 * @param edge        Output: 1 if falling edge detected
 * @return 0 on success, -1 on error
 */
int pointio_di_falling_edge(pointio_module_t *mod, uint8_t channel,
    uint32_t *prev_mask, int *edge);

/**
 * @brief Count transitions on a digital input channel.
 *
 * Implements a software counter for frequency measurement.
 * Counts both rising and falling edges over the specified window.
 *
 * @param mod         The digital input module
 * @param channel     Channel to monitor
 * @param window_ms   Measurement window in milliseconds
 * @param count       Output: transition count
 * @param frequency_hz Output: estimated frequency
 * @return 0 on success, -1 on error
 */
int pointio_di_count_transitions(pointio_module_t *mod, uint8_t channel,
    double window_ms, uint32_t *count, double *frequency_hz);

/*===========================================================================
 * L5: Digital Output Operations
 *===========================================================================*/

/**
 * @brief Write a single digital output channel.
 *
 * @param mod      The digital output module
 * @param channel  Channel to set
 * @param state    Desired output state
 * @return 0 on success, -1 if module is not output type or channel invalid
 */
int pointio_do_write_channel(pointio_module_t *mod, uint8_t channel,
    pointio_do_state_t state);

/**
 * @brief Write all digital output channels from a bitmask.
 *
 * Each bit in the mask corresponds to one output channel.
 * Only channels that exist in the module are written; extra bits are ignored.
 *
 * @param mod   The digital output module
 * @param mask  32-bit bitmask of output states
 * @return Number of channels written, -1 on error
 */
int pointio_do_write_all(pointio_module_t *mod, uint32_t mask);

/**
 * @brief Toggle a digital output channel.
 *
 * Reads current state and inverts it.
 *
 * @param mod      The digital output module
 * @param channel  Channel to toggle
 * @param new_state Output: new state after toggle
 * @return 0 on success, -1 on error
 */
int pointio_do_toggle(pointio_module_t *mod, uint8_t channel,
    pointio_do_state_t *new_state);

/**
 * @brief Generate a pulse on a digital output.
 *
 * Sets output ON for pulse_on_time_ms, then OFF for pulse_off_time_ms.
 * Can be configured for continuous pulsing or single-shot.
 *
 * @param mod        The digital output module
 * @param channel    Output channel
 * @param on_ms      Pulse ON duration
 * @param off_ms     Pulse OFF duration
 * @param continuous 1 = repeat, 0 = single shot
 * @return 0 on success, -1 on error
 */
int pointio_do_pulse(pointio_module_t *mod, uint8_t channel,
    double on_ms, double off_ms, int continuous);

/**
 * @brief Apply fault-state outputs to all channels.
 *
 * When a module enters fault state (connection lost, module faulted),
 * this sets all output channels to their configured fault behavior.
 *
 * @param mod  The digital output module
 * @return Number of channels set, -1 on error
 */
int pointio_do_apply_fault_state(pointio_module_t *mod);

/*===========================================================================
 * L5: Digital I/O Batch Transfer
 *===========================================================================*/

/**
 * @brief Copy input data from raw module image to structured values.
 *
 * Called by the scanner after receiving input assembly data from the module.
 * Handles bit unpacking and any input processing (inversion, filtering).
 *
 * @param mod       The module (DI type)
 * @param raw_data  Raw input assembly bytes
 * @param len       Length of raw data in bytes
 * @return Number of channels processed, -1 on error
 */
int pointio_di_process_input_image(pointio_module_t *mod,
    const uint8_t *raw_data, uint8_t len);

/**
 * @brief Prepare output data from structured values to raw module image.
 *
 * Called by the scanner before sending output assembly data to the module.
 * Handles bit packing and output mode application.
 *
 * @param mod       The module (DO type)
 * @param raw_data  Output: packed output assembly bytes
 * @param max_len   Maximum buffer size
 * @param out_len   Output: actual bytes written
 * @return Number of channels processed, -1 on error
 */
int pointio_do_prepare_output_image(pointio_module_t *mod,
    uint8_t *raw_data, uint8_t max_len, uint8_t *out_len);

#endif /* POINTIO_DIGITAL_H */
