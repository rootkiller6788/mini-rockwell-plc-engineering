/**
 * guardlogix_sil.c -- SIL Verification Engine per IEC 61508 / IEC 61511
 *
 * Implements all PFDavg formulas for standard SIF architectures:
 *   1oo1, 1oo2, 2oo2, 2oo3, 1oo2D
 *
 * Also implements Safe Failure Fraction (SFF) computation,
 * architectural constraint verification per IEC 61508-2 Table 3/4,
 * spurious trip rate estimation, and beta factor estimation.
 *
 * Knowledge points:
 *   L4: IEC 61508-6 Annex B PFDavg formulas
 *   L4: Architectural constraints (HFT vs SFF per component type)
 *   L4: ISO 13849-1 Category/PL mapping
 *   L5: Binary search for inverse proof test interval
 *   L5: Diversity-based beta factor estimation
 */

#include "guardlogix_sil.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ==========================================================================
 * PFDavg Formulas per IEC 61508-6 Annex B
 * ========================================================================== */

double glx_pfdavg_1oo1(double lambda_du, double t_pt, double ptc)
{
    if (lambda_du <= 0.0 || t_pt <= 0.0) return 0.0;
    if (ptc < 0.0) ptc = 0.0;
    if (ptc > 1.0) ptc = 1.0;

    /* PFDavg = lambda_du * T_pt / 2
     * With imperfect proof test coverage:
     * PFDavg = lambda_du * [ptc * T_pt/2 + (1-ptc) * lifetime/2]
     * For simplicity, we use the standard formula without lifetime term.
     */
    double base = lambda_du * t_pt / 2.0;
    double imperfect = lambda_du * (1.0 - ptc) * t_pt / 2.0;
    return base + imperfect * (1.0 - ptc);
}

double glx_pfdavg_1oo2(double lambda_du, double beta, double t_pt, double ptc)
{
    if (lambda_du <= 0.0 || t_pt <= 0.0) return 0.0;

    /* Clamp beta to valid range */
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;
    if (ptc < 0.0) ptc = 0.0;
    if (ptc > 1.0) ptc = 1.0;

    /* PFDavg = (1-beta) * lambda_du^2 * T_pt^2 / 3
     *         + beta * lambda_du * T_pt / 2
     *
     * With proof test coverage ptc:
     *   Independent term scaled by (1-ptc) because undetected failures
     *   remaining after proof test contribute to the quadratic term.
     */
    double independent = (1.0 - beta) * lambda_du * lambda_du
                         * t_pt * t_pt / 3.0;
    double common_cause = beta * lambda_du * t_pt / 2.0;

    /* Imperfect proof test: fraction (1-ptc) of DU failures remain */
    double ptc_factor = 1.0 + (1.0 - ptc);
    return independent * ptc_factor + common_cause;
}

double glx_pfdavg_2oo3(double lambda_du, double beta, double t_pt, double ptc)
{
    if (lambda_du <= 0.0 || t_pt <= 0.0) return 0.0;
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;
    if (ptc < 0.0) ptc = 0.0;
    if (ptc > 1.0) ptc = 1.0;

    /* PFDavg_2oo3 = (lambda_du)^2 * T^2 / 1
     *              + beta * lambda_du * T / 2
     *
     * The independent failure term for 2oo3 is larger than 1oo2 because
     * two out of three channels must fail to cause a system failure.
     * This is approximately 3x the 1oo2 independent term:
     *   3 * (lambda_du * T)^2 / 3 = (lambda_du * T)^2
     */
    double independent = lambda_du * lambda_du * t_pt * t_pt;
    double common_cause = beta * lambda_du * t_pt / 2.0;

    double ptc_factor = 1.0 + (1.0 - ptc);
    return independent * ptc_factor + common_cause;
}

double glx_pfdavg_1oo2d(double lambda_du, double beta, double beta_d,
                         double dc, double t_pt, double mttr, double diag_ti)
{
    if (lambda_du <= 0.0 || t_pt <= 0.0) return 0.0;
    if (mttr <= 0.0) mttr = 8.0; /* Default MTTR */
    if (diag_ti <= 0.0) diag_ti = 1.0; /* Default diagnostic test interval */

    /* Clamp parameters */
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;
    if (beta_d < 0.0) beta_d = 0.0;
    if (beta_d > 1.0) beta_d = 1.0;
    if (dc < 0.0) dc = 0.0;
    if (dc > 1.0) dc = 1.0;

    /* 1oo2D PFDavg formula:
     * PFDavg = (1-beta) * (1-DC)^2 * lambda_du^2 * T_pt^2 / 3
     *        + beta * lambda_du * T_pt / 2
     *        + lambda_du * (1-DC) * (MTTR + diag_ti/2)
     *
     * The third term accounts for dangerous detected failures that
     * are repaired within MTTR. The diagnostic test interval diag_ti
     * adds latency to detection of dangerous failures.
     */
    double independent = (1.0 - beta) * (1.0 - dc) * (1.0 - dc)
                         * lambda_du * lambda_du * t_pt * t_pt / 3.0;
    double common_cause = beta * lambda_du * t_pt / 2.0;
    double repair_term = lambda_du * (1.0 - dc) * (mttr + diag_ti / 2.0);

    return independent + common_cause + repair_term;
}

double glx_pfdavg_2oo2(double lambda_du, double beta, double t_pt)
{
    if (lambda_du <= 0.0 || t_pt <= 0.0) return 0.0;
    if (beta < 0.0) beta = 0.0;
    if (beta > 1.0) beta = 1.0;

    /* 2oo2: Both channels must be available.
     * PFDavg = 2 * (1-beta) * lambda_du * T_pt / 2
     *        + beta * lambda_du * T_pt / 2
     *
     * This architecture has NO fault tolerance for safety --
     * failure of either channel causes system failure.
     * It is used where spurious trip rate must be minimized.
     */
    double independent = 2.0 * (1.0 - beta) * lambda_du * t_pt / 2.0;
    double common_cause = beta * lambda_du * t_pt / 2.0;
    return independent + common_cause;
}

/* ==========================================================================
 * Safe Failure Fraction (SFF) per IEC 61508-2 section 7.4.3
 * ========================================================================== */

double glx_compute_sff(const glx_failure_rates_t *rates)
{
    if (!rates) return 0.0;

    double total = rates->lambda_safe + rates->lambda_dd + rates->lambda_du;

    /* Avoid division by zero */
    if (total <= 0.0) return 1.0;

    /* SFF = (safe + dangerous detected) / total failures
     *     = (lambda_s + lambda_dd) / (lambda_s + lambda_dd + lambda_du)
     */
    double numerator = rates->lambda_safe + rates->lambda_dd;
    return numerator / total;
}

/* ==========================================================================
 * Architectural Constraints per IEC 61508-2 Table 3/4
 * ========================================================================== */

glx_sil_level_t glx_architectural_sil_limit(glx_component_type_t comp_type,
                                              glx_hft_level_t hft,
                                              double sff)
{
    /* Clamp SFF */
    if (sff < 0.0) sff = 0.0;
    if (sff > 1.0) sff = 1.0;

    if (comp_type == GLX_COMPONENT_TYPE_A) {
        /* Type A: failure modes well-defined */
        if (sff >= 0.99) {
            if (hft >= GLX_HFT_2) return GLX_SIL_4;
            if (hft >= GLX_HFT_1) return GLX_SIL_4;
            return GLX_SIL_3; /* HFT 0 */
        } else if (sff >= 0.90) {
            if (hft >= GLX_HFT_2) return GLX_SIL_4;
            if (hft >= GLX_HFT_1) return GLX_SIL_4;
            return GLX_SIL_3; /* HFT 0 */
        } else if (sff >= 0.60) {
            if (hft >= GLX_HFT_2) return GLX_SIL_4;
            if (hft >= GLX_HFT_1) return GLX_SIL_3;
            return GLX_SIL_2; /* HFT 0 */
        } else {
            /* SFF < 60% */
            if (hft >= GLX_HFT_2) return GLX_SIL_3;
            if (hft >= GLX_HFT_1) return GLX_SIL_2;
            return GLX_SIL_1; /* HFT 0 */
        }
    } else {
        /* Type B: failure modes not completely known (microprocessors) */
        if (sff >= 0.99) {
            if (hft >= GLX_HFT_2) return GLX_SIL_4;
            if (hft >= GLX_HFT_1) return GLX_SIL_4;
            return GLX_SIL_3; /* HFT 0 */
        } else if (sff >= 0.90) {
            if (hft >= GLX_HFT_2) return GLX_SIL_3;
            if (hft >= GLX_HFT_1) return GLX_SIL_3;
            return GLX_SIL_2; /* HFT 0 */
        } else if (sff >= 0.60) {
            if (hft >= GLX_HFT_2) return GLX_SIL_3;
            if (hft >= GLX_HFT_1) return GLX_SIL_2;
            return GLX_SIL_1; /* HFT 0 */
        } else {
            /* SFF < 60% */
            if (hft >= GLX_HFT_2) return GLX_SIL_2;
            if (hft >= GLX_HFT_1) return GLX_SIL_1;
            return GLX_SIL_NONE; /* HFT 0 */
        }
    }
}

/* ==========================================================================
 * Complete SIF Verification
 * ========================================================================== */

int glx_verify_sif_sil(const glx_sif_model_t *model,
                        glx_sil_level_t target,
                        glx_sil_verification_t *result)
{
    if (!model || !result) return -1;

    /* Zero the result structure */
    memset(result, 0, sizeof(*result));

    double t_pt = (double)model->proof_test_interval_h;
    if (t_pt <= 0.0) t_pt = 8760.0; /* Default 1 year */

    /* Compute PFDavg for each subsystem based on architecture */
    double pfd_sensor = 0.0, pfd_logic = 0.0, pfd_actuator = 0.0;

    /* Sensor subsystem */
    switch (model->architecture) {
    case GLX_ARCH_1OO1:
        pfd_sensor = glx_pfdavg_1oo1(model->sensor_rates.lambda_du,
                                      t_pt, model->proof_test_coverage);
        pfd_logic = glx_pfdavg_1oo1(model->logic_rates.lambda_du,
                                     t_pt, model->proof_test_coverage);
        pfd_actuator = glx_pfdavg_1oo1(model->actuator_rates.lambda_du,
                                        t_pt, model->proof_test_coverage);
        break;
    case GLX_ARCH_1OO2:
        pfd_sensor = glx_pfdavg_1oo2(model->sensor_rates.lambda_du,
                                      model->beta, t_pt,
                                      model->proof_test_coverage);
        pfd_logic = glx_pfdavg_1oo2(model->logic_rates.lambda_du,
                                     model->beta, t_pt,
                                     model->proof_test_coverage);
        pfd_actuator = glx_pfdavg_1oo2(model->actuator_rates.lambda_du,
                                        model->beta, t_pt,
                                        model->proof_test_coverage);
        break;
    case GLX_ARCH_2OO3:
        pfd_sensor = glx_pfdavg_2oo3(model->sensor_rates.lambda_du,
                                      model->beta, t_pt,
                                      model->proof_test_coverage);
        pfd_logic = glx_pfdavg_2oo3(model->logic_rates.lambda_du,
                                     model->beta, t_pt,
                                     model->proof_test_coverage);
        pfd_actuator = glx_pfdavg_2oo3(model->actuator_rates.lambda_du,
                                        model->beta, t_pt,
                                        model->proof_test_coverage);
        break;
    case GLX_ARCH_1OO2D:
        pfd_sensor = glx_pfdavg_1oo2d(model->sensor_rates.lambda_du,
                                       model->beta, model->beta_d,
                                       model->dc_sensor, t_pt,
                                       model->sensor_rates.mttr, 1.0);
        pfd_logic = glx_pfdavg_1oo2d(model->logic_rates.lambda_du,
                                      model->beta, model->beta_d,
                                      model->dc_logic, t_pt,
                                      model->logic_rates.mttr, 1.0);
        pfd_actuator = glx_pfdavg_1oo2d(model->actuator_rates.lambda_du,
                                         model->beta, model->beta_d,
                                         model->dc_actuator, t_pt,
                                         model->actuator_rates.mttr, 1.0);
        break;
    case GLX_ARCH_2OO2:
        pfd_sensor = glx_pfdavg_2oo2(model->sensor_rates.lambda_du,
                                      model->beta, t_pt);
        pfd_logic = glx_pfdavg_2oo2(model->logic_rates.lambda_du,
                                     model->beta, t_pt);
        pfd_actuator = glx_pfdavg_2oo2(model->actuator_rates.lambda_du,
                                        model->beta, t_pt);
        break;
    default:
        /* 2OO4D or unknown -- default to 1oo2D */
        pfd_sensor = glx_pfdavg_1oo2d(model->sensor_rates.lambda_du,
                                       model->beta, model->beta_d,
                                       model->dc_sensor, t_pt,
                                       model->sensor_rates.mttr, 1.0);
        pfd_logic = glx_pfdavg_1oo2d(model->logic_rates.lambda_du,
                                      model->beta, model->beta_d,
                                      model->dc_logic, t_pt,
                                      model->logic_rates.mttr, 1.0);
        pfd_actuator = glx_pfdavg_1oo2d(model->actuator_rates.lambda_du,
                                         model->beta, model->beta_d,
                                         model->dc_actuator, t_pt,
                                         model->actuator_rates.mttr, 1.0);
        break;
    }

    /* Total PFDavg = sum of subsystems per IEC 61511-1 */
    result->pfd_avg = pfd_sensor + pfd_logic + pfd_actuator;

    /* Classify SIL from PFDavg */
    result->achieved_sil = glx_pfdavg_to_sil(result->pfd_avg);

    /* Compute SFF for the logic solver (most critical) */
    result->sff = glx_compute_sff(&model->logic_rates);

    /* Architectural constraint check on logic solver */
    result->hft_limited_sil = glx_architectural_sil_limit(
        model->component_type, model->hft, result->sff);

    /* Compute RRF */
    result->rrf = glx_compute_rrf(result->pfd_avg);

    /* Spurious trip rate */
    result->spurious_trip_rate = glx_spurious_trip_rate(
        model->architecture, model->logic_rates.lambda_safe,
        model->logic_rates.mttr);

    /* SIL verification result */
    if (result->achieved_sil >= target &&
        result->hft_limited_sil >= target) {
        result->sil_achieved = 1;
        result->hft_compliant = 1;
    } else {
        result->sil_achieved = 0;
        result->hft_compliant =
            (result->hft_limited_sil >= target) ? 1 : 0;
    }

    /* PFD margin */
    double target_pfd = glx_sil_to_pfdavg_threshold(target);
    if (result->pfd_avg > 0.0) {
        result->pfd_margin = target_pfd / result->pfd_avg;
    } else {
        result->pfd_margin = 1.0e6; /* Effectively infinite */
    }

    return 0;
}

double glx_compute_rrf(double pfd_avg)
{
    if (pfd_avg <= 0.0) return 1.0e10; /* Effectively infinite */
    if (pfd_avg >= 1.0) return 1.0;    /* No risk reduction */

    return 1.0 / pfd_avg;
}

glx_sil_level_t glx_pfdavg_to_sil(double pfd_avg)
{
    if (pfd_avg < 1.0e-4) return GLX_SIL_4;
    if (pfd_avg < 1.0e-3) return GLX_SIL_3;
    if (pfd_avg < 1.0e-2) return GLX_SIL_2;
    if (pfd_avg < 1.0e-1) return GLX_SIL_1;
    return GLX_SIL_NONE;
}

double glx_sil_to_pfdavg_threshold(glx_sil_level_t sil)
{
    switch (sil) {
    case GLX_SIL_1: return 1.0e-1;
    case GLX_SIL_2: return 1.0e-2;
    case GLX_SIL_3: return 1.0e-3;
    case GLX_SIL_4: return 1.0e-4;
    default:        return 1.0e-1;
    }
}

int glx_invert_proof_test_interval(const glx_sif_model_t *model,
                                    double target_pfd,
                                    uint32_t *t_pt_out)
{
    if (!model || !t_pt_out || target_pfd <= 0.0) return -1;

    glx_sif_model_t mutable_model = *model;

    /* Binary search for T_pt */
    uint32_t lo = 1;
    uint32_t hi = 350400; /* 40 years */
    uint32_t mid;
    glx_sil_verification_t result;

    /* Check if target achievable at T=1h */
    mutable_model.proof_test_interval_h = 1;
    glx_verify_sif_sil(&mutable_model, GLX_SIL_1, &result);
    if (result.pfd_avg > target_pfd) {
        *t_pt_out = 0;
        return -1; /* Cannot achieve target */
    }

    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        mutable_model.proof_test_interval_h = mid;
        glx_verify_sif_sil(&mutable_model, GLX_SIL_1, &result);

        if (result.pfd_avg <= target_pfd) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }

    *t_pt_out = lo;
    return 0;
}

double glx_spurious_trip_rate(glx_sif_architecture_t architecture,
                               double lambda_s, double mttr)
{
    if (lambda_s <= 0.0) return 0.0;
    if (mttr <= 0.0) mttr = 8.0;

    switch (architecture) {
    case GLX_ARCH_1OO1:
        /* Single channel: STR = lambda_s */
        return lambda_s;

    case GLX_ARCH_1OO2:
        /* Dual channel 1oo2: very low spurious trip rate because
         * both channels must spuriously trip. STR ~ 2 * lambda_s^2 * MTTR
         */
        return 2.0 * lambda_s * lambda_s * mttr;

    case GLX_ARCH_2OO2:
        /* Either channel trip causes spurious trip */
        return 2.0 * lambda_s;

    case GLX_ARCH_2OO3:
        /* Two out of three must spuriously trip */
        return 6.0 * lambda_s * lambda_s * mttr;

    case GLX_ARCH_1OO2D:
        /* Similar to 1oo2 but with diagnostics reducing spurious trips */
        return 1.5 * lambda_s * lambda_s * mttr;

    case GLX_ARCH_2OO4D:
        return 4.0 * lambda_s * lambda_s * mttr;

    default:
        return lambda_s;
    }
}

double glx_estimate_beta(double base_beta, uint8_t diversity_flags)
{
    if (base_beta < 0.0) base_beta = 0.05;
    if (base_beta > 1.0) base_beta = 0.20;

    /* Diversity measures per IEC 61508-6 Annex D:
     * Bit 0: Different technology    -> reduction factor 0.1
     * Bit 1: Different design team   -> reduction factor 0.2
     * Bit 2: Different physical loc  -> reduction factor 0.2
     * Bit 3: Functional diversity    -> reduction factor 0.3
     * Bit 4: Different manufacturer  -> reduction factor 0.1
     * Bit 5: Different compiler      -> reduction factor 0.15
     * Bit 6: Staggered testing       -> reduction factor 0.05
     * Bit 7: Enhanced diagnostics    -> reduction factor 0.05
     */
    double beta = base_beta;

    if (diversity_flags & 0x01) beta *= 0.90;
    if (diversity_flags & 0x02) beta *= 0.80;
    if (diversity_flags & 0x04) beta *= 0.80;
    if (diversity_flags & 0x08) beta *= 0.70;
    if (diversity_flags & 0x10) beta *= 0.90;
    if (diversity_flags & 0x20) beta *= 0.85;
    if (diversity_flags & 0x40) beta *= 0.95;
    if (diversity_flags & 0x80) beta *= 0.95;

    return beta;
}

glx_pl_level_t glx_sil_to_pl(glx_sil_level_t sil)
{
    switch (sil) {
    case GLX_SIL_1: return GLX_PL_C;
    case GLX_SIL_2: return GLX_PL_D;
    case GLX_SIL_3: return GLX_PL_E;
    case GLX_SIL_4: return GLX_PL_E;
    default:        return GLX_PL_A;
    }
}
