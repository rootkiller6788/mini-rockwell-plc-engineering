/**
 * @file    logix_motion_control.c
 * @brief   CIP Motion Control Implementation
 * L5-L6: S-curve profiles, servo loop, electronic gearing/camming
 *
 * Reference: MOTION-RM003; Biagiotti & Melchiorri (2008)
 */

#include <string.h>
#include <math.h>
#include "logix_motion_control.h"

void logix_motion_axis_init(motion_axis_t *axis,
                             const char *name,
                             motion_axis_type_t type,
                             uint32_t axis_number)
{
    if (!axis || !name) return;

    memset(axis, 0, sizeof(*axis));
    strncpy(axis->axis_name, name, 39);
    axis->axis_name[39] = '\0';
    axis->type = type;
    axis->axis_number = axis_number;
    axis->coarse_update_rate_us = 1000;  /* Default 1ms */

    /* Default dynamics */
    axis->max_velocity = 1000.0;
    axis->max_acceleration = 10000.0;
    axis->max_deceleration = 10000.0;
    axis->max_jerk = 100000.0;

    /* Default gains */
    axis->pos_kp = 50.0;
    axis->pos_kvff = 1.0;
    axis->pos_kaff = 0.0;

    axis->state = AXIS_STATE_DISABLED;
}

void logix_motion_axis_set_mechanical(motion_axis_t *axis,
                                        double resolution,
                                        double gear_num, double gear_den,
                                        double pitch_mm)
{
    if (!axis) return;
    axis->drive_resolution = resolution;
    axis->gear_ratio_num = (gear_num > 0.0) ? gear_num : 1.0;
    axis->gear_ratio_den = (gear_den > 0.0) ? gear_den : 1.0;
    axis->lead_screw_pitch_mm = pitch_mm;
}

void logix_motion_axis_set_dynamics(motion_axis_t *axis,
                                      double max_vel,
                                      double max_accel,
                                      double max_decel,
                                      double max_jerk)
{
    if (!axis) return;
    axis->max_velocity = (max_vel > 0.0) ? max_vel : 1000.0;
    axis->max_acceleration = (max_accel > 0.0) ? max_accel : 10000.0;
    axis->max_deceleration = (max_decel > 0.0) ? max_decel : 10000.0;
    axis->max_jerk = (max_jerk > 0.0) ? max_jerk : 100000.0;
}

void logix_motion_axis_set_gains(motion_axis_t *axis,
                                   double kp, double kvff, double kaff)
{
    if (!axis) return;
    axis->pos_kp = kp;
    axis->pos_kvff = kvff;
    axis->pos_kaff = kaff;
}

double logix_motion_compute_s_curve(double start_pos, double target_pos,
                                      double max_vel, double max_accel,
                                      double max_jerk,
                                      motion_s_curve_profile_t *profile)
{
    if (!profile) return 0.0;

    memset(profile, 0, sizeof(*profile));

    double distance = fabs(target_pos - start_pos);
    if (distance < 1e-12) {
        profile->total_time = 0.0;
        profile->total_distance = 0.0;
        return 0.0;
    }

    profile->total_distance = distance;

    /* Check if triangular profile (short move: max_vel not reached) */
    /* Time to reach max accel: t_j = max_accel / max_jerk */
    /* For short moves where max velocity is not reached,
     * the profile is triangular (no constant velocity phase).
     * Distance at which triangular profile reaches max_accel:
     *   d_accel_only = 2 * max_accel^3 / max_jerk^2 */
    double d_full = 2.0 * max_accel * max_accel * max_accel /
                    (max_jerk * max_jerk);

    if (distance <= d_full) {
        /* Triangular profile: just pure acceleration + deceleration */
        profile->is_triangular = true;

        /* For triangular: timing is determined by jerk and distance */
        /* Solve: distance = max_jerk * t_total^3 / 24 (approximation) */
        /* t_total = cbrt(24 * distance / max_jerk) */
        double t_total = cbrt(24.0 * distance / max_jerk);

        profile->jerk_up_time = t_total / 2.0;
        profile->jerk_down_time = t_total / 4.0;
        profile->decel_jerk_time = t_total / 4.0;
        profile->final_jerk_time = t_total / 4.0;
        profile->const_accel_time = 0.0;
        profile->const_vel_time = 0.0;
        profile->const_decel_time = 0.0;
        profile->total_time = t_total;
    } else {
        /* Full S-curve profile: reaches max velocity */
        profile->is_triangular = false;

        /* Jerk ramp-up to max acceleration */
        profile->jerk_up_time = max_accel / max_jerk;

        /* Time at constant acceleration */
        double d_accel = max_accel * max_accel / max_jerk;  /* Distance during jerk phases of accel */
        double d_total_accel = 2.0 * d_accel;  /* Both accel and decel jerk phases */

        double d_remaining = distance - d_total_accel;
        double d_vel = max_vel * max_vel / max_accel;  /* Distance covered during velocity change */
        if (d_remaining > d_vel) {
            /* Has constant velocity phase */
            double d_const_vel = d_remaining - d_vel;
            profile->const_vel_time = d_const_vel / max_vel;
            profile->const_accel_time = 0.0;
        } else {
            /* No constant velocity, compute actual accel time */
            profile->const_vel_time = 0.0;
        }

        profile->jerk_down_time = profile->jerk_up_time;
        profile->decel_jerk_time = profile->jerk_up_time;
        profile->final_jerk_time = profile->jerk_up_time;

        profile->total_time = profile->jerk_up_time * 4.0 +
                              profile->const_accel_time * 2.0 +
                              profile->const_vel_time;
    }

    return profile->total_time;
}

void logix_motion_planner_step(motion_planner_t *planner,
                                 double dt_sec,
                                 double *pos_cmd,
                                 double *vel_cmd)
{
    if (!planner || !pos_cmd || !vel_cmd) return;

    if (!planner->move_active || planner->move_complete) {
        *pos_cmd = planner->current_position;
        *vel_cmd = 0.0;
        return;
    }

    planner->elapsed_time += dt_sec;

    if (planner->elapsed_time >= planner->total_time) {
        /* Move complete */
        planner->elapsed_time = planner->total_time;
        planner->current_position = planner->target_position;
        planner->current_velocity = 0.0;
        planner->move_complete = true;
        planner->move_active = false;
        *pos_cmd = planner->current_position;
        *vel_cmd = 0.0;
        return;
    }

    /* Trapezoidal velocity profile simplification:
     * Phase 1: acceleration ramp
     * Phase 2: constant velocity
     * Phase 3: deceleration ramp */

    double t = planner->elapsed_time;
    double total = planner->total_time;
    double v_max = planner->profile_velocity;
    double a_max = planner->profile_accel;

    /* Time to accelerate to v_max */
    double t_accel = (a_max > 0.0) ? v_max / a_max : 0.0;
    double t_decel_start = total - t_accel;

    double vel, pos;

    if (t <= t_accel) {
        /* Acceleration phase */
        vel = a_max * t;
        pos = planner->start_position + 0.5 * a_max * t * t;
    } else if (t >= t_decel_start) {
        /* Deceleration phase */
        double t_dec = t - t_decel_start;
        vel = v_max - a_max * t_dec;
        if (vel < 0.0) vel = 0.0;

        /* Position during deceleration */
        double d_accel = 0.5 * a_max * t_accel * t_accel;
        double d_const = v_max * (t_decel_start - t_accel);
        double d_dec = v_max * t_dec - 0.5 * a_max * t_dec * t_dec;
        pos = planner->start_position + d_accel + d_const + d_dec;
    } else {
        /* Constant velocity phase */
        vel = v_max;
        double d_accel = 0.5 * a_max * t_accel * t_accel;
        double d_const = v_max * (t - t_accel);
        pos = planner->start_position + d_accel + d_const;
    }

    planner->current_velocity = vel;
    planner->current_position = pos;
    planner->distance_remaining = planner->target_position - pos;

    *pos_cmd = pos;
    *vel_cmd = vel;
}

void logix_motion_servo_update(motion_axis_t *axis,
                                double pos_cmd, double vel_ff,
                                double dt_sec)
{
    if (!axis) return;

    /* Position loop: P + velocity feedforward */
    double pos_error = pos_cmd - axis->actual_position;
    double vel_cmd = axis->pos_kp * pos_error + axis->pos_kvff * vel_ff;

    /* Simulate velocity integration:
     * actual_pos(n) = actual_pos(n-1) + vel_cmd * dt */
    axis->actual_position += vel_cmd * dt_sec;
    axis->actual_velocity = vel_cmd;
    axis->command_position = pos_cmd;

    /* Update following error */
    axis->following_error = fabs(pos_cmd - axis->actual_position);
}

double logix_motion_gear_compute(const motion_gear_t *gear,
                                   double master_reference)
{
    if (!gear) return 0.0;
    if (gear->ratio_den == 0.0) return 0.0;

    return master_reference * gear->ratio_num / gear->ratio_den + gear->offset;
}

double logix_motion_cam_interpolate(const motion_cam_t *cam,
                                      double master_pos)
{
    if (!cam || cam->cam_point_count < 2) return 0.0;

    /* Find segment containing master_pos */
    uint32_t seg = 0;
    for (uint32_t i = 0; i < cam->cam_point_count - 1; i++) {
        if (master_pos >= cam->cam_master[i] &&
            master_pos <= cam->cam_master[i + 1]) {
            seg = i;
            break;
        }
    }

    /* Linear interpolation within segment */
    if (seg >= cam->cam_point_count - 1) seg = cam->cam_point_count - 2;

    double m0 = cam->cam_master[seg];
    double m1 = cam->cam_master[seg + 1];
    double s0 = cam->cam_slave[seg];
    double s1 = cam->cam_slave[seg + 1];

    if (fabs(m1 - m0) < 1e-12) return s0;

    double t = (master_pos - m0) / (m1 - m0);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    return s0 + t * (s1 - s0);
}

bool logix_motion_check_following_error(const motion_axis_t *axis)
{
    if (!axis) return false;
    return axis->following_error > axis->following_error_limit;
}

double logix_motion_min_move_time(double distance,
                                    double max_vel,
                                    double max_accel,
                                    double max_jerk)
{
    if (distance <= 0.0) return 0.0;

    /* Triangular profile time (jerk-limited, no constant acceleration) */
    double t_triangular = cbrt(24.0 * distance / max_jerk);

    /* Trapezoidal profile time (accel-limited) */
    double t_accel = max_vel / max_accel;
    double d_accel = max_accel * t_accel * t_accel;

    double t_total;
    if (distance <= d_accel) {
        /* Triangular velocity profile (no constant velocity) */
        t_total = 2.0 * sqrt(distance / max_accel);
    } else {
        /* Trapezoidal velocity profile */
        double d_const = distance - d_accel;
        double t_const = d_const / max_vel;
        t_total = 2.0 * t_accel + t_const;
    }

    /* The actual minimum is the larger of the two constraints */
    if (t_triangular > t_total) {
        return t_triangular;
    }
    return t_total;
}