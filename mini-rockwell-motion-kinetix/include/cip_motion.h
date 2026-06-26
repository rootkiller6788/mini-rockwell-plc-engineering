/**
 * @file cip_motion.h
 * @brief CIP Motion protocol structures for Kinetix Integrated Motion on EtherNet/IP.
 *
 * Level: L2 Core Concepts + L3 Engineering Structures + L7 Applications
 * Reference:
 *   - ODVA CIP Motion Specification, Volume 1, "Common Industrial Protocol"
 *   - ODVA CIP Motion Specification, Volume 2, "CIP Motion Device Profiles"
 *   - Rockwell Automation, "Integrated Motion on EtherNet/IP" (MOTION-AT004)
 *   - IEC 61800-7-201: Profile Type 1 for Servo Drives (CiA 402)
 *   - IEEE 1588-2008: Precision Time Protocol (PTP) for CIP Sync
 *
 * CIP Motion defines standard objects, services, and data structures for
 * motion control over EtherNet/IP. Key objects:
 *   - Motion Axis Object (Class 0x42): Per-axis configuration and state
 *   - Motion Group Object (Class 0x43): Multi-axis coordination
 *   - Connection Manager (Class 0x06): CIP connections for cyclic data
 *   - Time Sync Object (Class 0x43): IEEE 1588 PTP for motion synchronization
 *
 * This module implements the CIP Motion protocol structures and state
 * machines needed to communicate with Kinetix drives.
 *
 * Alignment: RWTH Aachen Industrial Control Systems, ISA/IEC 61131-3,
 *            Tsinghua Industrial Communication and Control
 */

#ifndef CIP_MOTION_H
#define CIP_MOTION_H

#include "axis_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*===========================================================================
 * L2: Core Concepts – CIP Motion Connection Types
 *===========================================================================*/

/**
 * @brief CIP Motion connection type for EtherNet/IP I/O connections.
 *
 * CIP Motion uses standard EtherNet/IP connection types:
 *
 * MUTLICAST:  One producer → many consumers (controller→multiple drives).
 *             Lowers controller bandwidth. Default for motion.
 * UNICAST:    One producer → one consumer. Requires more controller bandwidth
 *             but uses less network bandwidth per multicast group.
 */
typedef enum {
    CIP_CONN_MULTICAST  = 0,
    CIP_CONN_UNICAST    = 1,
    CIP_CONN_LISTEN_ONLY = 2
} cip_connection_type_t;

/**
 * @brief CIP Motion connection state.
 */
typedef enum {
    CIP_CONN_STATE_IDLE         = 0,  /**< Connection not established */
    CIP_CONN_STATE_ALLOCATING   = 1,  /**< Forward Open in progress */
    CIP_CONN_STATE_ESTABLISHED  = 2,  /**< Connection active, O→T data flowing */
    CIP_CONN_STATE_TIMED_OUT    = 3,  /**< Connection timed out (not refreshed) */
    CIP_CONN_STATE_CLOSING      = 4,  /**< Forward Close in progress */
    CIP_CONN_STATE_FAULTED      = 5   /**< Connection faulted */
} cip_connection_state_t;

/*===========================================================================
 * L3: Engineering Structures – CIP Motion Data Layout
 *===========================================================================*/

/**
 * @brief CIP Motion controller-to-drive (O→T) cyclic data.
 *
 * This is the data produced by the controller (Originator) and consumed
 * by the drive (Target) every RPI cycle. Also called the "Command Data."
 *
 * Layout follows the CIP Motion Connection Data Format:
 *   Bytes 0-3:   Command word / status bits
 *   Bytes 4-11:  Target position (64-bit REAL)
 *   Bytes 12-19: Target velocity (64-bit REAL)
 *   Bytes 20-27: Torque offset (64-bit REAL)
 *   etc.
 */
#define CIP_MOTION_CMD_DATA_SIZE 64  /**< Typical O→T data size (bytes) */

typedef struct {
    /* Connection header */
    uint32_t sequence_number;               /**< Rolling sequence (for loss detection) */

    /* Control word */
    uint32_t control_word;                  /**< Drive control bits (servo on, reset faults, etc.) */
    uint32_t motion_command;                /**< Active motion instruction code */

    /* Position command */
    double   target_position;               /**< Commanded target position (eng units) */
    double   position_offset;               /**< Position fine offset (for incremental) */

    /* Velocity and torque */
    double   target_velocity;               /**< Commanded velocity (eng units/sec) */
    double   velocity_offset;               /**< Velocity fine trim */
    double   torque_feedforward;            /**< Torque/current feedforward (N·m or %) */

    /* Instruction parameters */
    double   max_speed;                     /**< Speed limit for active move */
    double   max_accel;                     /**< Acceleration limit */
    double   max_decel;                     /**< Deceleration limit */
    double   max_jerk;                      /**< Jerk limit (S-curve) */

    /* Gear/Cam references */
    double   master_reference;              /**< Master position for gearing/camming */
    double   gear_ratio_numerator;          /**< Active gear ratio numerator */
    double   gear_ratio_denominator;        /**< Active gear ratio denominator */

    /* Miscellaneous */
    uint32_t instruction_control;           /**< Instruction-specific control bits */
    uint32_t reserved;                      /**< Padding to align to 64 bytes */
} cip_motion_command_t;

/**
 * @brief CIP Motion drive-to-controller (T→O) cyclic data.
 *
 * Also called "Feedback Data" or "Actual Data." Produced by the drive
 * and consumed by the controller each RPI cycle.
 */
#define CIP_MOTION_FB_DATA_SIZE 64   /**< Typical T→O data size (bytes) */

typedef struct {
    /* Connection header */
    uint32_t sequence_number;               /**< Echo of last received O→T sequence */
    uint32_t status_word;                   /**< Drive status bits */
    uint32_t motion_status;                 /**< Motion-specific status bits */
    uint32_t fault_code;                    /**< Active fault code bitmap */

    /* Actual feedback */
    double   actual_position;               /**< Actual position from encoder (eng units) */
    double   actual_velocity;               /**< Actual velocity (eng units/sec) */
    double   actual_torque;                 /**< Actual torque (N·m) */
    double   actual_current;                /**< Actual motor current (A RMS) */
    double   actual_bus_voltage;            /**< DC bus voltage (V) */
    double   actual_drive_temp;             /**< Drive temperature (°C) */

    /* Following error */
    double   following_error;               /**< Position following error */
    double   velocity_error;                /**< Velocity following error */

    /* Status flags */
    uint32_t event_status;                  /**< Event notification flags */
    uint32_t axis_state;                    /**< Axis state as per CIP Motion */
    double   registration_position;         /**< Registration 1 latched position */
    double   registration2_position;        /**< Registration 2 latched position */

    /* Diagnostic */
    double   life_counter;                  /**< Running hours counter */
    uint32_t reserved;                      /**< Padding */
} cip_motion_feedback_t;

/*===========================================================================
 * L3: Engineering Structures – CIP Motion Axis Object Attributes
 *===========================================================================*/

/**
 * @brief CIP Motion Axis Object (Class 0x42) – Key instance attributes.
 *
 * These are the configurable attributes accessed via CIP Explicit Messaging
 * (Get_Attribute_Single / Set_Attribute_Single) for motion configuration.
 */
typedef enum {
    CIP_AXIS_ATTR_AXIS_TYPE           = 0x01,  /**< Servo/induction/linear/virtual */
    CIP_AXIS_ATTR_MOTOR_CATALOG       = 0x02,  /**< Motor catalog string */
    CIP_AXIS_ATTR_ENCODER_COUNTS      = 0x03,  /**< Feedback counts per revolution */
    CIP_AXIS_ATTR_VELOCITY_LIMIT      = 0x04,  /**< Max velocity limit */
    CIP_AXIS_ATTR_ACCEL_LIMIT         = 0x05,  /**< Max acceleration limit */
    CIP_AXIS_ATTR_DECEL_LIMIT         = 0x06,  /**< Max deceleration limit */
    CIP_AXIS_ATTR_JERK_LIMIT          = 0x07,  /**< Max jerk limit */
    CIP_AXIS_ATTR_POS_LOOP_GAIN       = 0x08,  /**< Position loop Kp */
    CIP_AXIS_ATTR_VEL_LOOP_GAIN       = 0x09,  /**< Velocity loop Kp */
    CIP_AXIS_ATTR_VEL_LOOP_INTEGRAL   = 0x0A,  /**< Velocity loop Ki */
    CIP_AXIS_ATTR_CURRENT_LOOP_GAIN   = 0x0B,  /**< Current loop Kp */
    CIP_AXIS_ATTR_CURRENT_LOOP_INT    = 0x0C,  /**< Current loop Ki */
    CIP_AXIS_ATTR_FOLLOWING_ERR_LIMIT = 0x0D,  /**< Following error fault limit */
    CIP_AXIS_ATTR_NOTCH1_FREQ         = 0x0E,  /**< Notch filter 1 frequency */
    CIP_AXIS_ATTR_NOTCH1_DEPTH        = 0x0F,  /**< Notch filter 1 depth */
    CIP_AXIS_ATTR_NOTCH2_FREQ         = 0x10,  /**< Notch filter 2 frequency */
    CIP_AXIS_ATTR_NOTCH2_DEPTH        = 0x11,  /**< Notch filter 2 depth */
    CIP_AXIS_ATTR_LOAD_INERTIA_RATIO  = 0x12,  /**< Load-to-motor inertia ratio */
    CIP_AXIS_ATTR_FRICTION_COMP       = 0x13,  /**< Friction compensation value */
    CIP_AXIS_ATTR_HOME_METHOD         = 0x14,  /**< Homing method (CiA 402) */
    CIP_AXIS_ATTR_HOME_OFFSET         = 0x15,  /**< Home offset */
    CIP_AXIS_ATTR_BRAKE_DELAY         = 0x16,  /**< Brake engage delay time */
    CIP_AXIS_ATTR_MAX_MOTOR_TEMP      = 0x17,  /**< Motor thermal model limit */
    CIP_AXIS_ATTR_LOAD_OBSERVER_GAIN  = 0x18,  /**< Load observer bandwidth */
} cip_axis_attribute_t;

/**
 * @brief CIP Motion common services.
 *
 * Services supported by the Motion Axis Object for explicit messaging.
 */
typedef enum {
    CIP_SERVICE_GET_ATTRIBUTE_SINGLE  = 0x0E,
    CIP_SERVICE_SET_ATTRIBUTE_SINGLE  = 0x10,
    CIP_SERVICE_RESET                 = 0x05,
    CIP_SERVICE_START                 = 0x06,
    CIP_SERVICE_STOP                  = 0x07,
    CIP_SERVICE_FAULT_RESET           = 0x4B,
    CIP_SERVICE_RUN_HOOKUP_TEST       = 0x4C,
    CIP_SERVICE_ABORT_MOTION          = 0x4D,
    CIP_SERVICE_APPLY_TUNING          = 0x4E,
    CIP_SERVICE_IDENTIFY_MOTOR        = 0x4F,
    CIP_SERVICE_START_HOMING          = 0x50,
    CIP_SERVICE_SET_HOME_POSITION     = 0x51,
    CIP_SERVICE_ENABLE_DRIVE          = 0x52,
    CIP_SERVICE_DISABLE_DRIVE         = 0x53,
    CIP_SERVICE_START_AUTOTUNE        = 0x54
} cip_service_code_t;

/*===========================================================================
 * L3: Engineering Structures – CIP Motion Connection Parameters
 *===========================================================================*/

/**
 * @brief CIP Motion cyclic connection configuration.
 *
 * Defines the parameters for establishing an EtherNet/IP class 1
 * connection for motion cyclic data exchange.
 */
typedef struct {
    /* Connection parameters */
    uint32_t originator_to_target_id;   /**< O→T connection ID */
    uint32_t target_to_originator_id;   /**< T→O connection ID */
    uint32_t rpi_us;                    /**< Requested Packet Interval (μs) */
    uint32_t timeout_multiplier;        /**< Connection timeout multiplier (×RPI) */
    cip_connection_type_t conn_type;    /**< Multicast vs unicast */

    /* Network addressing */
    uint32_t target_ip;                 /**< Drive IP address (network byte order) */
    uint16_t target_port;               /**< Drive UDP port (default 2222 for motion) */
    uint8_t  target_mac[6];             /**< Drive MAC address */
    uint8_t  target_slot;               /**< Logix backplane slot number */

    /* Connection timing */
    double   actual_rpi_ms;             /**< Actual achieved RPI (for diagnostics) */
    uint32_t packet_loss_count;         /**< Cumulative packet loss counter */
    uint32_t sequence_mismatch_count;   /**< Sequence number mismatch counter */
    double   max_observed_jitter_us;    /**< Maximum observed cycle jitter (μs) */

    /* CIP Sync / IEEE 1588 */
    bool     ptp_synced;                /**< IEEE 1588 time sync established */
    int64_t  ptp_offset_ns;             /**< Current PTP clock offset (ns) */
    double   ptp_mean_path_delay_ns;    /**< Mean path delay (ns) */
} cip_connection_params_t;

/**
 * @brief CIP Motion connection runtime state.
 */
typedef struct {
    cip_connection_state_t state;       /**< Connection state machine */
    cip_connection_params_t params;     /**< Connection parameters */
    cip_motion_command_t   cmd;         /**< O→T command data (controller → drive) */
    cip_motion_feedback_t  fb;          /**< T→O feedback data (drive → controller) */

    /* Stats */
    uint64_t tx_count;                  /**< Transmitted command packets */
    uint64_t rx_count;                  /**< Received feedback packets */
    uint32_t rx_timeouts;               /**< Consecutive receive timeouts */
    uint32_t sequence_errors;           /**< Cumulative sequence errors */
} cip_connection_t;

/*===========================================================================
 * L7: Industrial Applications – CIP Motion API
 *===========================================================================*/

/**
 * @brief Initialize a CIP Motion connection structure.
 *
 * Sets all connection parameters to defaults and initializes state to IDLE.
 *
 * @param conn          Connection to initialize
 * @param drive_ip      Kinetix drive IPv4 address (network byte order)
 * @param rpi_us        Requested Packet Interval in microseconds
 */
void cip_connection_init(cip_connection_t *conn,
                         uint32_t drive_ip, uint32_t rpi_us);

/**
 * @brief Simulate opening a CIP Motion connection (Forward Open).
 *
 * In a real system, this would send an EtherNet/IP Forward Open request.
 * In this simulation, it transitions the state through the CIP connection
 * lifecycle for offline testing and analysis.
 *
 * @param conn          Connection state
 * @param axis_config   Axis configuration (used to negotiate data size)
 * @return              0 on success (state transitions to ESTABLISHED)
 */
int cip_connection_open(cip_connection_t *conn,
                        const axis_config_t *axis_config);

/**
 * @brief Simulate closing a CIP Motion connection (Forward Close).
 *
 * @param conn  Connection to close
 * @return      0 on success
 */
int cip_connection_close(cip_connection_t *conn);

/**
 * @brief Build a CIP Motion command packet for a point-to-point move (MAM).
 *
 * Fills the O→T data structure with the appropriate control word, target
 * position, velocity, acceleration, and deceleration for a MAM instruction.
 *
 * @param cmd           Command packet to fill
 * @param target_pos    Target position (eng units)
 * @param speed         Move speed (eng units/sec)
 * @param accel         Acceleration (eng units/sec²)
 * @param decel         Deceleration (eng units/sec²)
 * @param move_type     Absolute, relative, or additive
 */
void cip_build_mam_command(cip_motion_command_t *cmd,
                           double target_pos, double speed,
                           double accel, double decel,
                           move_type_t move_type);

/**
 * @brief Build a CIP Motion command packet for jogging (MAJ).
 *
 * @param cmd       Command packet to fill
 * @param speed     Jog speed (eng units/sec, signed for direction)
 * @param accel     Acceleration rate
 * @param decel     Deceleration rate
 */
void cip_build_maj_command(cip_motion_command_t *cmd,
                           double speed, double accel, double decel);

/**
 * @brief Build a CIP Motion command packet for stopping (MAS).
 *
 * @param cmd       Command packet to fill
 * @param decel     Deceleration rate
 * @param immediate If true, abort fastest possible (no decel ramp)
 */
void cip_build_mas_command(cip_motion_command_t *cmd,
                           double decel, bool immediate);

/**
 * @brief Build CIP Motion command for servo on (MSO).
 *
 * @param cmd    Command packet to fill
 */
void cip_build_mso_command(cip_motion_command_t *cmd);

/**
 * @brief Build CIP Motion command for servo off (MSF).
 *
 * @param cmd    Command packet to fill
 */
void cip_build_msf_command(cip_motion_command_t *cmd);

/**
 * @brief Build CIP Motion command for fault reset (MAFR).
 *
 * @param cmd    Command packet to fill
 */
void cip_build_mafr_command(cip_motion_command_t *cmd);

/**
 * @brief Build CIP Motion command for gearing (MAG).
 *
 * @param cmd               Command packet
 * @param numerator         Gear ratio numerator
 * @param denominator       Gear ratio denominator
 * @param master_reference  Master axis position reference
 * @param clutch_cmd        true = engage, false = disengage
 */
void cip_build_mag_command(cip_motion_command_t *cmd,
                           double numerator, double denominator,
                           double master_reference, bool clutch_cmd);

/**
 * @brief Process a received feedback packet into the axis runtime structure.
 *
 * Extracts position, velocity, torque, fault codes, etc. from the
 * CIP Motion feedback data into the runtime axis state.
 *
 * @param fb        Received feedback packet
 * @param axis      Target axis runtime state (updated in place)
 */
void cip_process_feedback(const cip_motion_feedback_t *fb,
                          axis_runtime_t *axis);

/**
 * @brief Detect communication loss based on consecutive timeouts.
 *
 * A timeout occurs when no T→O packet is received within the
 * timeout multiplier × RPI window.
 *
 * @param conn                  Connection state
 * @param timeout_threshold     Max consecutive timeouts before fault
 * @return                      true if connection is considered lost
 */
bool cip_detect_comm_loss(cip_connection_t *conn, uint32_t timeout_threshold);

/**
 * @brief Update connection timeout tracking (called each RPI cycle).
 *
 * Increments timeout counter if no feedback received this cycle.
 *
 * @param conn              Connection state
 * @param feedback_received true if T→O packet was received this cycle
 */
void cip_update_timeout(cip_connection_t *conn, bool feedback_received);

/**
 * @brief Get the CIP Motion status word for a given axis state.
 *
 * Maps the internal axis_state_t to the CIP Motion status word bits.
 *
 * @param state  Axis state
 * @return       CIP status word
 */
uint32_t cip_get_status_word(axis_state_t state);

/**
 * @brief Parse the CIP Motion status word to axis state.
 *
 * @param status_word  CIP status word from drive feedback
 * @return             Mapped axis state
 */
axis_state_t cip_parse_axis_state(uint32_t status_word);

/*===========================================================================
 * L3: Engineering Structures – CIP Sync (IEEE 1588) Constants
 *===========================================================================*/

/**
 * @brief CIP Sync precision levels.
 */
typedef enum {
    CIP_SYNC_MASTER_CLOCK     = 0,   /**< Grandmaster clock (e.g., Stratix 5400) */
    CIP_SYNC_ORDINARY_CLOCK   = 1,   /**< Ordinary clock (controller / drive) */
    CIP_SYNC_BOUNDARY_CLOCK   = 2,   /**< Boundary clock (managed switch) */
    CIP_SYNC_TRANSPARENT_CLOCK = 3   /**< Transparent clock (switch) */
} cip_sync_role_t;

/**
 * @brief CIP Sync timing information for motion coordination.
 */
typedef struct {
    cip_sync_role_t role;
    int64_t  master_time_ns;           /**< Current grandmaster time (ns) */
    int64_t  local_time_ns;            /**< Local clock time (ns) */
    int64_t  clock_offset_ns;          /**< Offset from master (ns) */
    double   mean_path_delay_ns;       /**< Mean path delay (ns) */
    uint32_t sync_interval_s;          /**< Sync message interval (s) */
    uint32_t pdelay_interval_s;        /**< Peer delay measurement interval (s) */
    uint32_t sync_count;               /**< Received sync message count */
    uint32_t lost_sync_count;          /**< Lost sync count */
    bool     synced_to_master;         /**< Currently synchronized */
} cip_sync_info_t;

/**
 * @brief Initialize CIP Sync info.
 *
 * @param sync  CIP Sync info to initialize
 */
void cip_sync_init(cip_sync_info_t *sync);

/**
 * @brief Simulate receiving a PTP sync message and update clock.
 *
 * @param sync           CIP Sync state
 * @param master_time_ns Received master time (ns)
 * @return               Corrected local time (ns)
 */
int64_t cip_sync_process_message(cip_sync_info_t *sync, int64_t master_time_ns);

#endif /* CIP_MOTION_H */
