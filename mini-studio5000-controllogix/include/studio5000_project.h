/**
 * @file    studio5000_project.h
 * @brief   L1-L2: Studio 5000 Logix Designer Project Organization
 *
 * Knowledge Mapping:
 *   L1 Definitions:  Controller Organizer, Tasks, Programs, Routines,
 *                    Task types (Continuous/Periodic/Event), scheduling
 *   L2 Concepts:     Project scope hierarchy, program/phasing model,
 *                    scheduling priority, watchdogs, Overlap handling
 *   L3 Structures:   Task scheduling, periodic timing, jitter analysis
 *   L7 Applications: Rockwell Studio 5000 Logix Designer v20-v36
 *
 * Reference: Rockwell Automation Publication 1756-PM005
 *   "Logix5000 Controllers Design Considerations"
 *
 * Course Alignment:
 *   RWTH Aachen - Industrial Control Systems: IEC 61131-3 program organization
 *   Purdue ME 575 - Industrial Control: real-time scheduling
 *   ISA-88 - Batch Control: program/phasing model
 */

#ifndef STUDIO5000_PROJECT_H
#define STUDIO5000_PROJECT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ========================================================================
 * L1: Task Types (IEC 61131-3 / Logix 5000)
 * ======================================================================== */

typedef enum {
    TASK_TYPE_CONTINUOUS = 0,
    TASK_TYPE_PERIODIC   = 1,
    TASK_TYPE_EVENT      = 2
} logix_task_type_t;

typedef enum {
    OVERLAP_IGNORE        = 0,
    OVERLAP_CRUSH         = 1,
    OVERLAP_MINOR_FAULT   = 2
} logix_overlap_policy_t;

typedef struct {
    char                 name[64];
    logix_task_type_t    type;
    uint32_t             priority;
    uint32_t             period_us;
    uint32_t             watchdog_us;
    logix_overlap_policy_t overlap_policy;
    bool                 disable_automatic_output_processing;
    uint32_t             program_count;
    uint32_t             program_indices[100];
    int64_t              last_scan_time_ns;
    int64_t              max_scan_time_ns;
    int64_t              last_trigger_time_ns;
    bool                 overlap_occurred;
    bool                 task_enabled;
} logix_task_t;

typedef enum {
    ROUTINE_TYPE_MAIN     = 0,
    ROUTINE_TYPE_SUB      = 1,
    ROUTINE_TYPE_FAULT    = 2
} logix_routine_type_t;

typedef enum {
    ROUTINE_LANG_LADDER      = 0,
    ROUTINE_LANG_FBD         = 1,
    ROUTINE_LANG_STRUCT_TEXT = 2,
    ROUTINE_LANG_SFC         = 3
} logix_routine_language_t;

typedef struct {
    char                     name[64];
    logix_routine_type_t     type;
    logix_routine_language_t language;
    uint32_t                 rung_count;
    uint32_t                 argument_count;
    bool                     inhibit;
    bool                     is_safety;
    bool                     is_encrypted;
    bool                     is_sealed;
    uint32_t                 instruction_count;
    uint32_t                 data_size_bytes;
} logix_routine_t;

typedef struct {
    char                 name[64];
    bool                 disabled;
    bool                 is_phase_manager;
    logix_routine_t      main_routine;
    logix_routine_t      fault_routine;
    uint32_t             subroutine_count;
    uint32_t             subroutine_indices[500];
    uint32_t             program_tag_count;
    uint32_t             input_parameter_count;
    uint32_t             output_parameter_count;
    bool                 use_physical_download;
} logix_program_t;

typedef struct {
    char            controller_name[64];
    uint32_t        project_major_revision;
    uint32_t        project_minor_revision;
    char            description[256];
    uint32_t        major_fault_program_index;
    uint32_t        power_up_program_index;
    bool            redundant_controller_enabled;
    uint32_t        task_count;
    uint32_t        task_indices[32];
    uint32_t        program_count;
    uint32_t        program_indices[100];
    uint32_t        aoi_count;
    uint32_t        udt_count;
    uint32_t        module_count;
    uint32_t        firmware_compat_major;
    uint32_t        firmware_compat_minor;
    bool            safety_signature_exists;
    bool            safety_locked;
    bool            online_edits_pending;
    bool            forces_installed;
    bool            forces_enabled;
} studio5000_project_t;

typedef struct {
    int64_t     system_overhead_slice_ns;
    double      cpu_utilization_pct;
    int64_t     last_input_scan_time_ns;
    int64_t     last_scan_time_ns;
    int64_t     last_output_scan_time_ns;
    int64_t     last_overhead_time_ns;
    uint32_t    task_switch_count;
    uint32_t    interrupt_count;
} logix_scan_cycle_t;

typedef struct {
    uint32_t    task_index;
    int64_t     target_period_ns;
    int64_t     min_measured_period_ns;
    int64_t     max_measured_period_ns;
    int64_t     avg_measured_period_ns;
    double      jitter_us;
    double      jitter_percent;
    uint32_t    sample_count;
} logix_jitter_analysis_t;

/* API */
bool studio5000_project_init(studio5000_project_t *project,
                              const char *name,
                              uint32_t fw_major,
                              uint32_t fw_minor);

int32_t studio5000_task_create(studio5000_project_t *project,
                                const char *name,
                                logix_task_type_t type,
                                uint32_t priority,
                                uint32_t period_us,
                                uint32_t watchdog_us);

int32_t studio5000_program_create(studio5000_project_t *project,
                                   uint32_t task_index,
                                   const char *name,
                                   bool is_phase_manager);

bool studio5000_program_add_subroutine(studio5000_project_t *project,
                                         uint32_t program_index,
                                         const char *name,
                                         logix_routine_language_t language);

uint32_t studio5000_project_validate(const studio5000_project_t *project);

uint32_t studio5000_get_task_schedule(const studio5000_project_t *project,
                                       uint32_t *ordered_indices);

double studio5000_calculate_cpu_utilization(const studio5000_project_t *project,
                                              const int64_t *wcet_ns);

bool logix_analyze_task_jitter(const logix_task_t *task,
                                const int64_t *trigger_timestamps_ns,
                                uint32_t sample_count,
                                logix_jitter_analysis_t *result);

bool studio5000_is_schedule_feasible(const studio5000_project_t *project,
                                       const int64_t *wcet_ns);

#endif /* STUDIO5000_PROJECT_H */
