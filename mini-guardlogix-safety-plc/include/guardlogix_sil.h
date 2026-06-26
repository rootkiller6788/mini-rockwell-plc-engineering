/**
 * guardlogix_sil.h ? SIL Calculation Engine per IEC 61508 / IEC 61511
 *
 * Covers L4: Engineering Laws ? IEC 61508-6 Annex B PFDavg formulas for
 * various architectures (1oo1, 1oo2, 2oo3, 1oo2D).
 *
 * Covers L5: Algorithms ? PFDavg computation, architectural constraint
 * verification, Safe Failure Fraction analysis, Markov chain reliability
 * modeling for safety instrumented functions (SIF).
 *
 * Key Formulas:
 *
 *   1oo1 PFDavg = lambda_du * T_pt / 2
 *
 *   1oo2 PFDavg = (lambda_du)^2 * (T_pt)^2 / 3 + beta * lambda_du * T_pt / 2
 *
 *   2oo3 PFDavg = (lambda_du)^2 * (T_pt)^2 + beta * lambda_du * T_pt / 2
 *
 *   1oo2D PFDavg = (1-beta) * lambda_du^2 * T_pt^2 / 3
 *                 + beta * lambda_du * T_pt / 2
 *                 + lambda_du * (1-DC) * MTTR
 *
 * Reference: IEC 61508-6:2010 Annex B, IEC 61511-1:2016 section 11
 */

#ifndef GUARDLOGIX_SIL_H
#define GUARDLOGIX_SIL_H

#include <stdint.h>
#include <math.h>
#include "guardlogix_safety.h"

typedef enum {
    GLX_ARCH_1OO1  = 0,
    GLX_ARCH_1OO2  = 1,
    GLX_ARCH_2OO2  = 2,
    GLX_ARCH_2OO3  = 3,
    GLX_ARCH_1OO2D = 4,
    GLX_ARCH_2OO4D = 5
} glx_sif_architecture_t;

typedef enum {
    GLX_HFT_0 = 0,
    GLX_HFT_1 = 1,
    GLX_HFT_2 = 2
} glx_hft_level_t;

typedef enum {
    GLX_COMPONENT_TYPE_A = 0,
    GLX_COMPONENT_TYPE_B = 1
} glx_component_type_t;

typedef enum {
    GLX_CAT_B  = 0,
    GLX_CAT_1  = 1,
    GLX_CAT_2  = 2,
    GLX_CAT_3  = 3,
    GLX_CAT_4  = 4
} glx_iso13849_category_t;

typedef struct {
    double lambda_total;
    double lambda_safe;
    double lambda_dd;
    double lambda_du;
    double mtbf;
    double mttr;
} glx_failure_rates_t;

typedef struct {
    glx_sif_architecture_t  architecture;
    glx_component_type_t    component_type;
    glx_hft_level_t         hft;
    glx_failure_rates_t     sensor_rates;
    glx_failure_rates_t     logic_rates;
    glx_failure_rates_t     actuator_rates;
    double                  beta;
    double                  beta_d;
    double                  dc_sensor;
    double                  dc_logic;
    double                  dc_actuator;
    double                  proof_test_coverage;
    uint32_t                proof_test_interval_h;
    double                  mission_time_h;
} glx_sif_model_t;

typedef struct {
    double                  pfd_avg;
    glx_sil_level_t         achieved_sil;
    glx_sil_level_t         hft_limited_sil;
    double                  sff;
    double                  rrf;
    double                  spurious_trip_rate;
    uint8_t                 sil_achieved;
    uint8_t                 hft_compliant;
    double                  pfd_margin;
} glx_sil_verification_t;

/* Architecture-specific PFDavg formulas */
double glx_pfdavg_1oo1(double lambda_du, double t_pt, double ptc);
double glx_pfdavg_1oo2(double lambda_du, double beta, double t_pt, double ptc);
double glx_pfdavg_2oo3(double lambda_du, double beta, double t_pt, double ptc);
double glx_pfdavg_1oo2d(double lambda_du, double beta, double beta_d,
                         double dc, double t_pt, double mttr, double diag_ti);
double glx_pfdavg_2oo2(double lambda_du, double beta, double t_pt);

/* SFF and architectural constraints */
double glx_compute_sff(const glx_failure_rates_t *rates);
glx_sil_level_t glx_architectural_sil_limit(glx_component_type_t comp_type,
                                              glx_hft_level_t hft, double sff);

/* SIF verification */
int glx_verify_sif_sil(const glx_sif_model_t *model,
                        glx_sil_level_t target, glx_sil_verification_t *result);
double glx_compute_rrf(double pfd_avg);
glx_sil_level_t glx_pfdavg_to_sil(double pfd_avg);
double glx_sil_to_pfdavg_threshold(glx_sil_level_t sil);
int glx_invert_proof_test_interval(const glx_sif_model_t *model,
                                    double target_pfd, uint32_t *t_pt_out);
double glx_spurious_trip_rate(glx_sif_architecture_t architecture,
                               double lambda_s, double mttr);
double glx_estimate_beta(double base_beta, uint8_t diversity_flags);
glx_pl_level_t glx_sil_to_pl(glx_sil_level_t sil);

#endif /* GUARDLOGIX_SIL_H */
