/**
 * @file pointio_module.h
 * @brief Point I/O module management - creation, configuration, lifecycle.
 *
 * Level: L2 Core Concepts + L3 Engineering Structures
 * Reference:
 *   - Rockwell "Logix5000 Controllers I/O and Tag Data" (1756-PM004)
 *   - Rockwell "1734 POINT I/O User Manual" (1734-UM001)
 */

#ifndef POINTIO_MODULE_H
#define POINTIO_MODULE_H

#include "pointio_types.h"

/*===========================================================================
 * L3: Module Descriptor - complete representation of one Point I/O module
 *===========================================================================*/

/**
 * @brief Complete runtime descriptor for a single Point I/O module.
 *
 * This struct contains all static configuration (identity, type, channels)
 * plus dynamic state (status, connections, data image).
 */
typedef struct {
    /* --- Static identification --- */
    pointio_slot_t          slot;               /* Physical position */
    pointio_identity_t      identity;           /* Electronic Data Sheet */
    pointio_module_type_t   type;               /* Functional category */
    pointio_keying_t        keying;             /* Electronic keying */
    char                    name[64];           /* User-defined tag name */
    char                    description[128];   /* User-defined description */

    /* --- Channel configuration --- */
    uint8_t                 num_channels;       /* Total I/O channels */
    uint8_t                 input_channels;     /* Number of input channels */
    uint8_t                 output_channels;    /* Number of output channels */
    uint8_t                 input_size_bytes;   /* Input assembly size */
    uint8_t                 output_size_bytes;  /* Output assembly size */
    uint8_t                 config_size_bytes;  /* Configuration assembly size */

    /* --- Communication --- */
    pointio_connection_type_t conn_type;        /* Exclusive owner, etc. */
    uint32_t                rpi_us;             /* Requested Packet Interval (us) */
    uint32_t                timeout_us;         /* Connection timeout (us) */
    pointio_trigger_t       trigger;            /* Cyclic/COS/App */

    /* --- Fault & safety --- */
    pointio_fault_mode_t    fault_mode;         /* Output state on fault */
    pointio_prog_mode_t     prog_mode;          /* Output state in program */
    pointio_safety_params_t safety;             /* SIL parameters */

    /* --- Power --- */
    pointio_power_consumption_t power;          /* Power draw */

    /* --- Dynamic state --- */
    pointio_module_status_t status;             /* Current operating status */
    pointio_fault_code_t    active_fault;       /* Active fault (NONE if OK) */
    int                     connection_id;      /* CIP connection handle */
    pointio_connection_state_t conn_state;      /* Connection FSM state */
    uint32_t                last_update_us;     /* Last I/O update timestamp */
    uint32_t                scan_count;         /* Total scan cycles performed */

    /* --- Data image (flat memory) --- */
    uint8_t                 input_data[POINTIO_MAX_DATA_SIZE];
    uint8_t                 output_data[POINTIO_MAX_DATA_SIZE];
    uint8_t                 config_data[POINTIO_MAX_DATA_SIZE];
} pointio_module_t;

/**
 * @brief Point I/O chassis/rack descriptor.
 *
 * Represents the complete PointBus backplane with adapter + I/O modules.
 */
typedef struct {
    uint8_t             num_modules;            /* Active module count */
    pointio_module_t    modules[POINTIO_MAX_MODULES]; /* Indexed by slot */
    uint8_t             adapter_slot;           /* Slot 0 typically */
    pointio_power_budget_t budget;              /* System power budget */
    char                chassis_name[32];       /* User-defined name */
    pointio_conn_stats_t comm_stats;            /* Communication statistics */
} pointio_chassis_t;

/*===========================================================================
 * L1: Module API - Create / Destroy / Configure
 *===========================================================================*/

/**
 * @brief Initialize a chassis to safe defaults.
 *
 * All modules are set to NOT_PRESENT. Adapter is at slot 0.
 *
 * @param chassis  Pointer to chassis struct (must not be NULL)
 */
void pointio_chassis_init(pointio_chassis_t *chassis);

/**
 * @brief Initialize a single module to safe defaults.
 *
 * @param mod   Pointer to module struct
 * @param slot  Physical slot on PointBus
 */
void pointio_module_init(pointio_module_t *mod, uint8_t slot);

/**
 * @brief Configure a module as a digital input type.
 *
 * @param mod          Module to configure
 * @param catalog      Catalog number string (e.g. "1734-IB8")
 * @param num_channels Number of digital inputs
 * @param filter       Input filter time selection
 * @param name         User tag name
 * @return 0 on success, -1 if invalid parameters
 */
int pointio_module_config_digital_input(pointio_module_t *mod,
    const char *catalog, uint8_t num_channels,
    pointio_input_filter_t filter, const char *name);

/**
 * @brief Configure a module as a digital output type.
 *
 * @param mod          Module to configure
 * @param catalog      Catalog number string (e.g. "1734-OB8")
 * @param num_channels Number of digital outputs
 * @param fault_mode   Output state on fault
 * @param prog_mode    Output state in program mode
 * @param name         User tag name
 * @return 0 on success, -1 if invalid parameters
 */
int pointio_module_config_digital_output(pointio_module_t *mod,
    const char *catalog, uint8_t num_channels,
    pointio_fault_mode_t fault_mode, pointio_prog_mode_t prog_mode,
    const char *name);

/**
 * @brief Configure a module as an analog input type.
 *
 * @param mod           Module to configure
 * @param catalog       Catalog number string (e.g. "1734-IE4C")
 * @param num_channels  Number of analog input channels
 * @param range         Input range (4-20mA, 0-10V, etc.)
 * @param scaling       Array of scaling configs (one per channel)
 * @param name          User tag name
 * @return 0 on success, -1 if invalid parameters
 */
int pointio_module_config_analog_input(pointio_module_t *mod,
    const char *catalog, uint8_t num_channels,
    pointio_analog_range_t range,
    const pointio_analog_scaling_t *scaling, const char *name);

/**
 * @brief Configure a module as an analog output type.
 *
 * @param mod           Module to configure
 * @param catalog       Catalog number string (e.g. "1734-OE2C")
 * @param num_channels  Number of analog output channels
 * @param range         Output range (4-20mA, 0-10V, etc.)
 * @param name          User tag name
 * @return 0 on success, -1 if invalid parameters
 */
int pointio_module_config_analog_output(pointio_module_t *mod,
    const char *catalog, uint8_t num_channels,
    pointio_analog_output_range_t range, const char *name);

/*===========================================================================
 * L2: Module Management - Chassis Assembly
 *===========================================================================*/

/**
 * @brief Add a pre-configured module to the chassis at the given slot.
 *
 * The slot position must be free (NOT_PRESENT). The module is deep-copied
 * into the chassis array.
 *
 * @param chassis  The chassis
 * @param mod      The configured module
 * @return 0 on success, -1 on slot conflict or chassis full
 */
int pointio_chassis_add_module(pointio_chassis_t *chassis, const pointio_module_t *mod);

/**
 * @brief Remove a module from a slot.
 *
 * Sets the slot to NOT_PRESENT and clears the module data.
 *
 * @param chassis  The chassis
 * @param slot     Slot to clear
 * @return 0 on success, -1 if slot is adapter or out of range
 */
int pointio_chassis_remove_module(pointio_chassis_t *chassis, uint8_t slot);

/**
 * @brief Find a module by its user-defined name.
 *
 * @param chassis  The chassis
 * @param name     Tag name to search for
 * @return Pointer to module, or NULL if not found
 */
pointio_module_t *pointio_chassis_find_module(pointio_chassis_t *chassis, const char *name);

/*===========================================================================
 * L4: Power Budget Calculation
 *===========================================================================*/

/**
 * @brief Calculate the total power budget for a chassis.
 *
 * Sums all module power draws and compares against adapter/capacity limits.
 * The 1734-AENT adapter supplies 1000 mA at 5V on the PointBus backplane.
 * Field power is supplied separately (1734-FPD or external).
 *
 * Reference: 1734-SG001, "Point I/O Selection Guide", Power Specifications
 *
 * @param chassis  The assembled chassis
 * @param budget   Output: populated power budget struct
 * @return 0 if within limits, 1 if overloaded, -1 on error
 */
int pointio_calculate_power_budget(const pointio_chassis_t *chassis,
    pointio_power_budget_t *budget);

/*===========================================================================
 * L5: Sequence-of-Events (SOE) Support
 *===========================================================================*/

/**
 * @brief Read timestamped event data from a module's SOE buffer.
 *
 * For digital input modules that support Change-of-State (COS) with timestamp,
 * this retrieves the next unread event from the module's internal buffer.
 *
 * @param mod       The input module
 * @param entry     Output: the SOE entry
 * @return Number of events retrieved (0 or 1), -1 if SOE not supported
 */
int pointio_module_read_soe(pointio_module_t *mod, pointio_soe_entry_t *entry);

/*===========================================================================
 * L7: CompactLogix CPU Property Query
 *===========================================================================*/

/**
 * @brief Get the property table for a CompactLogix CPU model.
 *
 * Provides memory, I/O, and motion axis capacities for capacity planning.
 *
 * @param model  CPU model enum
 * @param cpu    Output: populated CPU properties
 * @return 0 on success, -1 for unknown model
 */
int compactlogix_get_cpu_properties(compactlogix_cpu_model_t model,
    compactlogix_cpu_t *cpu);

/**
 * @brief Check if a given I/O configuration fits within CPU limits.
 *
 * @param cpu          CPU properties
 * @param num_modules  Number of modules in I/O tree
 * @param node_count   Number of Ethernet nodes
 * @return 1 if fits, 0 if exceeds, -1 on error
 */
int compactlogix_check_capacity(const compactlogix_cpu_t *cpu,
    uint32_t num_modules, uint32_t node_count);

#endif /* POINTIO_MODULE_H */
