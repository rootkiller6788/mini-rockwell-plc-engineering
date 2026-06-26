/**
 * @file    logix_udt_aoi.h
 * @brief   L1-L3: User-Defined Types and Add-On Instructions
 *
 * Knowledge Mapping:
 *   L1 Definitions:  UDT member types, AOI parameter classes (Input/Output/
 *                    InOut), AOI context structure, member visibility/required,
 *                    AOI revision management, source protection
 *   L2 Concepts:     Encapsulation and reuse, parameter passing (Input/Output/
 *                    InOut by value/reference), AOI local tags, AOI scan model,
 *                    nesting restrictions
 *   L3 Structures:   AOI memory layout, signature validation, revision locking,
 *                    online editing restrictions for AOIs
 *   L4 Standards:    ISA-88 modular programming, IEC 61131-3 POUs
 *   L7 Applications: Rockwell Studio 5000 AOI and UDT engineering
 *
 * Reference: Rockwell Automation Publication 1756-PM010
 *   "Logix5000 Controllers Add-On Instructions"
 *
 * Course Alignment:
 *   RWTH Aachen - Industrial Control Systems: POU reuse patterns
 *   CMU 24-677 - Adv Ctrl Systems: software component engineering
 *   ISA-88 - Batch Control: equipment module encapsulation
 */

#ifndef LOGIX_UDT_AOI_H
#define LOGIX_UDT_AOI_H

#include <stdint.h>
#include <stdbool.h>
#include "logix_tag_model.h"
#include "studio5000_project.h"

/* ========================================================================
 * L1: AOI Parameter Definitions
 * ======================================================================== */

/**
 * @brief AOI parameter direction
 *
 * Add-On Instructions define parameters that form the external interface:
 *   - Input: Passed by value FROM caller; read-only inside AOI
 *   - Output: Passed by reference FROM AOI; write-only by AOI
 *   - InOut: Passed by reference; read-write both inside and outside
 *   - Public local: Visible to HMI/SCADA for external access
 *
 * AOI parameters cannot be external access (HMI) writable unless
 * they are InOut parameters.
 *
 * Reference: 1756-PM010, Chapter 2 "AOI Parameters"
 */
typedef enum {
    AOI_PARAM_INPUT      = 0,
    AOI_PARAM_OUTPUT     = 1,
    AOI_PARAM_INOUT      = 2,
    AOI_PARAM_PUBLIC     = 3   /* Public local tag */
} aoi_param_direction_t;

/**
 * @brief AOI parameter definition
 */
typedef struct {
    char                   name[40];
    aoi_param_direction_t  direction;
    logix_atomic_type_t    data_type;
    uint32_t               udt_type_index;  /* If data_type == STRUCT */
    logix_array_desc_t     array_info;
    char                   default_value[64];
    char                   description[128];
    bool                   required;        /* Must be wired by caller */
    bool                   visible;         /* Visible in tag browser */
    bool                   constant;        /* Input constant (cannot change at runtime) */
    uint32_t               offset_bytes;    /* Byte offset in AOI context */
    uint32_t               size_bytes;
} aoi_parameter_t;

/**
 * @brief L1: AOI Routine (logic inside an AOI)
 *
 * An AOI can contain:
 *   - One Main logic routine
 *   - Optional EnableInFalse routine (executes when EnableIn is false)
 *   - Optional Prescan routine
 *   - Optional Postscan routine
 *   - Optional Fault routine
 *
 * Each routine is a standard Studio 5000 routine (LD/FBD/ST/SFC).
 *
 * Reference: 1756-PM010, Chapter 3 "AOI Logic"
 */
typedef struct {
    char                     name[64];
    logix_routine_language_t language;
    bool                     is_enable_in_false;
    bool                     is_prescan;
    bool                     is_postscan;
    bool                     is_fault;
    uint32_t                 rung_count;
    uint32_t                 instruction_count;
} aoi_routine_t;

/**
 * @brief L1-L2: Complete Add-On Instruction definition
 *
 * An AOI (Add-On Instruction) is a user-defined instruction that encapsulates
 * commonly used logic into a reusable block. Similar to a subroutine but with:
 *   - Defined interface (Input/Output/InOut parameters)
 *   - Encapsulated local tags (invisible outside)
 *   - Support for nesting (AOI can call other AOIs)
 *   - Revision management and signature validation
 *   - Source protection (encryption and license sealing)
 *   - Online editing restrictions (requires download to modify)
 *
 * AOI scan execution:
 *   if EnableIn is true:
 *       Execute Main logic routine
 *   else:
 *       Execute EnableInFalse routine (if defined)
 *       All Output parameters set to their default values
 *
 * AOI prescan (controller transition to Run):
 *   Execute Prescan routine (if defined)
 *   Initialize all Output parameters to defaults
 *
 * Restrictions:
 *   - AOIs cannot be edited online (requires download)
 *   - Maximum parameters: 256 (Input + Output + InOut)
 *   - Maximum nesting depth: 25 levels (firmware-dependent)
 *   - Safety AOIs: additional SIL-specific restrictions
 *
 * Reference: 1756-PM010, Chapter 1 "AOI Overview"
 */
typedef struct {
    char               name[40];
    char               description[256];
    uint32_t           major_revision;
    uint32_t           minor_revision;
    uint32_t           parameter_count;
    aoi_parameter_t    parameters[128];
    uint32_t           routine_count;
    aoi_routine_t      routines[4];
    uint32_t           local_tag_count;
    uint32_t           context_size_bytes;  /* Total runtime context size */
    bool               is_safety;           /* SIL-rated AOI */
    bool               is_encrypted;        /* Source protection enabled */
    bool               is_sealed;           /* License-protected */
    bool               enable_in_required;  /* EnableIn pin is visible */
    bool               enable_out_visible;  /* EnableOut pin is visible */
    char               revision_note[256];
    char               signature[32];       /* Revision signature hash */
} logix_aoi_t;

/**
 * @brief L3: AOI instance context (runtime data for one AOI call)
 *
 * When an AOI is instantiated (used in a routine), a context structure
 * is allocated containing:
 *   - Backing tag for the AOI instance (holds Input/Output/InOut values)
 *   - Local tag storage (visible only inside AOI)
 *   - Scan state flags (prescan active, enable last state, etc.)
 *
 * The backing tag is typically a UDT generated automatically by Studio 5000
 * with the AOI's parameter layout as member fields.
 */
typedef struct {
    uint32_t           aoi_definition_index;  /* Index into AOI definition table */
    char               instance_name[40];
    bool               enable_in;             /* Current EnableIn state */
    bool               enable_out;            /* Computed EnableOut state */
    bool               enable_in_prev;        /* Previous scan EnableIn state */
    bool               prescan_active;
    bool               postscan_active;
    bool               faulted;
    uint32_t           fault_code;
    uint8_t             context_data[4096];    /* Serialized context memory */
    uint32_t           context_size_used;
} aoi_instance_t;

/* ========================================================================
 * L2: AOI Nesting and Call Graph
 * ======================================================================== */

/**
 * @brief AOI call graph tracking
 *
 * The Controller Organizer validates that AOI call hierarchy does not
 * exceed maximum nesting depth and prevents circular references.
 *
 * AOI_A -> AOI_B -> AOI_C   (valid nesting, depth = 3)
 * AOI_A -> AOI_B -> AOI_A   (invalid: circular reference)
 *
 * Reference: 1756-PM010, "AOI Nesting Guidelines"
 */
typedef struct {
    uint32_t aoi_index;
    uint32_t called_aoi_indices[32];
    uint32_t called_count;
    uint32_t caller_count;
    bool     has_circular_ref;
    uint32_t max_nest_depth;
} aoi_call_graph_node_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief Define a new UDT with given name and description
 * Complexity: O(1)
 */
bool logix_udt_create(logix_udt_t *udt, const char *name,
                       const char *description);

/**
 * @brief Add a member to a UDT definition
 *
 * Automatically computes byte offset with proper alignment.
 * Validates that member name is unique within the UDT.
 *
 * @return true on success, false if max members exceeded or name duplicate
 * Complexity: O(1)
 */
bool logix_udt_add_member(logix_udt_t *udt, const char *name,
                            logix_atomic_type_t type,
                            uint32_t array_dim1,
                            uint32_t array_dim2,
                            uint32_t array_dim3);

/**
 * @brief Add a nested UDT member (member whose type is another UDT)
 * Complexity: O(1)
 */
bool logix_udt_add_udt_member(logix_udt_t *udt, const char *name,
                                uint32_t member_udt_index);

/**
 * @brief Create a new AOI definition
 *
 * Initializes the AOI with given name and creates an auto-generated
 * backing UDT for the AOI context tag.
 *
 * Complexity: O(1)
 */
bool logix_aoi_create(logix_aoi_t *aoi, const char *name,
                       const char *description);

/**
 * @brief Add a parameter to an AOI definition
 *
 * Parameters form the external interface of the AOI:
 *   - Input parameters read by AOI (set by caller)
 *   - Output parameters written by AOI (read by caller)
 *   - InOut parameters: read-write both directions
 *
 * @return true on success
 * Complexity: O(1)
 */
bool logix_aoi_add_parameter(logix_aoi_t *aoi, const char *name,
                               aoi_param_direction_t direction,
                               logix_atomic_type_t data_type,
                               bool required, bool visible);

/**
 * @brief Add a logic routine to an AOI
 * Complexity: O(1)
 */
bool logix_aoi_add_routine(logix_aoi_t *aoi, const char *name,
                             logix_routine_language_t language,
                             bool is_enable_in_false,
                             bool is_prescan);

/**
 * @brief Validate an AOI definition
 *
 * Checks:
 *   - Name is valid (no special characters, not reserved word)
 *   - At least one Output or InOut parameter (useful AOI)
 *   - Required parameters cannot have default values
 *   - No duplicate parameter names
 *   - EnableIn cannot share names with parameters
 *   - Nesting depth does not exceed maximum
 *   - No circular references in call graph
 *
 * @return number of validation errors (0 = valid)
 * Complexity: O(P + R) where P = params, R = routines
 */
uint32_t logix_aoi_validate(const logix_aoi_t *aoi);

/**
 * @brief Compute AOI context memory size
 *
 * Accounts for all Input/Output/InOut parameter sizes plus local tag
 * memory, with proper alignment padding.
 *
 * @return Total bytes needed for runtime context
 * Complexity: O(P) where P = parameter count
 */
uint32_t logix_aoi_context_size(const logix_aoi_t *aoi);

/**
 * @brief Instance an AOI (create a callable instance)
 *
 * Allocates the backing tag and context memory for the AOI instance.
 * Initializes all parameters to their default values.
 *
 * @return true on success
 * Complexity: O(P) for parameter copying
 */
bool logix_aoi_instantiate(const logix_aoi_t *aoi,
                             const char *instance_name,
                             aoi_instance_t *instance);

/**
 * @brief Generate the automatically-created backing UDT for an AOI
 *
 * Studio 5000 automatically generates a UDT for each AOI that serves
 * as the backing tag type. The UDT members correspond to the AOI
 * parameters in the order: Input, Output, InOut, Public Local.
 *
 * @param udt [out] Generated backing UDT
 * Complexity: O(P)
 */
void logix_aoi_generate_backing_udt(const logix_aoi_t *aoi,
                                      logix_udt_t *udt);

/**
 * @brief Check for circular reference in AOI call graph
 *
 * Uses depth-first search with cycle detection.
 *
 * @param aois        Array of all AOI definitions
 * @param aoi_count   Number of AOI definitions
 * @param call_graph  [out] Call graph with cycle detection results
 * @return true if any circular reference detected
 *
 * Complexity: O(V + E) where V = AOI count, E = call edges
 * Reference: Tarjan (1972) strongly connected components
 */
bool logix_aoi_detect_circular_ref(const logix_aoi_t *aois,
                                     uint32_t aoi_count,
                                     aoi_call_graph_node_t *call_graph);

/**
 * @brief Increment AOI revision and update signature
 *
 * AOI signature is a hash over:
 *   - AOI definition name
 *   - Parameter names, types, and order
 *   - Routine structure (language, rung count)
 *   - Revision major.minor numbers
 *
 * Changing any of these invalidates the signature, requiring
 * re-validation of all AOI instances.
 *
 * Complexity: O(P + R)
 */
void logix_aoi_update_signature(logix_aoi_t *aoi);

#endif /* LOGIX_UDT_AOI_H */