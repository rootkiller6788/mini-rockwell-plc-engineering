/**
 * @file cip_motion.c
 * @brief CIP Motion protocol implementation for Kinetix Integrated Motion on EtherNet/IP.
 *
 * Level: L3, L5, L7
 * Reference:
 *   - ODVA CIP Motion Specification, Volumes 1 & 2
 *   - Rockwell Automation MOTION-AT004, "Integrated Motion on EtherNet/IP"
 *   - IEC 61800-7-201 (CiA 402), Profile Type 1 for Servo Drives
 *   - IEEE 1588-2008, Precision Time Protocol
 *   - Rockwell Automation, "EtherNet/IP Network Configuration" (ENET-UM001)
 *
 * Implements:
 *   L3: CIP Motion connection lifecycle, O→T/T→O data structures
 *   L5: CIP Sync clock synchronization (IEEE 1588 simulation)
 *   L7: Motion instruction encoding (MAM/MAJ/MAS/MSO/MSF/MAFR/MAG),
 *       feedback processing, communication loss detection
 *
 * Alignment: RWTH Aachen Industrial Control Systems, ISA/IEC 61131-3,
 *            Tsinghua Industrial Communication and Control
 */

#include "cip_motion.h"
#include <string.h>
#include <math.h>

/*===========================================================================
 * L3: CIP Connection Management
 *===========================================================================*/

/* Control word bit definitions per CIP Motion */
#define CIP_CTRL_SERVO_ON         (1u << 0)
#define CIP_CTRL_FAULT_RESET      (1u << 1)
#define CIP_CTRL_ENABLE_DRIVE     (1u << 2)
#define CIP_CTRL_ABORT_MOTION     (1u << 3)
#define CIP_CTRL_START_HOMING     (1u << 4)
#define CIP_CTRL_HOME_ABSOLUTE    (1u << 5)
#define CIP_CTRL_GEARING_ACTIVE   (1u << 6)
#define CIP_CTRL_CAMMING_ACTIVE   (1u << 7)
#define CIP_CTRL_REGISTRATION_ARM (1u << 8)
#define CIP_CTRL_TORQUE_MODE      (1u << 9)
#define CIP_CTRL_VELOCITY_MODE    (1u << 10)

/* Status word bit definitions */
#define CIP_STATUS_SERVO_ACTIVE    (1u << 0)
#define CIP_STATUS_AT_HOME         (1u << 1)
#define CIP_STATUS_MOVE_COMPLETE   (1u << 2)
#define CIP_STATUS_ACCELERATING    (1u << 3)
#define CIP_STATUS_DECELERATING    (1u << 4)
#define CIP_STATUS_CONST_VEL       (1u << 5)
#define CIP_STATUS_FAULTED         (1u << 6)
#define CIP_STATUS_WARNING         (1u << 7)
#define CIP_STATUS_IN_POSITION     (1u << 8)
#define CIP_STATUS_STO_ACTIVE      (1u << 9)
#define CIP_STATUS_BRAKE_ENGAGED   (1u << 10)
#define CIP_STATUS_DRIVE_READY     (1u << 11)
#define CIP_STATUS_CONNECTION_OK   (1u << 12)

/* Motion status word bit definitions */
#define CIP_MOTION_STATUS_MOVE_IP  (1u << 0)  /**< Move in progress */
#define CIP_MOTION_STATUS_MOVE_PC  (1u << 1)  /**< Process complete */
#define CIP_MOTION_STATUS_MOVE_ERR (1u << 2)  /**< Move error */
#define CIP_MOTION_STATUS_GEARED   (1u << 3)  /**< Gearing active */
#define CIP_MOTION_STATUS_CAMMED   (1u << 4)  /**< Camming active */
#define CIP_MOTION_STATUS_HOMING   (1u << 5)  /**< Homing in progress */
#define CIP_MOTION_STATUS_HOMED    (1u << 6)  /**< Homing complete */
#define CIP_MOTION_STATUS_REG1_OK  (1u << 7)  /**< Registration 1 captured */
#define CIP_MOTION_STATUS_REG2_OK  (1u << 8)  /**< Registration 2 captured */

void cip_connection_init(cip_connection_t *conn,
                         uint32_t drive_ip, uint32_t rpi_us)
{
    if (!conn) return;
    memset(conn, 0, sizeof(cip_connection_t));

    conn->state = CIP_CONN_STATE_IDLE;
    conn->params.target_ip = drive_ip;
    conn->params.rpi_us = rpi_us;
    conn->params.target_port = 2222;  /* CIP Motion UDP port */
    conn->params.timeout_multiplier = 4;
    conn->params.conn_type = CIP_CONN_MULTICAST;
    conn->params.target_slot = 0;

    /* Initialize sequence number */
    conn->cmd.sequence_number = 1;
    conn->fb.sequence_number = 0;
}

int cip_connection_open(cip_connection_t *conn,
                        const axis_config_t *axis_config)
{
    if (!conn) return -1;

    if (conn->state != CIP_CONN_STATE_IDLE &&
        conn->state != CIP_CONN_STATE_CLOSING) {
        return -1; /* Already open or in error state */
    }

    /* Simulate Forward Open */
    conn->state = CIP_CONN_STATE_ALLOCATING;

    /* Negotiate connection IDs */
    conn->params.originator_to_target_id = 0x40001000 | (axis_config ? axis_config->axis_instance : 1);
    conn->params.target_to_originator_id = 0x40002000 | (axis_config ? axis_config->axis_instance : 1);

    /* Connection established */
    conn->state = CIP_CONN_STATE_ESTABLISHED;
    conn->tx_count = 0;
    conn->rx_count = 0;
    conn->rx_timeouts = 0;
    conn->sequence_errors = 0;
    conn->cmd.sequence_number = 1;

    /* Reset feedback structure */
    memset(&conn->fb, 0, sizeof(conn->fb));

    return 0;
}

int cip_connection_close(cip_connection_t *conn)
{
    if (!conn) return -1;

    if (conn->state != CIP_CONN_STATE_ESTABLISHED) {
        return -1;
    }

    conn->state = CIP_CONN_STATE_CLOSING;
    /* Simulate Forward Close delay */
    conn->state = CIP_CONN_STATE_IDLE;

    return 0;
}

/*===========================================================================
 * L7: Motion Instruction Command Encoding
 *===========================================================================*/

/**
 * @brief Build common command header fields.
 *
 * Sets default control bits and clears instruction-specific fields.
 *
 * @param cmd        Command packet
 * @param instr_type Motion instruction type code
 */
static void cip_command_init(cip_motion_command_t *cmd, motion_instr_type_t instr_type)
{
    if (!cmd) return;
    memset(cmd, 0, sizeof(cip_motion_command_t));

    cmd->control_word = CIP_CTRL_ENABLE_DRIVE;
    cmd->motion_command = (uint32_t)instr_type;
    cmd->sequence_number++;
}

void cip_build_mam_command(cip_motion_command_t *cmd,
                           double target_pos, double speed,
                           double accel, double decel,
                           move_type_t move_type)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MAM);

    cmd->target_position = target_pos;
    cmd->max_speed       = speed;
    cmd->max_accel       = accel;
    cmd->max_decel       = decel;
    cmd->max_jerk        = 0.0;

    /* Instruction control: move type in bits 0-1 */
    cmd->instruction_control = (uint32_t)(move_type & 0x03);
}

void cip_build_maj_command(cip_motion_command_t *cmd,
                           double speed, double accel, double decel)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MAJ);

    /* Jog uses velocity command (signed) */
    cmd->target_velocity = speed;
    cmd->max_accel       = accel;
    cmd->max_decel       = decel;
    cmd->max_speed       = fabs(speed);
    cmd->control_word   |= CIP_CTRL_VELOCITY_MODE;
}

void cip_build_mas_command(cip_motion_command_t *cmd,
                           double decel, bool immediate)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MAS);

    cmd->max_decel = decel;

    if (immediate) {
        cmd->control_word |= CIP_CTRL_ABORT_MOTION;
        cmd->max_decel = 1e9; /* Infinite deceleration – immediate stop */
    }
}

void cip_build_mso_command(cip_motion_command_t *cmd)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MSO);
    cmd->control_word |= CIP_CTRL_SERVO_ON;
    cmd->control_word |= CIP_CTRL_ENABLE_DRIVE;
}

void cip_build_msf_command(cip_motion_command_t *cmd)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MSF);
    cmd->control_word &= ~CIP_CTRL_SERVO_ON;
    cmd->control_word &= ~CIP_CTRL_ENABLE_DRIVE;
}

void cip_build_mafr_command(cip_motion_command_t *cmd)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MAFR);
    cmd->control_word |= CIP_CTRL_FAULT_RESET;
}

void cip_build_mag_command(cip_motion_command_t *cmd,
                           double numerator, double denominator,
                           double master_reference, bool clutch_cmd)
{
    if (!cmd) return;
    cip_command_init(cmd, MOTION_INSTR_MAG);

    cmd->gear_ratio_numerator   = numerator;
    cmd->gear_ratio_denominator = denominator;
    cmd->master_reference       = master_reference;

    if (clutch_cmd) {
        cmd->control_word |= CIP_CTRL_GEARING_ACTIVE;
    }
}

/*===========================================================================
 * L3: Feedback Processing
 *===========================================================================*/

void cip_process_feedback(const cip_motion_feedback_t *fb,
                          axis_runtime_t *axis)
{
    if (!fb || !axis) return;

    /* Sequence number validation */
    if (fb->sequence_number != 0) {
        /* Sequence OK – copy feedback data */
        axis->actual_position   = fb->actual_position;
        axis->actual_velocity   = fb->actual_velocity;
        axis->actual_torque     = fb->actual_torque;
        axis->actual_current    = fb->actual_current;
        axis->actual_bus_voltage = fb->actual_bus_voltage;
        axis->actual_drive_temp_c = fb->actual_drive_temp;
        axis->position_error    = fb->following_error;
        axis->velocity_error    = fb->velocity_error;
        axis->fault_code        = (uint16_t)fb->fault_code;
        axis->registration_position = fb->registration_position;

        /* Parse status word */
        axis->servo_active  = (fb->status_word & CIP_STATUS_SERVO_ACTIVE) != 0;
        axis->at_home       = (fb->status_word & CIP_STATUS_AT_HOME) != 0;
        axis->move_complete  = (fb->motion_status & CIP_MOTION_STATUS_MOVE_PC) != 0;
        axis->accelerating   = (fb->status_word & CIP_STATUS_ACCELERATING) != 0;
        axis->decelerating   = (fb->status_word & CIP_STATUS_DECELERATING) != 0;
        axis->constant_velocity = (fb->status_word & CIP_STATUS_CONST_VEL) != 0;

        /* Parse axis state */
        axis->state = cip_parse_axis_state(fb->axis_state);
    }
}

/*===========================================================================
 * L5: Communication Loss Detection
 *===========================================================================*/

bool cip_detect_comm_loss(cip_connection_t *conn, uint32_t timeout_threshold)
{
    if (!conn) return true;

    if (conn->rx_timeouts >= timeout_threshold) {
        conn->state = CIP_CONN_STATE_TIMED_OUT;
        return true;
    }

    return false;
}

void cip_update_timeout(cip_connection_t *conn, bool feedback_received)
{
    if (!conn) return;

    if (conn->state != CIP_CONN_STATE_ESTABLISHED) {
        /* Only track timeouts for established connections */
        conn->rx_timeouts++;
        return;
    }

    if (feedback_received) {
        conn->rx_timeouts = 0;
        conn->rx_count++;
    } else {
        conn->rx_timeouts++;
    }
}

/*===========================================================================
 * L3: Status Word Encoding / Decoding
 *===========================================================================*/

uint32_t cip_get_status_word(axis_state_t state)
{
    switch (state) {
    case AXIS_STATE_DISABLED:
        return CIP_STATUS_DRIVE_READY | CIP_STATUS_STO_ACTIVE;
    case AXIS_STATE_STARTING:
        return CIP_STATUS_DRIVE_READY;
    case AXIS_STATE_RUNNING:
        return CIP_STATUS_SERVO_ACTIVE | CIP_STATUS_DRIVE_READY
             | CIP_STATUS_CONNECTION_OK;
    case AXIS_STATE_STOPPING:
        return CIP_STATUS_SERVO_ACTIVE | CIP_STATUS_DRIVE_READY;
    case AXIS_STATE_ERROR_STOP:
        return CIP_STATUS_FAULTED | CIP_STATUS_DRIVE_READY;
    case AXIS_STATE_HOMING:
        return CIP_STATUS_SERVO_ACTIVE | CIP_STATUS_DRIVE_READY;
    case AXIS_STATE_AXIS_OFFLINE:
        return 0;
    default:
        return CIP_STATUS_DRIVE_READY;
    }
}

axis_state_t cip_parse_axis_state(uint32_t status_word)
{
    if (status_word == 0) {
        return AXIS_STATE_AXIS_OFFLINE;
    }

    if (status_word & CIP_STATUS_FAULTED) {
        return AXIS_STATE_ERROR_STOP;
    }

    if ((status_word & CIP_STATUS_SERVO_ACTIVE) &&
        (status_word & CIP_STATUS_CONNECTION_OK)) {
        return AXIS_STATE_RUNNING;
    }

    if (status_word & CIP_STATUS_SERVO_ACTIVE) {
        return AXIS_STATE_STOPPING;
    }

    if (status_word & CIP_STATUS_DRIVE_READY) {
        return AXIS_STATE_DISABLED;
    }

    return AXIS_STATE_AXIS_OFFLINE;
}

/*===========================================================================
 * L5: CIP Sync (IEEE 1588 PTP) Simulation
 *
 * CIP Sync enables sub-microsecond synchronization between the Logix
 * controller and Kinetix drives. This is critical for:
 *   - Coordinated multi-axis motion (position lock between axes)
 *   - Registration accuracy (±1 μs timestamp resolution)
 *   - Distributed motion (axes on different drives moving together)
 *
 * IEEE 1588 PTP uses a master-slave hierarchy:
 *   Grandmaster (Stratix 5400/5410 switch) → Ordinary Clock (controller)
 *     → Ordinary Clock (Kinetix drive)
 *
 * The sync process:
 *   1. Master sends Sync message at t1 (captured in hardware)
 *   2. Slave receives at t2
 *   3. Master sends Follow_Up with t1
 *   4. Slave sends Delay_Req at t3
 *   5. Master receives at t4
 *   6. Master sends Delay_Resp with t4
 *
 *   Offset = (t2 - t1) - mean_path_delay
 *   mean_path_delay = ((t2 - t1) + (t4 - t3)) / 2
 *
 * This implementation simulates the PTP clock synchronization
 * for offline motion time coordination testing.
 *===========================================================================*/

void cip_sync_init(cip_sync_info_t *sync)
{
    if (!sync) return;
    memset(sync, 0, sizeof(cip_sync_info_t));

    sync->role = CIP_SYNC_ORDINARY_CLOCK;
    sync->sync_interval_s = 1;    /* 1 second sync interval (default) */
    sync->pdelay_interval_s = 2;  /* 2 second delay measurement */
    sync->synced_to_master = false;
}

int64_t cip_sync_process_message(cip_sync_info_t *sync, int64_t master_time_ns)
{
    if (!sync) return 0;

    sync->master_time_ns = master_time_ns;
    sync->sync_count++;

    /* Simulate path delay measurement */
    static double sim_delay_ns = 500.0; /* 500 ns typical for 100m cable */

    /* Update clock offset using simple low-pass filter */
    int64_t raw_offset = master_time_ns - sync->local_time_ns;

    /* First-order filter: offset = 0.9·offset_prev + 0.1·raw_offset */
    double alpha = 0.1;
    sync->clock_offset_ns = (int64_t)(
        (1.0 - alpha) * (double)sync->clock_offset_ns + alpha * (double)raw_offset
    );
    sync->mean_path_delay_ns = sim_delay_ns;

    /* Correct local time */
    sync->local_time_ns = master_time_ns - sync->clock_offset_ns;

    sync->synced_to_master = true;

    return sync->local_time_ns;
}

/*===========================================================================
 * L5: Connection Diagnostics and Statistics
 *===========================================================================*/

/**
 * @brief Compute the jitter between consecutive receive events.
 *
 * Jitter = actual_rpi - nominal_rpi
 *
 * Excessive jitter (>50 μs) may indicate network congestion or
 * switch configuration issues.
 *
 * @param conn      Connection state
 * @param timestamp Current receive timestamp (ns)
 */
void cip_compute_jitter(cip_connection_t *conn, int64_t timestamp)
{
    if (!conn) return;

    static int64_t last_timestamp = 0;

    if (last_timestamp > 0) {
        double interval_us = (double)(timestamp - last_timestamp) / 1000.0;
        double rpi_us = (double)conn->params.rpi_us;
        double jitter_us = interval_us - rpi_us;

        if (fabs(jitter_us) > conn->params.max_observed_jitter_us) {
            conn->params.max_observed_jitter_us = fabs(jitter_us);
        }
    }

    last_timestamp = timestamp;
}

/**
 * @brief Check sequence number integrity.
 *
 * Each O→T packet from controller increments the sequence number.
 * The T→O packet from the drive echoes the last received sequence number.
 * A mismatch indicates packet loss.
 *
 * @param expected_seq  Expected sequence number
 * @param received_seq  Received echoed sequence number
 * @return              true if sequence matches
 */
bool cip_check_sequence(uint32_t expected_seq, uint32_t received_seq)
{
    if (expected_seq == 0) return true; /* First cycle */

    /* Allow wrap-around (32-bit counter) */
    int32_t diff = (int32_t)(expected_seq - received_seq);
    return (diff >= 0 && diff <= 1); /* 0 = OK, 1 = one lost */
}

/**
 * @brief Compute the CIP connection packet rate.
 *
 * Packet rate = 1 / RPI for each direction.
 *
 * For a 2 ms RPI with 8 axes, total packet rate:
 *   8 axes × 2 directions × (1/0.002) = 8000 packets/sec
 *
 * This is well within the capability of a 100 Mbps EtherNet/IP network
 * (theoretical max ~100,000 packets/sec for CIP Motion).
 *
 * @param rpi_us        Requested Packet Interval (μs)
 * @param num_axes      Number of axes
 * @return              Total packet rate (packets/sec)
 */
double cip_compute_packet_rate(uint32_t rpi_us, uint32_t num_axes)
{
    if (rpi_us == 0) return 0.0;
    double rpi_s = (double)rpi_us * 1e-6;
    return (double)num_axes * 2.0 / rpi_s; /* 2 directions per axis */
}
