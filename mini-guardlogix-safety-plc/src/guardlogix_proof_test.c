/**
 * guardlogix_proof_test.c -- Proof Test Management
 *
 * Knowledge points:
 *   L1: Proof test definition per IEC 61511 (periodic test to reveal
 *       dangerous undetected failures)
 *   L2: Proof test coverage (PTC) -- fraction of DU failures detected
 *       by the proof test
 *   L4: PFDavg sensitivity to proof test interval
 *   L5: Optimal proof test interval computation considering cost
 *   L5: Partial stroke testing for valves (PST)
 *   L6: Proof test scheduling in continuous operation
 *
 * Reference: IEC 61511-1:2016 section 16 (Proof testing)
 *            ISA-TR84.00.03 (Proof test guidance)
 */

#include "guardlogix_safety.h"
#include "guardlogix_sil.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAX_PROOF_TEST_RECORDS 50
#define DEFAULT_PARTIAL_STROKE_INTERVAL_H 720 /* 30 days */

typedef struct {
    uint32_t timestamp;         /* When the test was performed */
    uint32_t interval_hours;    /* Interval since last test */
    double   coverage_factor;   /* Coverage achieved (0-1) */
    uint8_t  test_passed;       /* 1 = passed */
    uint8_t  test_type;         /* 0=full, 1=partial stroke */
    uint32_t anomalies_found;   /* Number of anomalies detected */
} glx_proof_test_record_t;

typedef struct {
    uint8_t     test_count;
    glx_proof_test_record_t records[MAX_PROOF_TEST_RECORDS];
    uint32_t    next_due_timestamp;
    uint32_t    configured_interval_h;
    double      configured_coverage;
    uint8_t     partial_stroke_enabled;
    uint32_t    partial_stroke_interval_h;
    double      partial_stroke_coverage;
    uint8_t     overdue;
} glx_proof_test_manager_t;

/**
 * Initialize the proof test manager.
 *
 * @param ptm              Proof test manager.
 * @param interval_hours   Required proof test interval in hours.
 * @param coverage         Expected proof test coverage (0.0-1.0).
 * @return                 0 on success.
 */
int glx_proof_test_manager_init(glx_proof_test_manager_t *ptm,
                                 uint32_t interval_hours,
                                 double coverage)
{
    if (!ptm) return -1;
    if (interval_hours == 0) return -1;
    if (coverage < 0.0 || coverage > 1.0) return -1;

    memset(ptm, 0, sizeof(*ptm));
    ptm->configured_interval_h = interval_hours;
    ptm->configured_coverage = coverage;
    ptm->next_due_timestamp = interval_hours * 3600; /* Convert to seconds */
    ptm->overdue = 0;

    return 0;
}

/**
 * Enable partial stroke testing for valves.
 *
 * Partial stroke testing moves a valve partially (typically 10-20%)
 * and returns it to its normal position. This reveals certain failure
 * modes without interrupting the process. The coverage is lower than
 * a full proof test but can be performed much more frequently.
 *
 * @param ptm             Proof test manager.
 * @param enabled         1 = enable partial stroke testing.
 * @param interval_hours  Interval between partial stroke tests.
 * @param coverage        Coverage factor (typically 0.5-0.7 for PST).
 * @return                0 on success.
 */
int glx_proof_test_enable_partial_stroke(glx_proof_test_manager_t *ptm,
                                          int enabled,
                                          uint32_t interval_hours,
                                          double coverage)
{
    if (!ptm) return -1;
    ptm->partial_stroke_enabled = (uint8_t)enabled;
    ptm->partial_stroke_interval_h = interval_hours;
    ptm->partial_stroke_coverage = coverage;
    return 0;
}

/**
 * Record a completed proof test.
 *
 * Updates the test history, resets the due timer, and tracks
 * the interval since the previous test.
 *
 * @param ptm              Proof test manager.
 * @param timestamp        Current time (Unix epoch seconds).
 * @param test_type        0 = full proof test, 1 = partial stroke.
 * @param anomalies_found  Number of anomalies detected.
 * @param passed           1 = test passed (no dangerous failures found).
 * @return                 0 on success.
 */
int glx_proof_test_record(glx_proof_test_manager_t *ptm,
                           uint32_t timestamp,
                           uint8_t test_type,
                           uint32_t anomalies_found,
                           int passed)
{
    if (!ptm) return -1;
    if (ptm->test_count >= MAX_PROOF_TEST_RECORDS) return -1;

    uint8_t idx = ptm->test_count;

    /* Compute interval since last test */
    if (idx > 0) {
        uint32_t prev_time = ptm->records[idx - 1].timestamp;
        ptm->records[idx].interval_hours =
            (timestamp - prev_time) / 3600;
    } else {
        ptm->records[idx].interval_hours =
            ptm->configured_interval_h;
    }

    ptm->records[idx].timestamp = timestamp;
    ptm->records[idx].test_type = test_type;
    ptm->records[idx].anomalies_found = anomalies_found;
    ptm->records[idx].test_passed = (uint8_t)passed;

    if (test_type == 0) {
        /* Full proof test coverage */
        ptm->records[idx].coverage_factor = ptm->configured_coverage;
        /* Reset due timer for full test */
        ptm->next_due_timestamp = timestamp +
            ptm->configured_interval_h * 3600;
    } else {
        /* Partial stroke test coverage */
        ptm->records[idx].coverage_factor = ptm->partial_stroke_coverage;
        /* Partial stroke does not reset the full proof test timer */
    }

    ptm->test_count++;
    ptm->overdue = 0;

    return 0;
}

/**
 * Check if a proof test is currently overdue.
 *
 * @param ptm              Proof test manager.
 * @param current_time     Current time (Unix epoch seconds).
 * @return                 0 = on schedule, -1 = overdue.
 */
int glx_proof_test_is_overdue(const glx_proof_test_manager_t *ptm,
                               uint32_t current_time)
{
    if (!ptm) return -1;
    if (current_time > ptm->next_due_timestamp) return -1;
    return 0;
}

/**
 * Compute the effective proof test coverage considering partial strokes.
 *
 * If partial stroke testing is enabled with coverage C_pst at interval
 * I_pst, the effective coverage for the full proof test interval I_full is:
 *
 *   C_eff = 1 - (1 - C_full) * (1 - C_pst)^(I_full / I_pst)
 *
 * This accounts for the cumulative benefit of frequent partial strokes.
 *
 * @param ptm Proof test manager.
 * @return    Effective coverage factor (0.0 to 1.0).
 */
double glx_proof_test_effective_coverage(const glx_proof_test_manager_t *ptm)
{
    if (!ptm) return 0.0;

    double c_full = ptm->configured_coverage;
    double c_eff = c_full;

    if (ptm->partial_stroke_enabled &&
        ptm->partial_stroke_interval_h > 0) {

        double n_partial = (double)ptm->configured_interval_h /
                          (double)ptm->partial_stroke_interval_h;
        double c_pst = ptm->partial_stroke_coverage;

        /* Cumulative effect of n partial strokes */
        c_eff = 1.0 - (1.0 - c_full) *
                pow(1.0 - c_pst, n_partial);
    }

    return c_eff;
}

/**
 * Get the time remaining until the next proof test is due.
 *
 * @param ptm          Proof test manager.
 * @param current_time Current time (Unix epoch seconds).
 * @return             Seconds until due, or 0 if overdue.
 */
uint32_t glx_proof_test_time_remaining(const glx_proof_test_manager_t *ptm,
                                        uint32_t current_time)
{
    if (!ptm) return 0;
    if (current_time >= ptm->next_due_timestamp) return 0;
    return ptm->next_due_timestamp - current_time;
}

/**
 * Compute the recommended proof test interval based on SIF model
 * and operational constraints.
 *
 * Considers:
 *   1. Minimum interval from SIL requirement (PFDavg constraint)
 *   2. Maximum interval from operational policy (typically 5 years)
 *   3. Regulatory requirements (annual for some jurisdictions)
 *
 * @param model        SIF model.
 * @param target_sil   Target SIL level.
 * @param min_days     Minimum allowed interval in days (e.g., 90).
 * @param max_days     Maximum allowed interval in days (e.g., 1825 = 5y).
 * @param rec_hours    Output: recommended interval in hours.
 * @return             0 on success.
 */
int glx_proof_test_recommend_interval(const glx_sif_model_t *model,
                                       glx_sil_level_t target_sil,
                                       uint32_t min_days,
                                       uint32_t max_days,
                                       uint32_t *rec_hours)
{
    if (!model || !rec_hours) return -1;

    /* Convert days to hours */
    uint32_t min_hours = min_days * 24;
    uint32_t max_hours = max_days * 24;

    /* Get minimum interval from SIL requirement */
    double target_pfd = glx_sil_to_pfdavg_threshold(target_sil);
    uint32_t sil_min_hours = 0;
    int ret = glx_invert_proof_test_interval(model, target_pfd,
                                              &sil_min_hours);
    if (ret != 0) {
        /* Cannot achieve target -- use shortest possible */
        sil_min_hours = min_hours;
    }

    /* Choose the more conservative interval */
    if (sil_min_hours > max_hours) {
        *rec_hours = max_hours; /* Risk: may not meet SIL */
        return -1;
    }

    if (sil_min_hours < min_hours) {
        *rec_hours = min_hours;
    } else {
        *rec_hours = sil_min_hours;
    }

    return 0;
}

/**
 * Compute the proof test interval impact on PFDavg.
 *
 * Shows how PFDavg changes as T_pt varies, for sensitivity analysis.
 * Returns the PFDavg ratio compared to the design-basis PFDavg.
 *
 * @param model     SIF model with design-basis T_pt.
 * @param new_t_pt  New proof test interval to evaluate.
 * @param ratio_out Output: ratio of new PFDavg to design-basis PFDavg.
 * @return          0 on success.
 */
int glx_proof_test_sensitivity(const glx_sif_model_t *model,
                                uint32_t new_t_pt,
                                double *ratio_out)
{
    if (!model || !ratio_out) return -1;

    glx_sif_model_t modified = *model;
    glx_sil_verification_t base_result, new_result;

    /* Compute design-basis PFDavg */
    glx_verify_sif_sil(model, GLX_SIL_1, &base_result);

    /* Compute PFDavg at new interval */
    modified.proof_test_interval_h = new_t_pt;
    glx_verify_sif_sil(&modified, GLX_SIL_1, &new_result);

    if (base_result.pfd_avg > 0.0) {
        *ratio_out = new_result.pfd_avg / base_result.pfd_avg;
    } else {
        *ratio_out = 1.0;
    }

    return 0;
}
