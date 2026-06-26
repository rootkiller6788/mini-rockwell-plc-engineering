/**
 * @file pointio_scanner.h
 * @brief I/O scanning engine declarations.
 *
 * Level: L3 Engineering Structures + L5 Algorithms
 */

#ifndef POINTIO_SCANNER_H
#define POINTIO_SCANNER_H

#include "pointio_types.h"
#include "pointio_module.h"
#include "pointio_connection.h"

/* Scanner lifecycle */
int pointio_scanner_start(uint32_t interval_us);
int pointio_scanner_stop(pointio_chassis_t *chassis);
int pointio_scanner_is_running(void);

/* Main scan cycle */
int pointio_scanner_scan_cycle(pointio_chassis_t *chassis,
    pointio_connection_pool_t *pool, uint64_t now_us);

/* Single-module operations */
int pointio_scanner_update_module_input(pointio_chassis_t *chassis, uint8_t slot);
int pointio_scanner_signal_cos(pointio_chassis_t *chassis, uint8_t slot);

/* Rack-optimized connections */
int pointio_scanner_build_rack_optimized_input(const pointio_chassis_t *chassis,
    uint8_t *buffer, uint16_t buffer_size, uint16_t *data_len);
int pointio_scanner_distribute_rack_optimized_output(pointio_chassis_t *chassis,
    const uint8_t *data, uint16_t data_len);

/* Timing analysis */
int pointio_scanner_get_timing(double *avg_scan_us, double *max_scan_us,
    double *min_scan_us, uint32_t *overruns);
int pointio_scanner_analyze_jitter(double *max_jitter_us,
    double *rms_jitter_us, int *is_excessive);

#endif /* POINTIO_SCANNER_H */
