/**
 * @file plc_rockwell_aoi.h
 * @brief Add-On Instruction (AOI) Core ? Rockwell Studio 5000 / RSLogix 5000
 *
 * Knowledge Coverage: L1 Definitions ? L2 Core Concepts ? L3 Engineering Structures
 *
 * An Add-On Instruction (AOI) is a reusable instruction object that encapsulates
 * logic, parameters, local tags, and documentation into a single entity. AOIs are
 * the Rockwell equivalent of IEC 61131-3 Function Blocks but with Studio 5000
 * specific features: revision tracking, signature-based integrity, and source
 * protection.
 *
 * Reference: Rockwell Automation Publication 1756-PM010 (Logix 5000 Controllers
 *            Add-On Instructions Programming Manual)
 * Ref: IEC 61131-3 ?2.5.1.3 (Program Organization Units)
 *
 * Key distinctions AOI vs Subroutine:
 *   - AOI: Encapsulates parameters (Input/Output/InOut), local tags, scan mode
 *   - Subroutine: Flat parameter passing via JSR, no internal state
 *   - AOI: Support for EnableIn/EnableOut, instruction signature, revision
 *   - Subroutine: Simpler, but less maintainable for complex logic
 */

#ifndef PLC_ROCKWELL_AOI_H
#define PLC_ROCKWELL_AOI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Core Definitions ? AOI Parameter Direction
 * ============================================================================
 * In Studio 5000, AOI parameters have four visibility/direction modes.
 * These map to IEC 61131-3 VAR_INPUT, VAR_OUTPUT, VAR_IN_OUT respectively.
 * EnableIn/EnableOut is a Rockwell-specific extension controlling whether
 * the AOI logic executes based on rung conditions.
 */

/** AOI parameter direction ? maps to IEC 61131-3 + Rockwell extensions */
typedef enum {
    AOI_PARAM_INPUT    = 0,  /**< Read-only from caller (VAR_INPUT)              */
    AOI_PARAM_OUTPUT   = 1,  /**< Write-only to caller (VAR_OUTPUT)             */
    AOI_PARAM_INOUT    = 2,  /**< Read-write, passed by reference (VAR_IN_OUT)  */
    AOI_PARAM_ENABLEIN = 3,  /**< Rockwell-specific: rung-condition-in          */
    AOI_PARAM_ENABLEOUT= 4   /**< Rockwell-specific: rung-condition-out         */
} aoi_param_direction_t;

/* ============================================================================
 * L1: Core Definitions ? AOI Parameter Data Types
 * ============================================================================
 * Studio 5000 atomic data types. These correspond to IEC 61131-3 elementary
 * types but use Rockwell naming conventions. Bit-level types follow Logix 5000
 * memory model: 32-bit DINT is the fundamental unit; BOOL occupies 32 bits
 * in a tag but packs into BOOL arrays.
 */

/** AOI atomic data type ? equivalent to IEC 61131-3 elementary types */
typedef enum {
    AOI_TYPE_BOOL   = 0,  /**< Boolean (IEC: BOOL) ? Logix stores as 32-bit   */
    AOI_TYPE_SINT   = 1,  /**< Signed 8-bit integer (IEC: SINT)               */
    AOI_TYPE_INT    = 2,  /**< Signed 16-bit integer (IEC: INT)               */
    AOI_TYPE_DINT   = 3,  /**< Signed 32-bit integer (IEC: DINT) ? native word */
    AOI_TYPE_LINT   = 4,  /**< Signed 64-bit integer (Logix v21+, IEC: LINT)  */
    AOI_TYPE_REAL   = 5,  /**< IEEE 754 single-precision float (IEC: REAL)    */
    AOI_TYPE_LREAL  = 6,  /**< IEEE 754 double-precision float (Logix v33+)   */
    AOI_TYPE_STRING = 7,  /**< CHARACTER array with .LEN (IEC: STRING)         */
    AOI_TYPE_DWORD  = 8,  /**< 32-bit bit-string (DWORD)                       */
    AOI_TYPE_LWORD  = 9,  /**< 64-bit bit-string (LWORD)                       */
    AOI_TYPE_COUNTER= 10, /**< Preset counter structure (CTU/CTD preset)       */
    AOI_TYPE_TIMER  = 11, /**< Timer structure (.PRE/.ACC/.EN/.TT/.DN)        */
    AOI_TYPE_UDT    = 12, /**< User-Defined Type reference                     */
    AOI_TYPE_AOI_REF= 13  /**< Nested AOI reference                            */
} aoi_data_type_t;

/* ============================================================================
 * L1: Core Definitions ? Value Storage and Parameter Definition
 * ============================================================================ */

/** Atomic value storage ? union covers all elementary types */
typedef union {
    bool     bool_val;
    int8_t   sint_val;
    int16_t  int_val;
    int32_t  dint_val;
    int64_t  lint_val;
    float    real_val;
    double   lreal_val;
} aoi_atomic_value_t;

/** Single AOI parameter definition */
typedef struct {
    char                    name[40];
    aoi_param_direction_t   direction;
    aoi_data_type_t         data_type;
    bool                    required;
    bool                    visible;
    bool                    constant;
    char                    default_value[128];
    char                    description[256];
    char                    engineering_units[16];
    double                  min_value;
    double                  max_value;
    int32_t                 bit_mask;
} aoi_parameter_t;

/* ============================================================================
 * L2: Core Concepts ? AOI Scan Modes
 * ============================================================================
 * Studio 5000 v24+ supports scan mode overrides per AOI. This affects whether
 * the AOI consumes scan-time when EnableIn is false.
 *
 *   Mode           | Enabled | Disabled | Use Case
 *   ---------------|---------|----------|--------------------------
 *   OVERLOAD       | Exec    | Skip     | Default ? saves scan time
 *   PRESCAN_IMMUNE | Exec    | Skip     | Phased startup ordering
 *   ALWAYS_SCAN    | Exec    | Exec     | Safety monitoring only
 *
 * Ref: Rockwell KB QA1022 ? AOI Scan Mode Behavior
 */

/** AOI scan mode ? controls execution when EnableIn is false */
typedef enum {
    AOI_SCAN_OVERLOAD        = 0,
    AOI_SCAN_PRESCAN_IMMUNE  = 1,
    AOI_SCAN_ALWAYS_SCAN     = 2
} aoi_scan_mode_t;

/* ============================================================================
 * L2: Core Concepts ? AOI Signature & Revision
 * ============================================================================
 * When an AOI is defined with Generate Source Protection, Studio 5000 computes
 * a SHA-1 based instruction signature. Modifying the AOI changes the signature.
 * This is the mechanism behind "Sealed" AOIs in Logix v21+.
 *
 * The signature is computed over:
 *   - Parameter names, types, and order
 *   - Local tags (names and types only, not values)
 *   - Logic (rung topology, not tag names)
 *   - Scan mode, class, and category
 *
 * Ref: Rockwell KB BF12312 ? Signature Mismatch Troubleshooting
 */

#define AOI_SIGNATURE_LENGTH    20
#define AOI_REVISION_NOTE_LEN   128
#define AOI_MAX_PARAMETERS      64
#define AOI_MAX_LOCAL_TAGS      256
#define AOI_MAX_ROUTINES        16
#define AOI_MAX_NESTING_DEPTH   7
#define AOI_MAX_CONTEXT_OVERRIDES 32

/** AOI signature ? cryptographic identity of instruction definition */
typedef struct {
    uint8_t     digest[AOI_SIGNATURE_LENGTH];
    uint32_t    timestamp;
    uint32_t    change_counter;
} aoi_signature_t;

/** AOI revision entry ? version history tracking */
typedef struct {
    uint16_t    major_version;
    uint16_t    minor_version;
    char        revision_note[AOI_REVISION_NOTE_LEN];
    aoi_signature_t signature;
    bool        source_protected;
    bool        sealed;
} aoi_revision_t;

/* ============================================================================
 * L1/L2: AOI Local Tag ? Internal memory within AOI scope
 * ============================================================================ */

/** Local tag scoped to AOI ? invisible outside the instruction */
typedef struct {
    char            tag_name[40];
    aoi_data_type_t data_type;
    aoi_atomic_value_t initial_value;
    bool            retain_on_powerup;
    bool            is_array;
    uint16_t        array_dim[3];
    uint8_t         array_dims_count;
} aoi_local_tag_t;

/* ============================================================================
 * L1: Complete AOI Definition Structure
 * ============================================================================ */

/** Forward declarations */
struct aoi_instance_struct;
struct aoi_execution_context_struct;

/** Logic callback type */
typedef void (*aoi_logic_callback_t)(struct aoi_instance_struct* inst,
                                      const struct aoi_execution_context_struct* ctx,
                                      void* user_data);

/** AOI definition ? the "class" that instances are created from */
typedef struct {
    char                name[40];
    char                description[512];
    char                category[64];
    aoi_scan_mode_t     scan_mode;
    uint16_t            parameter_count;
    aoi_parameter_t     parameters[AOI_MAX_PARAMETERS];
    uint16_t            local_tag_count;
    aoi_local_tag_t     local_tags[AOI_MAX_LOCAL_TAGS];
    uint8_t             routine_count;
    bool                has_enablein;
    bool                has_enableout;
    aoi_revision_t      revision;
    char                source_key[64];
    bool                is_source_protected;
    bool                is_certified;
    uint32_t            estimated_scan_ns;
    uint16_t            memory_bytes;
    /* Context-sensitive parameter overrides (v31+) */
    uint16_t            csp_count;
    struct aoi_context_param_struct* csp_overrides;
    /* Registered logic callback */
    aoi_logic_callback_t logic_callback;
    void*               logic_user_data;
} aoi_definition_t;

/* ============================================================================
 * L2: AOI Instance ? Runtime memory for one usage of an AOI
 * ============================================================================ */

/** Single AOI instance ? runtime state for one usage */
typedef struct aoi_instance_struct {
    char                instance_name[40];
    aoi_definition_t*   definition;
    bool                enable_in;
    bool                enable_out;
    bool                error;
    uint32_t            error_code;
    aoi_atomic_value_t* parameter_values;
    aoi_atomic_value_t* local_tag_values;
    uint32_t            scan_counter;
    uint32_t            last_scan_us;
} aoi_instance_t;

/* ============================================================================
 * L2: Core Concepts ? AOI Execution Context
 * ============================================================================
 * Controller Scan Phases (Logix 5000):
 *   1. Input Scan ? read physical inputs -> input image table
 *   2. Program Scan ? execute all scheduled programs/tasks
 *   3. Output Scan ? write output image table -> physical outputs
 *   4. Housekeeping ? communication, diagnostics, system overhead
 */

/** Controller scan phase enumeration */
typedef enum {
    AOI_PHASE_PRESCAN      = 0,
    AOI_PHASE_NORMAL_SCAN  = 1,
    AOI_PHASE_POSTSCAN     = 2,
    AOI_PHASE_INHIBIT      = 3
} aoi_scan_phase_t;

/** AOI execution context ? passed to each AOI call */
typedef struct aoi_execution_context_struct {
    aoi_scan_phase_t    phase;
    uint32_t            scan_cycle_ms;
    uint32_t            elapsed_scan_us;
    uint32_t            watchdog_us;
    bool                first_scan;
    bool                fault_active;
    uint32_t            task_priority;
} aoi_execution_context_t;

/* ============================================================================
 * L3: Engineering Structures ? AOI Routine Organization
 * ============================================================================ */

/** AOI routine type */
typedef enum {
    AOI_ROUTINE_MAIN        = 0,
    AOI_ROUTINE_SUBROUTINE  = 1,
    AOI_ROUTINE_FAULT       = 2
} aoi_routine_type_t;

/** AOI routine ? a named logic container within an AOI */
typedef struct {
    char                name[40];
    aoi_routine_type_t  type;
    bool                is_safety;
    uint16_t            rung_count;
} aoi_routine_t;

/* ============================================================================
 * L2/L3: AOI Library ? Manage multiple AOI definitions (project scope)
 * ============================================================================ */

/** AOI library ? container for all AOIs in a Studio 5000 project */
typedef struct {
    char                project_name[64];
    char                controller_type[32];
    uint32_t            firmware_major;
    uint32_t            firmware_minor;
    uint16_t            aoi_count;
    aoi_definition_t*   aoi_definitions;
    uint16_t            aoi_capacity;
    aoi_signature_t     library_signature;
} aoi_library_t;

/* ============================================================================
 * L3: Engineering Structures ? Parameter Validation Rules
 * ============================================================================ */

/** AOI validation error codes */
typedef enum {
    AOI_VALID_OK                = 0,
    AOI_VALID_ERR_NAME_DUP      = 1,
    AOI_VALID_ERR_NAME_RESERVED = 2,
    AOI_VALID_ERR_TYPE_MISMATCH = 3,
    AOI_VALID_ERR_CONST_IMMUTABLE=4,
    AOI_VALID_ERR_REQUIRED_UNWIRED=5,
    AOI_VALID_ERR_REQUIRED_LADDER=6,
    AOI_VALID_ERR_INOUT_ATOMIC  =7,
    AOI_VALID_ERR_VISIBLE_HIDDEN=8,
    AOI_VALID_ERR_NESTING_DEPTH =9,
    AOI_VALID_ERR_SAFETY_NONCERT=10,
    AOI_VALID_ERR_SIGN_CHANGED  =11,
    AOI_VALID_ERR_ARRAY_BOUNDS  =12,
    AOI_VALID_ERR_STRING_OVERFLOW=13,
    AOI_VALID_ERR_PARAM_COUNT   =14,
    AOI_VALID_ERR_TAG_COUNT     =15
} aoi_validation_error_t;

/* ============================================================================
 * L3: Engineering Structures ? Context-Sensitive Parameter (CSP)
 * ============================================================================ */

/** Context types that affect parameter behavior */
typedef enum {
    AOI_CTX_STANDARD_TASK  = 0,
    AOI_CTX_SAFETY_TASK    = 1,
    AOI_CTX_EVENT_TASK     = 2,
    AOI_CTX_EQUIPMENT_PHASE= 3
} aoi_context_type_t;

/** Context-sensitive parameter override */
typedef struct aoi_context_param_struct {
    aoi_context_type_t context;
    uint16_t           param_index;
    bool               override_required;
    bool               new_required;
    bool               override_visible;
    bool               new_visible;
} aoi_context_param_t;

/* ============================================================================
 * L4: Engineering Laws ? GuardLogix Safety AOI Certification
 * ============================================================================
 * Safety AOIs in GuardLogix controllers must meet IEC 61508 SIL 2/3 requirements.
 * Ref: Rockwell SAFETY-AT020, IEC 61508-3 ?7.4.4
 */

/** Safety integrity level (from IEC 61508 / 61511) */
typedef enum {
    AOI_SIL_NONE  = 0,
    AOI_SIL_1     = 1,
    AOI_SIL_2     = 2,
    AOI_SIL_3     = 3,
    AOI_SIL_4     = 4
} aoi_sil_level_t;

/** Safety AOI certification data */
typedef struct {
    aoi_sil_level_t     sil_level;
    char                cert_body[32];
    char                cert_number[48];
    uint32_t            cert_date_unix;
    uint32_t            cert_expiry_unix;
    bool                double_exec;
    uint32_t            max_response_ms;
    uint32_t            diagnostic_coverage;
    double              pfd_avg;
    double              pfh;
    bool                data_diverse;
    bool                temporal_diverse;
} aoi_safety_cert_t;

/* ============================================================================
 * L4: Engineering Laws ? IEC 61131-3 POU mapping
 * ============================================================================ */

/** IEC 61131-3 POU type enumeration */
typedef enum {
    POU_TYPE_PROGRAM   = 0,
    POU_TYPE_FB        = 1,
    POU_TYPE_FUNCTION  = 2,
    POU_TYPE_CLASS     = 3
} iec_pou_type_t;

/** IEC 61131-3 to Rockwell AOI mapping */
typedef struct {
    iec_pou_type_t      iec_type;
    aoi_definition_t*   rockwell_aoi;
    char                iec_namespace[64];
    bool                semantic_equivalent;
    char                deviations[1024];
} iec_aoi_mapping_t;

/* ============================================================================
 * L5: Algorithms/Methods ? SHA-1 Context for Signature Computation
 * ============================================================================ */

/** SHA-1 context for AOI signature computation */
typedef struct {
    uint32_t    state[5];
    uint64_t    byte_count;
    uint8_t     buffer[64];
    uint8_t     buffer_offset;
} aoi_sha1_ctx_t;

/* ============================================================================
 * Core API: AOI Definition Management
 * ============================================================================ */

aoi_definition_t* aoi_definition_create(const char* name, const char* description);
void aoi_definition_free(aoi_definition_t* def);

aoi_validation_error_t aoi_definition_add_parameter(
    aoi_definition_t* def, const char* name,
    aoi_param_direction_t direction, aoi_data_type_t data_type, bool required);

aoi_validation_error_t aoi_definition_add_local_tag(
    aoi_definition_t* def, const char* name,
    aoi_data_type_t data_type, bool retain_on_powerup);

void aoi_definition_set_scan_mode(aoi_definition_t* def, aoi_scan_mode_t mode);
void aoi_definition_set_safety_cert(aoi_definition_t* def, const aoi_safety_cert_t* cert);

/* ============================================================================
 * Core API: AOI Instance Management
 * ============================================================================ */

aoi_instance_t* aoi_instance_create(const char* instance_name, aoi_definition_t* def);
void aoi_instance_free(aoi_instance_t* inst);
void aoi_instance_reset(aoi_instance_t* inst);

/* ============================================================================
 * Core API: AOI Execution
 * ============================================================================ */

bool aoi_instance_execute(aoi_instance_t* inst, const aoi_execution_context_t* ctx);
void aoi_definition_set_logic_callback(aoi_definition_t* def,
                                        aoi_logic_callback_t callback, void* user_data);

/* ============================================================================
 * Core API: Parameter Value Access
 * ============================================================================ */

bool aoi_instance_get_param(aoi_instance_t* inst, const char* param_name,
                            aoi_atomic_value_t* value_out);
aoi_validation_error_t aoi_instance_set_param(aoi_instance_t* inst,
                                               const char* param_name,
                                               const aoi_atomic_value_t* value);
bool aoi_instance_get_local_tag(aoi_instance_t* inst, const char* tag_name,
                                aoi_atomic_value_t* value_out);
aoi_validation_error_t aoi_instance_set_local_tag(aoi_instance_t* inst,
                                                   const char* tag_name,
                                                   const aoi_atomic_value_t* value);
int32_t aoi_definition_get_param_index(const aoi_definition_t* def, const char* name);
aoi_atomic_value_t aoi_instance_get_param_by_index(const aoi_instance_t* inst,
                                                    uint16_t index);
void aoi_instance_set_param_by_index(aoi_instance_t* inst, uint16_t index,
                                      const aoi_atomic_value_t* value);

/* ============================================================================
 * Core API: Validation
 * ============================================================================ */

aoi_validation_error_t aoi_definition_validate(const aoi_definition_t* def);
aoi_validation_error_t aoi_instance_validate(const aoi_instance_t* inst);

/* ============================================================================
 * Core API: Signature & Revision
 * ============================================================================ */

void aoi_compute_signature(const aoi_definition_t* def, aoi_signature_t* sig_out);
bool aoi_signature_equal(const aoi_signature_t* a, const aoi_signature_t* b);

/* ============================================================================
 * Core API: AOI Library
 * ============================================================================ */

aoi_library_t* aoi_library_create(const char* project_name,
                                   const char* controller_type,
                                   uint32_t fw_major, uint32_t fw_minor);
void aoi_library_free(aoi_library_t* lib);
aoi_validation_error_t aoi_library_add_aoi(aoi_library_t* lib, aoi_definition_t* def);
aoi_definition_t* aoi_library_find_aoi(const aoi_library_t* lib, const char* name);
uint16_t aoi_library_count(const aoi_library_t* lib);
void aoi_library_compute_signature(aoi_library_t* lib);

/* ============================================================================
 * Core API: Context Sensitivity
 * ============================================================================ */

aoi_validation_error_t aoi_definition_add_context_override(
    aoi_definition_t* def, const aoi_context_param_t* ctx_param);
void aoi_definition_resolve_context(const aoi_definition_t* def,
                                     uint16_t param_index, aoi_context_type_t context,
                                     bool* effective_required_out,
                                     bool* effective_visible_out);

#ifdef __cplusplus
}
#endif

#endif /* PLC_ROCKWELL_AOI_H */
