/**
 * @file pointio_scanner.c
 * @brief I/O scanning engine for Point I/O data exchange.
 *
 * Level: L5 Algorithms - cyclic I/O scan, data mapping, trigger handling
 *
 * Reference:
 *   - Rockwell "Logix5000 Controllers I/O and Tag Data" (1756-PM004), Ch.4
 *   - Rockwell "CompactLogix 5370 Controllers User Manual" (1769-UM021), Ch.5
 *
 * Implements the I/O scan cycle:
 *   1. Read inputs from all modules (T->O data)
 *   2. Execute user program (not included - this is the PLC scan)
 *   3. Write outputs to all modules (O->T data)
 *   4. Update housekeeping (scan timing, faults)
 *
 * Supports:
 *   - Cyclic scanning (fixed RPI)
 *   - Change-of-State (COS) scanning
 *   - Rack-optimized connections (single packet for digital modules)
 *   - Scan timing analysis and jitter monitoring
 */

#include "pointio_types.h"
#include "pointio_module.h"
#include "pointio_connection.h"
#include "pointio_digital.h"
#include "pointio_analog.h"
#include "pointio_diagnostics.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/*===========================================================================
 * L3: Scan Engine State
 *===========================================================================*/

/**
 * @brief I/O scanner state machine.
 */
typedef struct {
    uint64_t    scan_number;            /* Monotonically increasing */
    uint64_t    last_scan_start_us;     /* Timestamp of last scan start */
    uint64_t    last_scan_end_us;       /* Timestamp of last scan end */
    uint32_t    scan_interval_us;       /* Configured scan interval */
    double      avg_scan_time_us;       /* Rolling average */
    double      max_scan_time_us;
    double      min_scan_time_us;
    uint32_t    overruns;               /* Scans exceeding interval */
    int         scanning_enabled;       /* 1 = running, 0 = stopped */
    int         cos_pending;            /* 1 = COS event queued */
} pointio_scanner_state_t;

/* Global scanner state */
static pointio_scanner_state_t g_scanner = {0};

/*===========================================================================
 * L5: I/O Scan Cycle
 *
 * The I/O scan is the fundamental timing loop that updates all
 * I/O data. In CompactLogix, the scan cycle is:
 *
 *   Housekeeping → Input Scan → Program Scan → Output Scan → Comms
 *
 * The Input Scan reads all T->O data from Point I/O modules and
 * copies it to the controller's input image table.
 * The Output Scan writes the controller's output image table to
 * the O->T data of each module.
 *
 * RPI determines the scan rate. Typical values:
 *   - Fast digital: 2-20ms
 *   - Analog: 20-100ms
 *   - Slow/high-precision: 100-750ms
 *
 * The scan must complete within the RPI to avoid overruns.
 *===========================================================================*/

/**
 * Execute one complete I/O scan cycle on a chassis.
 *
 * Processes all modules in order: inputs first (so program scan
 * gets fresh data), then outputs (after program scan).
 *
 * @param chassis   The I/O chassis
 * @param pool      Active connection pool
 * @param now_us    Current timestamp (microseconds)
 * @return 0 on success, -1 on critical fault
 */
int pointio_scanner_scan_cycle(pointio_chassis_t *chassis,
    pointio_connection_pool_t *pool, uint64_t now_us)
{
    if (!chassis || !pool) return -1;
    if (!g_scanner.scanning_enabled) return -1;

    /* Record scan start */
    g_scanner.last_scan_start_us = now_us;
    g_scanner.scan_number++;

    /* === Phase 1: Input Scan (read T->O data) === */
    for (int slot = 0; slot < POINTIO_MAX_MODULES; slot++) {
        pointio_module_t *mod = &chassis->modules[slot];

        if (mod->status != POINTIO_STATUS_OK &&
            mod->status != POINTIO_STATUS_STANDBY) {
            continue;
        }
        if (mod->type == POINTIO_TYPE_UNKNOWN) continue;

        /* Process module input data */
        /* In real system: trigger CIP implicit message receive */
        /* Here: simulate data arrival */

        if (mod->type == POINTIO_TYPE_DIGITAL_INPUT ||
            mod->type == POINTIO_TYPE_SAFETY_INPUT) {
            /* Digital input: bit-packed format, already in input_data[] */
            /* Count "scan" as receipt of input data */
            mod->last_update_us = (uint32_t)now_us;
        } else if (mod->type == POINTIO_TYPE_ANALOG_INPUT) {
            /* Analog input: INT format, 2 bytes per channel */
            mod->last_update_us = (uint32_t)now_us;
        }

        mod->scan_count++;
    }

    /* === Phase 2: Configurable delay (simulates program scan) === */
    /* In real PLC, the user program runs here */
    /* We simulate a brief delay representative of logic execution */
    /* (Not implemented - this is a non-functional simulation) */

    /* === Phase 3: Output Scan (write O->T data) === */
    for (int slot = 0; slot < POINTIO_MAX_MODULES; slot++) {
        pointio_module_t *mod = &chassis->modules[slot];

        if (mod->status != POINTIO_STATUS_OK) continue;
        if (mod->output_channels == 0) continue;

        /* Prepare and "send" output data */
        uint8_t output_buffer[POINTIO_MAX_DATA_SIZE];
        uint8_t out_len = 0;

        if (mod->type == POINTIO_TYPE_DIGITAL_OUTPUT ||
            mod->type == POINTIO_TYPE_SAFETY_OUTPUT) {
            pointio_do_prepare_output_image(mod, output_buffer,
                POINTIO_MAX_DATA_SIZE, &out_len);
        } else if (mod->type == POINTIO_TYPE_ANALOG_OUTPUT) {
            /* Analog output data already in output_data[] */
            out_len = mod->output_size_bytes;
            memcpy(output_buffer, mod->output_data, out_len);
        }

        /* "Send" via CIP */
        if (mod->connection_id >= 0 && out_len > 0) {
            pointio_conn_send_output(pool, mod->connection_id,
                output_buffer, out_len);
        }
    }

    /* === Phase 4: Housekeeping === */
    uint64_t scan_end = now_us;  /* Approximate */
    uint64_t scan_duration = scan_end - g_scanner.last_scan_start_us;

    /* Update timing statistics */
    if (g_scanner.scan_number == 1) {
        g_scanner.avg_scan_time_us = (double)scan_duration;
        g_scanner.max_scan_time_us = (double)scan_duration;
        g_scanner.min_scan_time_us = (double)scan_duration;
    } else {
        /* Exponential moving average with alpha = 0.2 */
        g_scanner.avg_scan_time_us = 0.8 * g_scanner.avg_scan_time_us +
                                      0.2 * (double)scan_duration;
        if ((double)scan_duration > g_scanner.max_scan_time_us) {
            g_scanner.max_scan_time_us = (double)scan_duration;
        }
        if ((double)scan_duration < g_scanner.min_scan_time_us) {
            g_scanner.min_scan_time_us = (double)scan_duration;
        }
    }

    g_scanner.last_scan_end_us = scan_end;

    /* Detect scan overrun */
    if ((double)scan_duration > (double)g_scanner.scan_interval_us) {
        g_scanner.overruns++;
    }

    /* Check connection timeouts */
    pointio_conn_check_timeouts(pool, now_us);

    return 0;
}

/*===========================================================================
 * L5: Scanner Control
 *===========================================================================*/

/**
 * Initialize and start the I/O scanner.
 *
 * Configures the scan interval (typically the fastest RPI among
 * all connections) and resets all timing statistics.
 *
 * @param interval_us  Scan interval in microseconds
 * @return 0 on success, -1 if interval is 0
 */
int pointio_scanner_start(uint32_t interval_us)
{
    if (interval_us == 0) return -1;

    memset(&g_scanner, 0, sizeof(g_scanner));
    g_scanner.scan_interval_us = interval_us;
    g_scanner.scanning_enabled = 1;
    g_scanner.min_scan_time_us = 1e12;  /* Large initial value */
    g_scanner.max_scan_time_us = 0.0;

    return 0;
}

/**
 * Stop the I/O scanner.
 *
 * Sets all module outputs to their fault state before stopping.
 */
int pointio_scanner_stop(pointio_chassis_t *chassis)
{
    if (!chassis) return -1;

    /* Apply fault states to all output modules */
    for (int slot = 0; slot < POINTIO_MAX_MODULES; slot++) {
        pointio_module_t *mod = &chassis->modules[slot];
        if (mod->output_channels > 0 &&
            (mod->type == POINTIO_TYPE_DIGITAL_OUTPUT ||
             mod->type == POINTIO_TYPE_SAFETY_OUTPUT)) {
            pointio_do_apply_fault_state(mod);
        }
    }

    g_scanner.scanning_enabled = 0;
    return 0;
}

/**
 * Check if the scanner is currently running.
 */
int pointio_scanner_is_running(void)
{
    return g_scanner.scanning_enabled;
}

/*===========================================================================
 * L5: Change-of-State (COS) Trigger
 *
 * COS scanning is more efficient than cyclic scanning for modules
 * where the input state changes infrequently (e.g., limit switches,
 * pushbuttons). Instead of transmitting at every RPI, the module
 * transmits only on state change, with a background heartbeat at
 * a much slower rate.
 *
 * The COS trigger accelerates RPI temporarily when a change is detected,
 * providing near-instant notification of field events.
 *===========================================================================*/

/**
 * Signal a COS event (simulates a field device state change).
 *
 * In real hardware, the module detects the input state change and
 * immediately sends a T->O packet, bypassing the RPI timer.
 * The scanner then processes this packet out-of-cycle.
 */
int pointio_scanner_signal_cos(pointio_chassis_t *chassis, uint8_t slot)
{
    if (!chassis) return -1;
    if (slot == 0 || slot >= POINTIO_MAX_MODULES) return -1;

    pointio_module_t *mod = &chassis->modules[slot];
    if (mod->status != POINTIO_STATUS_OK) return -1;

    g_scanner.cos_pending = 1;

    /* Process the COS input immediately */
    mod->scan_count++;
    mod->last_update_us = (uint32_t)g_scanner.last_scan_start_us;

    return 0;
}

/*===========================================================================
 * L5: Scan Timing Analysis
 *
 * Measures scan jitter, which is critical for:
 *   - Motion control (axis position updates must be isochronous)
 *   - Fast PID loops (I/O update jitter degrades control quality)
 *   - Time-stamped data (Sequence of Events accuracy depends on
 *     known and bounded scan time)
 *
 * Acceptable jitter:
 *   - Process control: < 10% of RPI
 *   - Motion/position: < 1% of RPI
 *   - SOE: < 100 us absolute
 *===========================================================================*/

/**
 * Get current scan timing statistics.
 */
int pointio_scanner_get_timing(double *avg_scan_us, double *max_scan_us,
    double *min_scan_us, uint32_t *overruns)
{
    if (avg_scan_us)  *avg_scan_us  = g_scanner.avg_scan_time_us;
    if (max_scan_us)  *max_scan_us  = g_scanner.max_scan_time_us;
    if (min_scan_us)  *min_scan_us  = g_scanner.min_scan_time_us;
    if (overruns)     *overruns     = g_scanner.overruns;

    return 0;
}

/**
 * Analyze scan jitter against acceptable limits.
 *
 * Computes max_jitter = max_scan - min_scan and
 * RMS jitter = sqrt(avg((scan_i - avg)^2)).
 *
 * @param max_jitter_us  Output: max observed jitter
 * @param rms_jitter_us  Output: RMS jitter
 * @param is_excessive   Output: 1 if jitter > 25% of scan interval
 * @return 0 on success
 */
int pointio_scanner_analyze_jitter(double *max_jitter_us,
    double *rms_jitter_us, int *is_excessive)
{
    if (!max_jitter_us || !rms_jitter_us || !is_excessive) return -1;

    *max_jitter_us = g_scanner.max_scan_time_us - g_scanner.min_scan_time_us;

    /* RMS jitter approximation: (max-min)/6 for normal-like distributions */
    *rms_jitter_us = (*max_jitter_us) / 6.0;

    /* Excessive if jitter > 25% of scan interval */
    *is_excessive = 0;
    if (g_scanner.scan_interval_us > 0 &&
        *max_jitter_us > 0.25 * (double)g_scanner.scan_interval_us) {
        *is_excessive = 1;
    }

    return 0;
}

/*===========================================================================
 * L5: Module Data Synchronization
 *
 * Syncs a module's input image with fresh data (simulated).
 * In real hardware, this reads the latest CIP implicit message.
 *===========================================================================*/

/**
 * Force an immediate input update for a specific module.
 *
 * Bypasses the normal scan cycle for one-shot diagnostic reads.
 *
 * @param chassis  The chassis
 * @param slot     Module slot to update
 * @return 0 on success
 */
int pointio_scanner_update_module_input(pointio_chassis_t *chassis,
    uint8_t slot)
{
    if (!chassis) return -1;
    if (slot >= POINTIO_MAX_MODULES) return -1;

    pointio_module_t *mod = &chassis->modules[slot];
    if (mod->status != POINTIO_STATUS_OK && mod->status != POINTIO_STATUS_STANDBY) {
        return -1;
    }

    /* Trigger a simulated input update */
    mod->scan_count++;
    mod->last_update_us = (uint32_t)g_scanner.last_scan_start_us;

    return 0;
}

/*===========================================================================
 * L6: Rack-Optimized Connection
 *
 * For digital I/O modules, CompactLogix supports rack-optimized
 * connections. Instead of one connection per module, a single
 * connection carries the data for all digital modules in the chassis.
 * This dramatically reduces the number of CIP connections and
 * EtherNet/IP packets.
 *
 * The rack-optimized assembly maps:
 *   - Slot 0: Adapter status (4 bytes)
 *   - Slot 1: Digital input module 1 (1-2 bytes)
 *   - Slot 2: Digital input module 2 (1-2 bytes)
 *   - ...
 *   - Total: Compact image with all digital data contiguous
 *
 * Analog and specialty modules still use direct connections.
 *===========================================================================*/

/**
 * Build a rack-optimized input assembly.
 *
 * @param chassis     The chassis
 * @param buffer      Output buffer for rack-optimized data
 * @param buffer_size Size of buffer
 * @param data_len    Output: actual bytes written
 * @return 0 on success
 */
int pointio_scanner_build_rack_optimized_input(const pointio_chassis_t *chassis,
    uint8_t *buffer, uint16_t buffer_size, uint16_t *data_len)
{
    if (!chassis || !buffer || !data_len) return -1;

    uint16_t offset = 0;

    /* Header: adapter status */
    if (offset + 4 > buffer_size) return -1;
    buffer[offset++] = (uint8_t)chassis->modules[0].status;
    buffer[offset++] = 0;  /* Reserved */
    buffer[offset++] = 0;
    buffer[offset++] = 0;

    /* Digital input modules: pack data sequentially */
    for (int slot = 1; slot < POINTIO_MAX_MODULES; slot++) {
        const pointio_module_t *mod = &chassis->modules[slot];
        if (mod->status != POINTIO_STATUS_OK) continue;
        if (mod->type != POINTIO_TYPE_DIGITAL_INPUT &&
            mod->type != POINTIO_TYPE_SAFETY_INPUT) continue;

        uint8_t needed = (mod->num_channels + 7) / 8;
        if (offset + needed > buffer_size) return -1;

        memcpy(&buffer[offset], mod->input_data, needed);
        offset += needed;
    }

    *data_len = offset;
    return 0;
}

/**
 * Parse a rack-optimized output assembly and distribute to modules.
 */
int pointio_scanner_distribute_rack_optimized_output(pointio_chassis_t *chassis,
    const uint8_t *data, uint16_t data_len)
{
    if (!chassis || !data) return -1;

    uint16_t offset = 0;

    /* Skip adapter header (4 bytes) */
    if (offset + 4 > data_len) return -1;
    offset += 4;

    /* Distribute to digital output modules */
    for (int slot = 1; slot < POINTIO_MAX_MODULES; slot++) {
        pointio_module_t *mod = &chassis->modules[slot];
        if (mod->status != POINTIO_STATUS_OK) continue;
        if (mod->type != POINTIO_TYPE_DIGITAL_OUTPUT &&
            mod->type != POINTIO_TYPE_SAFETY_OUTPUT) continue;

        uint8_t needed = (mod->num_channels + 7) / 8;
        if (offset + needed > data_len) return -1;

        memcpy(mod->output_data, &data[offset], needed);
        offset += needed;
    }

    return 0;
}
