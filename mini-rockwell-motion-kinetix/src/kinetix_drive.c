/**
 * @file kinetix_drive.c
 * @brief Kinetix drive-specific models: DC bus, power ratings, thermal, STO safety.
 *
 * Level: L2-L4, L6, L7
 * Reference:
 *   - Rockwell Automation, Kinetix 5100/5300/5500/5700 User Manuals (2198-*)
 *   - Rockwell Automation, "Kinetix Motion Control Selection Guide" (KNX-SG001)
 *   - ISO 13849-1:2015, "Safety of machinery – Safety-related parts of control systems"
 *   - IEC 61508, "Functional Safety of E/E/PE Safety-Related Systems"
 *   - IEC 61800-5-2, "Adjustable Speed Electrical Power Drive Systems – Safety Requirements"
 *
 * Implements:
 *   L2: DC bus model, energy balance, shunt regulator
 *   L3: Drive power rating database, catalog matching
 *   L4: IEC 61508 SIL 3 / ISO 13849-1 PL e safety architecture
 *   L6: Drive sizing and selection, thermal management
 *   L7: Kinetix catalog integration (Studio 5000 Motion Group configuration)
 *
 * Alignment: RWTH Aachen, ISA/IEC 61508/61511, Tsinghua Motion Control
 */

#include "axis_types.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*===========================================================================
 * L3: Kinetix Drive Power Rating Database
 *===========================================================================*/

/**
 * @brief Kinetix 5100 drive power ratings.
 *
 * Key models:
 *   2198-E1004-ERS: 1 kW, 240V 1-ph / 480V 3-ph, 2.5A cont
 *   2198-E1007-ERS: 2 kW, 480V 3-ph, 5.0A cont
 *   2198-E1015-ERS: 4.4 kW, 480V 3-ph, 10.0A cont
 *   2198-E1020-ERS: 7.5 kW, 480V 3-ph, 14.0A cont
 *   2198-E1030-ERS: 11.0 kW, 480V 3-ph, 23.0A cont
 */
typedef struct {
    uint16_t catalog_id;
    const char *catalog_number;
    drive_power_rating_t rating;
} kinetix_drive_entry_t;

static const kinetix_drive_entry_t g_kinetix_5100_database[] = {
    { 0x5101, "2198-E1004-ERS", {
        2, 120, 2.5, 7.5, 1.0, 560.0, 50.0, 100.0, 400.0, 380.0 } },
    { 0x5102, "2198-E1007-ERS", {
        2, 240, 5.0, 15.0, 2.0, 560.0, 50.0, 100.0, 400.0, 380.0 } },
    { 0x5103, "2198-E1015-ERS", {
        2, 480, 10.0, 30.0, 4.4, 820.0, 100.0, 50.0, 400.0, 380.0 } },
    { 0x5104, "2198-E1020-ERS", {
        2, 480, 14.0, 42.0, 7.5, 940.0, 100.0, 50.0, 400.0, 380.0 } },
    { 0x5105, "2198-E1030-ERS", {
        2, 480, 23.0, 69.0, 11.0, 1410.0, 200.0, 33.0, 400.0, 380.0 } },
};

static const size_t g_kinetix_5100_count =
    sizeof(g_kinetix_5100_database) / sizeof(g_kinetix_5100_database[0]);

/**
 * @brief Kinetix 5300 drive power ratings (CIP Motion, single-axis).
 *
 * 2198-C1004-ERS: 1.0 kW, 480V 3-ph, 2.0A cont
 * 2198-C1007-ERS: 2.0 kW, 480V 3-ph, 5.0A cont
 * 2198-C1015-ERS: 4.3 kW, 480V 3-ph, 10.5A cont
 * 2198-C1020-ERS: 7.7 kW, 480V 3-ph, 16.0A cont
 * 2198-C1030-ERS: 10.0 kW, 480V 3-ph, 23.0A cont
 */
static const kinetix_drive_entry_t g_kinetix_5300_database[] = {
    { 0x5301, "2198-C1004-ERS", {
        2, 480, 2.0, 6.0, 1.0, 560.0, 50.0, 100.0, 420.0, 390.0 } },
    { 0x5302, "2198-C1007-ERS", {
        2, 480, 5.0, 15.0, 2.0, 560.0, 50.0, 100.0, 420.0, 390.0 } },
    { 0x5303, "2198-C1015-ERS", {
        2, 480, 10.5, 31.5, 4.3, 820.0, 100.0, 50.0, 420.0, 390.0 } },
    { 0x5304, "2198-C1020-ERS", {
        2, 480, 16.0, 48.0, 7.7, 940.0, 100.0, 50.0, 420.0, 390.0 } },
    { 0x5305, "2198-C1030-ERS", {
        2, 480, 23.0, 69.0, 10.0, 1410.0, 200.0, 33.0, 420.0, 390.0 } },
};

static const size_t g_kinetix_5300_count =
    sizeof(g_kinetix_5300_database) / sizeof(g_kinetix_5300_database[0]);

/**
 * @brief Kinetix 5700 (dual-axis) power ratings.
 *
 * 2198-D006-ERS3: Dual axis, 1.0 kW per axis, 480V 3-ph
 * 2198-D012-ERS3: Dual axis, 2.0 kW per axis
 * 2198-D020-ERS3: Dual axis, 4.0 kW per axis
 * 2198-D032-ERS3: Dual axis, 7.3 kW per axis
 * 2198-D057-ERS3: Dual axis, 14.6 kW per axis
 */
static const kinetix_drive_entry_t g_kinetix_5700_database[] = {
    { 0x5701, "2198-D006-ERS3", {
        2, 480, 2.5, 7.5, 2.0, 1120.0, 100.0, 50.0, 810.0, 770.0 } },
    { 0x5702, "2198-D012-ERS3", {
        2, 480, 5.3, 15.9, 4.0, 1120.0, 100.0, 50.0, 810.0, 770.0 } },
    { 0x5703, "2198-D020-ERS3", {
        2, 480, 9.5, 28.5, 8.0, 1880.0, 200.0, 33.0, 810.0, 770.0 } },
    { 0x5704, "2198-D032-ERS3", {
        2, 480, 14.5, 43.5, 14.6, 2820.0, 200.0, 33.0, 810.0, 770.0 } },
    { 0x5705, "2198-D057-ERS3", {
        2, 480, 27.0, 81.0, 29.2, 5640.0, 400.0, 16.5, 810.0, 770.0 } },
};

static const size_t g_kinetix_5700_count =
    sizeof(g_kinetix_5700_database) / sizeof(g_kinetix_5700_database[0]);

/**
 * @brief Look up a Kinetix drive entry by catalog ID.
 *
 * @param catalog_id   Drive catalog ID
 * @param[out] rating  Output power rating (if found)
 * @return             0 on success, -1 if not found
 */
int kinetix_drive_lookup(uint16_t catalog_id, drive_power_rating_t *rating)
{
    if (!rating) return -1;

    /* Search 5100 database */
    for (size_t i = 0; i < g_kinetix_5100_count; i++) {
        if (g_kinetix_5100_database[i].catalog_id == catalog_id) {
            memcpy(rating, &g_kinetix_5100_database[i].rating,
                   sizeof(drive_power_rating_t));
            return 0;
        }
    }

    /* Search 5300 database */
    for (size_t i = 0; i < g_kinetix_5300_count; i++) {
        if (g_kinetix_5300_database[i].catalog_id == catalog_id) {
            memcpy(rating, &g_kinetix_5300_database[i].rating,
                   sizeof(drive_power_rating_t));
            return 0;
        }
    }

    /* Search 5700 database */
    for (size_t i = 0; i < g_kinetix_5700_count; i++) {
        if (g_kinetix_5700_database[i].catalog_id == catalog_id) {
            memcpy(rating, &g_kinetix_5700_database[i].rating,
                   sizeof(drive_power_rating_t));
            return 0;
        }
    }

    return -1; /* Not found */
}

/*===========================================================================
 * L2: DC Bus Energy Balance Model
 *
 * The DC bus in a Kinetix drive is a capacitor bank that stores energy
 * from the rectified AC mains (or shared DC bus). During motor deceleration,
 * the motor acts as a generator and pumps energy back onto the DC bus.
 *
 * Bus voltage dynamics:
 *   C · dV_bus/dt = I_rectifier - I_motor
 *
 * If V_bus exceeds the shunt turn-on threshold, the shunt transistor
 * (braking chopper) activates, dissipating excess energy as heat.
 *
 * Energy stored: E = 0.5 · C · V²
 *
 * During deceleration, the kinetic energy of the load is recovered:
 *   E_kin = 0.5 · J · ω²
 *
 * The voltage rise is: ΔV = sqrt(V0² + 2·E_kin / C) - V0
 *
 * If ΔV would push V above the turn-on voltage, the shunt regulator
 * must dissipate the excess.
 *
 * Reference: Rockwell Automation, "Shunt Module Selection Guide"
 *===========================================================================*/

/**
 * @brief Compute DC bus voltage rise due to regenerative energy.
 *
 * @param bus_voltage       Initial DC bus voltage (V)
 * @param bus_capacitance   DC bus capacitance (μF)
 * @param inertia           Total system inertia (kg·cm²)
 * @param initial_velocity  Initial velocity before deceleration (rad/s)
 * @param[out] final_voltage Final DC bus voltage (V)
 * @param[out] shunt_energy Shunt energy to dissipate (J)
 * @return                  0 on success, -1 on invalid parameters
 */
int kinetix_compute_regenerative_energy(double bus_voltage,
                                         double bus_capacitance,
                                         double inertia,
                                         double initial_velocity,
                                         double *final_voltage,
                                         double *shunt_energy)
{
    if (bus_voltage <= 0.0 || bus_capacitance <= 0.0
        || inertia <= 0.0 || initial_velocity <= 0.0) {
        return -1;
    }

    /* Kinetic energy in joules */
    double J_kgm2 = inertia * 1e-4;  /* Convert kg·cm² to kg·m² */
    double E_kin = 0.5 * J_kgm2 * initial_velocity * initial_velocity;

    /* Capacitor energy: E_cap = 0.5 * C * V² */
    double C_farad = bus_capacitance * 1e-6;  /* μF → F */
    double E_cap_initial = 0.5 * C_farad * bus_voltage * bus_voltage;

    /* Total energy after regen: E_cap_final = E_cap_initial + E_kin */
    double E_cap_final = E_cap_initial + E_kin;

    /* Compute final bus voltage from energy */
    double V_final = sqrt(2.0 * E_cap_final / C_farad);

    if (final_voltage) *final_voltage = V_final;

    /* Shunt energy: only needed if V_final > shunt turn-on voltage
       For this calculation, assume shunt turn-on at 810V (5700 series) */
    double V_shunt_on = 810.0;
    if (V_final > V_shunt_on && shunt_energy) {
        double E_cap_shunt = 0.5 * C_farad * V_shunt_on * V_shunt_on;
        *shunt_energy = E_cap_final - E_cap_shunt;
        if (*shunt_energy < 0.0) *shunt_energy = 0.0;
    } else if (shunt_energy) {
        *shunt_energy = 0.0;
    }

    return 0;
}

/*===========================================================================
 * L6: Drive Sizing – Motor-Drive Matching
 *
 * Drive selection rules per Rockwell Kinetix Selection Guide:
 *   1. Drive continuous current ≥ Motor rated current
 *   2. Drive peak current ≥ Motor peak current (for ≥ 2 seconds)
 *   3. DC bus voltage class matches input voltage
 *   4. Drive output power ≥ Motor rated power (accounting for efficiency)
 *===========================================================================*/

/**
 * @brief Check if a drive can adequately power a specified motor.
 *
 * Verifies that the drive's current and power ratings meet or exceed
 * the motor's requirements.
 *
 * @param rating    Drive power rating
 * @param motor     Motor database entry
 * @param duty_cycle Motor duty cycle (0-1, e.g., 0.8 for 80% utilization)
 * @return          true if drive is adequately sized
 */
bool kinetix_check_drive_motor_match(const drive_power_rating_t *rating,
                                      const motor_database_t *motor,
                                      double duty_cycle)
{
    if (!rating || !motor) return false;
    if (duty_cycle <= 0.0 || duty_cycle > 1.0) duty_cycle = 1.0;

    /* Check continuous current */
    double required_cont_current = motor->rated_current_amps * duty_cycle;
    if (rating->output_current_cont < required_cont_current) {
        return false;
    }

    /* Check peak current */
    if (rating->output_current_peak < motor->peak_current_amps) {
        return false;
    }

    /* Check power */
    if (rating->output_power_kw < motor->rated_power_kw * duty_cycle) {
        return false;
    }

    return true;
}

/*===========================================================================
 * L6: Thermal Model – Motor Temperature Estimation
 *
 * First-order thermal model:
 *   C_th · dT/dt = P_loss - (T - T_amb) / R_th
 *
 * where:
 *   C_th = thermal capacitance (J/K)
 *   R_th = thermal resistance (K/W)
 *   P_loss = I²·R (copper losses, dominant at low speed)
 *          + V²·ω²/Kv²·R   (iron losses, dominant at high speed)
 *   T_amb = ambient temperature (typically 40°C for industrial)
 *   τ_th = C_th · R_th (thermal time constant, seconds)
 *
 * Discretized (Forward Euler):
 *   T(k+1) = T(k) + (Ts/τ_th) · (P_loss·R_th - (T(k) - T_amb))
 *
 * Reference: Hughes & Drury (2013), Ch.5 – Motor Rating and Selection
 *===========================================================================*/

/**
 * @brief Update the motor thermal model.
 *
 * Estimates winding temperature based on current and motor parameters.
 * This is a simplified model suitable for overload prediction.
 *
 * @param current_amps      Motor current (A RMS)
 * @param speed_rps         Motor speed (rev/s)
 * @param motor             Motor database
 * @param ambient_temp_c    Ambient temperature (°C)
 * @param prev_temp_c       Previous temperature estimate (°C)
 * @param Ts                Update interval (s)
 * @return                  New temperature estimate (°C)
 */
double kinetix_thermal_model_update(double current_amps, double speed_rps,
                                      const motor_database_t *motor,
                                      double ambient_temp_c,
                                      double prev_temp_c, double Ts)
{
    if (!motor || motor->thermal_time_const_s <= 0.0) {
        return ambient_temp_c;
    }

    /* Copper losses: I²·R */
    double P_copper = current_amps * current_amps * motor->resistance_ohms;

    /* Iron losses: approximate as proportional to speed */
    double P_iron = 0.02 * P_copper * (speed_rps / (motor->rated_speed_rpm / 60.0));

    double P_loss = P_copper + P_iron;

    /* Thermal resistance estimated from rated conditions */
    double R_th = (motor->max_winding_temp_c - ambient_temp_c)
                  / (motor->rated_current_amps * motor->rated_current_amps
                     * motor->resistance_ohms);

    if (R_th <= 0.0) return prev_temp_c;

    double tau = motor->thermal_time_const_s;
    double dT = (Ts / tau) * (P_loss * R_th - (prev_temp_c - ambient_temp_c));

    double new_temp = prev_temp_c + dT;

    /* Clamp to reasonable range */
    if (new_temp < ambient_temp_c) new_temp = ambient_temp_c;
    if (new_temp > 300.0) new_temp = 300.0; /* Physical limit */

    return new_temp;
}

/**
 * @brief Compute motor overload time at a given current.
 *
 * I²t overload model:
 *   t_trip = τ_th · ln( (I² - I_rated²) / (I² - I_max²) )
 *
 * This is the time for the motor to reach its maximum temperature
 * when operated at current I above rated.
 *
 * @param current_amps  Actual motor current (A RMS)
 * @param motor         Motor database
 * @return              Time to overload in seconds, INFINITY if no overload
 */
double kinetix_compute_overload_time(double current_amps,
                                       const motor_database_t *motor)
{
    if (!motor) return INFINITY;
    if (current_amps <= motor->rated_current_amps) return INFINITY;

    /* I²t thermal model: t_trip = τ · ln(I² / (I² - I_rated²))
     * Reference: IEC 60255-8 thermal overload relay model
     * Requires I > I_rated for finite trip time. */
    double I2 = current_amps * current_amps;
    double I_rated2 = motor->rated_current_amps * motor->rated_current_amps;

    if (I2 <= I_rated2) return INFINITY; /* No overload */

    double I2_diff = I2 - I_rated2;
    if (I2_diff <= 0.0) return INFINITY;

    double tau = motor->thermal_time_const_s;
    double t_trip = tau * log(I2 / I2_diff);

    return t_trip;
}

/*===========================================================================
 * L2: DC Bus Capacitor Health Estimation
 *
 * Electrolytic capacitors in the DC bus degrade over time due to:
 *   - Temperature (Arrhenius law: life halves every 10°C increase)
 *   - Ripple current (ESR heating)
 *   - Voltage stress
 *
 * The capacitance decreases and ESR increases as the capacitor ages.
 * When capacitance drops to ~80% of nominal, replacement is recommended.
 *===========================================================================*/

/**
 * @brief Estimate capacitor remaining life based on operating conditions.
 *
 * Uses the Arrhenius equation for electrolytic capacitor life:
 *
 *   L = L_rated × 2^((T_rated - T_actual) / 10) × (V_rated / V_actual)^n
 *
 * where n ≈ 2.5 for aluminum electrolytic capacitors.
 *
 * Reference: Nippon Chemi-Con, "Aluminum Electrolytic Capacitors Life Estimation"
 *
 * @param rated_life_h       Rated life at rated temp (hours, typically 5000-10000)
 * @param rated_temp_c       Rated temperature (°C, typically 105°C)
 * @param actual_temp_c      Actual operating temperature (°C)
 * @param rated_voltage      Rated voltage (V)
 * @param actual_voltage     Actual operating voltage (V)
 * @return                   Estimated remaining life (hours)
 */
double kinetix_capacitor_life_estimate(double rated_life_h,
                                         double rated_temp_c,
                                         double actual_temp_c,
                                         double rated_voltage,
                                         double actual_voltage)
{
    if (actual_temp_c >= rated_temp_c) return 0.0; /* Over temperature */

    /* Arrhenius temperature factor */
    double temp_factor = pow(2.0, (rated_temp_c - actual_temp_c) / 10.0);

    /* Voltage derating factor */
    double voltage_ratio = rated_voltage / actual_voltage;
    double voltage_factor = pow(voltage_ratio, 2.5);

    return rated_life_h * temp_factor * voltage_factor;
}

/*===========================================================================
 * L4: Safe Torque Off (STO) – Functional Safety Architecture
 *
 * Kinetix drives implement STO per IEC 61800-5-2, achieving:
 *   - SIL 3 per IEC 61508 (dual-channel with diagnostics)
 *   - PL e / Cat. 4 per ISO 13849-1
 *
 * Architecture: 1oo2 (1-out-of-2) with diagnostics
 *   - Two independent STO channels
 *   - Both must de-energize to remove torque
 *   - Diagnostics detect single-fault conditions
 *   - Safe state = no torque-producing energy to motor
 *
 * STO removes gate drive power from the IGBTs, preventing any
 * torque-producing current. The motor coasts to stop.
 *
 * For Safe Stop 1 (SS1), STO is preceded by a controlled deceleration
 * (Safe Stop 2 without STO gives monitored standstill).
 *===========================================================================*/

/**
 * @brief Validate STO circuit integrity.
 *
 * In a real Kinetix drive, this is performed by the safety monitoring
 * MCU that cross-checks both STO channels and the gate drive status.
 *
 * @param sto1_active       STO channel 1 active (24V removed = safe)
 * @param sto2_active       STO channel 2 active
 * @param gate_drive_disabled Gate drive disabled feedback
 * @return                  true if safety function is intact
 */
bool kinetix_sto_integrity_check(bool sto1_active, bool sto2_active,
                                   bool gate_drive_disabled)
{
    /* Both STO channels must agree (discrepancy = fault) */
    if (sto1_active != sto2_active) {
        return false; /* Channel mismatch – diagnostic fault */
    }

    /* If STO is active, gate drive must be disabled */
    if (sto1_active && !gate_drive_disabled) {
        return false; /* Gate drive should be off but isn't – dangerous fault */
    }

    /* If STO is not active, gate drive should be enabled */
    if (!sto1_active && gate_drive_disabled) {
        return false; /* Gate drive should be on but isn't */
    }

    return true;
}

/**
 * @brief Compute the probability of dangerous failure per hour (PFHd).
 *
 * For a 1oo2 architecture with diagnostics:
 *   PFHd ≈ λ_D¹ · λ_D² · T2   (λ_D = dangerous failure rate, T2 = diagnostic interval)
 *
 * Typical values for Kinetix STO:
 *   λ_D ≈ 1e-7 per hour (per channel)
 *   T2  = 100 ms (diagnostic interval)
 *
 * PFHd ≈ 1e-7 × 1e-7 × (100/3600000) ≈ 2.8e-18 / hour
 * This is well within SIL 3 requirement (PFHd < 1e-7/h).
 *
 * @param lambda_d1     Dangerous failure rate channel 1 (/hour)
 * @param lambda_d2     Dangerous failure rate channel 2 (/hour)
 * @param diag_interval Diagnostic test interval (hours)
 * @return              PFHd (probability of dangerous failure per hour)
 */
double kinetix_sto_pfhd_compute(double lambda_d1, double lambda_d2,
                                  double diag_interval)
{
    if (lambda_d1 <= 0.0 || lambda_d2 <= 0.0) return 1e-9; /* Assume best case */

    /* 1oo2 with diagnostics */
    double pfhd = lambda_d1 * lambda_d2 * diag_interval * 2.0;

    /* Common cause factor β (1% typical) */
    double beta = 0.01;
    double cc_pfhd = beta * (lambda_d1 + lambda_d2) / 2.0;

    return pfhd + cc_pfhd;
}

/**
 * @brief Compute Safe Failure Fraction (SFF) for STO.
 *
 * SFF = (Σ λ_S + Σ λ_DD) / (Σ λ_S + Σ λ_DD + Σ λ_DU)
 *
 * SFF determines the maximum SIL per IEC 61508-2:
 *   SFF < 60%:  SIL 1 max
 *   SFF 60-90%: SIL 2 max
 *   SFF 90-99%: SIL 3 max
 *   SFF ≥ 99%:  SIL 4 max
 *
 * Kinetix STO typically achieves SFF > 99% (SIL 3 capable).
 *
 * @param lambda_s   Safe failure rate
 * @param lambda_dd  Dangerous detected failure rate
 * @param lambda_du  Dangerous undetected failure rate
 * @return           Safe Failure Fraction (0-1)
 */
double kinetix_sto_sff_compute(double lambda_s, double lambda_dd,
                                 double lambda_du)
{
    double denominator = lambda_s + lambda_dd + lambda_du;
    if (denominator <= 0.0) return 1.0; /* Perfect by definition */

    return (lambda_s + lambda_dd) / denominator;
}

/*===========================================================================
 * L6: Classic Problem – Drive Selection for a Conveyor Application
 *
 * Scenario: A VPL-B1653F motor drives a conveyor belt with:
 *   - Load inertia: 5× motor rotor inertia
 *   - Continuous torque requirement: 60% of motor rated
 *   - Peak torque: 80% of motor peak (acceleration phase)
 *   - Ambient temp: 40°C, drive in IP20 enclosure
 *
 * Select the appropriate Kinetix 5300 drive.
 *===========================================================================*/

/**
 * @brief Recommend a Kinetix drive catalog ID for a given motor and load.
 *
 * Selects the smallest drive that can handle the specified continuous
 * and peak current requirements.
 *
 * @param motor             Motor to power
 * @param load_inertia_ratio J_load / J_motor
 * @param[out] drive_catalog Recommended drive catalog ID
 * @return                  0 on success, -1 if no suitable drive found
 */
int kinetix_recommend_drive(const motor_database_t *motor,
                              double load_inertia_ratio,
                              uint16_t *drive_catalog)
{
    if (!motor || !drive_catalog || load_inertia_ratio < 0.0) return -1;

    /* Total inertia */
    /* Total inertia (reserved for future drive sizing refinement) */
    /* double J_total = motor->rotor_inertia_kgcm2 * (1.0 + load_inertia_ratio); */
    (void)load_inertia_ratio;

    /* Peak current demand during acceleration */
    double accel_torque = motor->rated_torque_nm * load_inertia_ratio * 1.5;
    double total_peak_torque = motor->rated_torque_nm + accel_torque;
    double required_peak_current = total_peak_torque / motor->torque_constant_kt;
    if (required_peak_current > motor->peak_current_amps) {
        required_peak_current = motor->peak_current_amps;
    }

    /* Continuous current: 60% duty */
    double required_cont_current = motor->rated_current_amps;

    /* Search 5300 database for matching drive */
    for (size_t i = 0; i < g_kinetix_5300_count; i++) {
        if (g_kinetix_5300_database[i].rating.output_current_cont >= required_cont_current &&
            g_kinetix_5300_database[i].rating.output_current_peak >= required_peak_current) {
            *drive_catalog = g_kinetix_5300_database[i].catalog_id;
            return 0;
        }
    }

    /* Try 5700 series */
    for (size_t i = 0; i < g_kinetix_5700_count; i++) {
        if (g_kinetix_5700_database[i].rating.output_current_cont >= required_cont_current &&
            g_kinetix_5700_database[i].rating.output_current_peak >= required_peak_current) {
            *drive_catalog = g_kinetix_5700_database[i].catalog_id;
            return 0;
        }
    }

    return -1; /* No suitable drive */
}

/**
 * @brief Get the Kinetix catalog number string.
 *
 * @param catalog_id  Drive catalog ID
 * @return            Catalog number string, or "UNKNOWN" if not found
 */
const char *kinetix_get_catalog_number(uint16_t catalog_id)
{
    /* Search all databases */
    for (size_t i = 0; i < g_kinetix_5100_count; i++) {
        if (g_kinetix_5100_database[i].catalog_id == catalog_id)
            return g_kinetix_5100_database[i].catalog_number;
    }
    for (size_t i = 0; i < g_kinetix_5300_count; i++) {
        if (g_kinetix_5300_database[i].catalog_id == catalog_id)
            return g_kinetix_5300_database[i].catalog_number;
    }
    for (size_t i = 0; i < g_kinetix_5700_count; i++) {
        if (g_kinetix_5700_database[i].catalog_id == catalog_id)
            return g_kinetix_5700_database[i].catalog_number;
    }
    return "UNKNOWN";
}
