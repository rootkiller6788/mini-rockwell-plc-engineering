/**
 * @file    logix_iec61131_compliance.h
 * @brief   L4: IEC 61131-3 Compliance and Programming Standards
 *
 * Knowledge Mapping:
 *   L4 Standards:      IEC 61131-3 programming languages (LD, FBD, ST, SFC,
 *                      IL-deprecated), POU types, data types standard,
 *                      Rockwell deviations from standard
 *   L1 Definitions:    Program Organization Unit (POU), Function Block
 *                      vs Function, EN/ENO pattern
 *   L7 Applications:   Studio 5000 IEC 61131-3 compliance mapping
 *
 * IEC 61131-3 defines five programming languages for PLCs:
 *   - LD  (Ladder Diagram): Graphical, relay-like, most widely used
 *   - FBD (Function Block Diagram): Graphical, process control oriented
 *   - ST  (Structured Text): Textual, Pascal-like, complex algorithms
 *   - SFC (Sequential Function Chart): Graphical, step/transition
 *   - IL  (Instruction List): Textual, assembly-like (deprecated in 3rd Ed.)
 *
 * ControlLogix/Studio 5000 implements all four active languages with
 * some Rockwell-specific extensions and deviations.
 *
 * Reference: IEC 61131-3:2013 "Programmable controllers - Part 3:
 *            Programming languages"
 *            Karl-Heinz John (2013), "IEC 61131-3: Programming Industrial
 *            Automation Systems"
 *
 * Course Alignment:
 *   RWTH Aachen - PLC/SCADA Engineering: IEC 61131-3 fundamentals
 *   Purdue ME 575 - Industrial Control: PLC programming standards
 *   ISA/IEC - International industrial standards
 */

#ifndef LOGIX_IEC61131_COMPLIANCE_H
#define LOGIX_IEC61131_COMPLIANCE_H

#include <stdint.h>
#include <stdbool.h>
#include "studio5000_project.h"
#include "logix_tag_model.h"

/* ========================================================================
 * L4: IEC 61131-3 Language Classification
 * ======================================================================== */

/**
 * @brief POU (Program Organization Unit) types per IEC 61131-3
 *
 * Three POU types form the execution hierarchy:
 *   - PROGRAM: Top-level, contains tasks, has no return value.
 *             Allocated to tasks in the controller organizer.
 *   - FUNCTION_BLOCK: Instantiable, maintains internal state, returns
 *                     values through output parameters.
 *   - FUNCTION: Stateless, returns a single primary value, no internal
 *               memory (except for edge detection). e.g., ADD, SQRT.
 *
 * Rockwell mapping:
 *   PROGRAM  -> Logix Program (in Controller Organizer)
 *   FB       -> Add-On Instruction (AOI)
 *   FUNCTION -> Built-in instructions (ADD, MUL, MOV, etc.)
 *
 * Reference: IEC 61131-3:2013, Clause 4 "Software Model"
 */
typedef enum {
    POU_TYPE_PROGRAM        = 0,
    POU_TYPE_FUNCTION_BLOCK = 1,
    POU_TYPE_FUNCTION       = 2
} iec_pou_type_t;

/**
 * @brief IEC 61131-3 standard compliance level
 *
 * Studio 5000 is generally compliant with many IEC 61131-3 features
 * but has notable Rockwell-specific extensions:
 *
 * Compliant:
 *   + Standard data types (BOOL, SINT, INT, DINT, REAL)
 *   + Array types (up to 3 dimensions)
 *   + User-defined structure types (UDT)
 *   + All four graphical/textual languages
 *   + EN/ENO pattern (EnableIn/EnableOut in Logix)
 *
 * Non-compliant / Extended:
 *   - No direct IL support (deprecated in IEC 61131-3 3rd Ed anyway)
 *   - STRING type has different max length (82 vs IEC's default 255)
 *   - LINT, USINT, UINT, UDINT, ULINT, LREAL added (IEC 61131-3 3rd Ed)
 *   - AOI vs standard Function Block (similar but not identical)
 *   - Program-scope vs Controller-scope tag visibility (Rockwell extension)
 *   - No VAR_EXTERNAL / VAR_ACCESS for cross-POU access (uses scope instead)
 *   - No CONFIGURATION/RESOURCE hierarchy (uses flat controller model)
 *   - Timer/Counter are instructions, not standard FBs
 *
 * Reference: Rockwell Publication PROCES-RM001
 *            "IEC 61131-3 Compliance in Logix Controllers"
 */
typedef enum {
    IEC_COMPLIANCE_FULL      = 0,  /* Fully conformant */
    IEC_COMPLIANCE_EXTENDED  = 1,  /* Conformant with extensions */
    IEC_COMPLIANCE_PARTIAL   = 2,  /* Partially conformant */
    IEC_COMPLIANCE_NON       = 3   /* Not conformant */
} iec_compliance_level_t;

/* ========================================================================
 * L4: Ladder Logic (LD) Structure
 *
 * Ladder Diagram is the most widely used language in ControlLogix.
 * It represents logic as power rails with contacts and coils:
 *
 * Left power rail ---[ ]---[/]---( )--- Right power rail
 *                    NO     NC    Coil
 *
 * Execution order:
 *   - Rungs execute top-to-bottom, left-to-right
 *   - Each rung evaluated once per scan
 *   - Outputs updated only at end of rung (not immediately)
 *   - Subroutine calls via JSR instruction
 *
 * Key LD elements in Logix:
 *   - XIC (Examine If Closed): TRUE when bit = 1
 *   - XIO (Examine If Open): TRUE when bit = 0
 *   - OTE (Output Energize): Set bit = rung condition
 *   - OTL/OTU (Output Latch/Unlatch): Retentive set/reset
 *   - ONS (One Shot): TRUE for one scan on rising edge
 *   - OSF/OSR (One Shot Falling/Rising): Edge detection
 *
 * Reference: 1756-RM003, "Ladder Diagram Programming"
 * ======================================================================== */

typedef enum {
    LD_ELEMENT_XIC   = 0,  /* Examine If Closed (normally open contact) */
    LD_ELEMENT_XIO   = 1,  /* Examine If Open (normally closed contact) */
    LD_ELEMENT_OTE   = 2,  /* Output Energize (coil) */
    LD_ELEMENT_OTL   = 3,  /* Output Latch (set) */
    LD_ELEMENT_OTU   = 4,  /* Output Unlatch (reset) */
    LD_ELEMENT_ONS   = 5,  /* One Shot (rising edge detection) */
    LD_ELEMENT_OSF   = 6,  /* One Shot Falling */
    LD_ELEMENT_OSR   = 7,  /* One Shot Rising (alternate) */
    LD_ELEMENT_CTU   = 8,  /* Count Up instruction block */
    LD_ELEMENT_CTD   = 9,  /* Count Down instruction block */
    LD_ELEMENT_TON   = 10, /* Timer On Delay block */
    LD_ELEMENT_TOF   = 11, /* Timer Off Delay block */
    LD_ELEMENT_JSR   = 12, /* Jump to Subroutine */
    LD_ELEMENT_RET   = 13, /* Return from Subroutine */
    LD_ELEMENT_MOV   = 14, /* Move data */
    LD_ELEMENT_CMP   = 15, /* Compare */
    LD_ELEMENT_ADD   = 16, /* Add */
    LD_ELEMENT_SUB   = 17, /* Subtract */
    LD_ELEMENT_MUL   = 18, /* Multiply */
    LD_ELEMENT_DIV   = 19, /* Divide */
    LD_ELEMENT_AOI   = 20  /* Add-On Instruction call */
} ld_element_type_t;

/**
 * @brief A single ladder element (contact, coil, or instruction)
 */
typedef struct {
    ld_element_type_t type;
    char              operand_tag[40];  /* Tag associated with this element */
    uint32_t          column;           /* Visual column position */
    bool              is_series;        /* True = series (AND), False = parallel (OR) */
    bool              negated;          /* Invert logic */
    bool              input_state;      /* Evaluated input state from left */
    bool              output_state;     /* Computed output state to right */
} ld_element_t;

/**
 * @brief A ladder logic rung (horizontal network)
 *
 * A rung consists of a series-parallel network of contacts leading
 * to one or more output coils/instructions.
 */
typedef struct {
    uint32_t        rung_number;
    char            comment[256];
    uint32_t        element_count;
    ld_element_t    elements[32];
    bool            rung_condition_in;   /* From previous rung */
    bool            rung_condition_out;  /* To next rung */
    bool            is_inhibited;        /* AFI (Always False Instruction) */
} ld_rung_t;

/**
 * @brief L4: IEC 61131-3 POU interface definition
 *
 * Every POU has:
 *   - Declaration part: VAR, VAR_INPUT, VAR_OUTPUT, VAR_IN_OUT
 *   - Body: implementation in one of the five languages
 *   - Optional: VAR_EXTERNAL, VAR_GLOBAL, VAR_ACCESS
 *
 * In Logix:
 *   - Program tags = VAR (program-scope)
 *   - Controller tags = VAR_GLOBAL (controller-scope)
 *   - AOI Parameters = VAR_INPUT, VAR_OUTPUT, VAR_IN_OUT
 *
 * Reference: IEC 61131-3:2013, Clause 5 "Common Elements"
 */
typedef struct {
    iec_pou_type_t    pou_type;
    char              name[64];
    logix_routine_language_t language;
    uint32_t          input_var_count;
    uint32_t          output_var_count;
    uint32_t          in_out_var_count;
    uint32_t          var_count;         /* Internal variables */
    bool              has_en_eno;        /* EN/ENO pattern used */
} iec_pou_interface_t;

/* ========================================================================
 * L4: Structured Text (ST) Language Elements
 *
 * Structured Text is a high-level textual language resembling Pascal.
 * It's used for complex algorithms, loop calculations, and data
 * manipulation that would be cumbersome in ladder logic.
 *
 * Key constructs:
 *   IF...THEN...ELSIF...ELSE...END_IF
 *   CASE...OF...ELSE...END_CASE
 *   FOR...TO...BY...DO...END_FOR
 *   WHILE...DO...END_WHILE
 *   REPEAT...UNTIL...END_REPEAT
 *   RETURN, EXIT
 *
 * Operators:
 *   Arithmetic: +, -, *, /, MOD, ** (exponent, Logix extension)
 *   Comparison: =, <>, <, >, <=, >=
 *   Logical: AND, OR, NOT, XOR
 *   Bitwise: &, |, ^, ~ (Logix extension vs IEC's AND/OR)
 *   Assignment: :=  (IEC standard: := ; Logix also accepts :=)
 *
 * Studio 5000 ST differences from IEC 61131-3 ST:
 *   - Semicolons required after each statement (IEC: optional in some cases)
 *   - CASE requires ELSE clause in Logix (IEC: optional)
 *   - No direct string operations (use special instructions)
 *   - Array subscript: tag[i] vs IEC: tag[i] (same, but IEC allows [i,j])
 *
 * Reference: 1756-RM003, "Structured Text Programming"
 *            IEC 61131-3:2013, Clause 6.4 "Structured Text"
 * ======================================================================== */

typedef enum {
    ST_STMT_ASSIGNMENT = 0,   /* := */
    ST_STMT_IF         = 1,
    ST_STMT_FOR        = 2,
    ST_STMT_WHILE      = 3,
    ST_STMT_REPEAT     = 4,
    ST_STMT_CASE       = 5,
    ST_STMT_CALL       = 6,   /* Function/AOI call */
    ST_STMT_RETURN     = 7,
    ST_STMT_EXIT       = 8,
    ST_STMT_EMPTY      = 9
} st_statement_type_t;

/**
 * @brief IEC 61131-3 data type mapping
 *
 * Maps IEC 61131-3 standard types to ControlLogix types
 * and documents compliance level for each.
 */
typedef struct {
    logix_atomic_type_t    logix_type;
    const char            *iec_name;
    uint32_t               iec_size_bytes;   /* IEC standard size */
    uint32_t               logix_size_bytes; /* Actual Logix size */
    iec_compliance_level_t compliance;
} iec_type_mapping_t;

/* ========================================================================
 * L4: SFC (Sequential Function Chart) Structure
 *
 * SFC is a graphical language for sequential control based on
 * Grafcet (IEC 60848). It consists of:
 *   - Steps: Active or inactive states
 *   - Transitions: Conditions to move between steps
 *   - Actions: Operations performed while step is active
 *   - Branches: Selection (divergence/convergence) and Parallel
 *
 * Studio 5000 SFC restrictions:
 *   - Maximum 255 steps per SFC
 *   - One initial step (automatically active at start)
 *   - Actions can be LD, FBD, or ST (not SFC)
 *   - Transitions must be a single BOOL expression
 *
 * Reference: 1756-RM003, "SFC Programming"
 *            IEC 61131-3:2013, Clause 6.5 "Sequential Function Chart"
 * ======================================================================== */

typedef enum {
    SFC_ELEMENT_INITIAL_STEP   = 0,
    SFC_ELEMENT_STEP           = 1,
    SFC_ELEMENT_TRANSITION     = 2,
    SFC_ELEMENT_SELECT_DIVERGE = 3,  /* Single horizontal line branching */
    SFC_ELEMENT_SELECT_CONVERGE = 4,
    SFC_ELEMENT_PARALLEL_DIVERGE = 5,  /* Double horizontal line */
    SFC_ELEMENT_PARALLEL_CONVERGE = 6,
    SFC_ELEMENT_JUMP           = 7
} sfc_element_type_t;

typedef enum {
    SFC_ACTION_QUALIFIER_N     = 0,  /* Non-stored: active while step active */
    SFC_ACTION_QUALIFIER_S     = 1,  /* Set (stored): active until reset */
    SFC_ACTION_QUALIFIER_R     = 2,  /* Reset: deactivate stored action */
    SFC_ACTION_QUALIFIER_L     = 3,  /* Time Limited: max N seconds */
    SFC_ACTION_QUALIFIER_D     = 4,  /* Delayed: start after N seconds */
    SFC_ACTION_QUALIFIER_P     = 5,  /* Pulse: one scan when step activates */
    SFC_ACTION_QUALIFIER_P0    = 6,  /* Pulse (falling): one scan when step deactivates */
    SFC_ACTION_QUALIFIER_SD    = 7   /* Stored & Delayed */
} sfc_action_qualifier_t;

typedef struct {
    char                    step_name[40];
    sfc_element_type_t      type;
    bool                    active;
    uint32_t                action_count;
    bool                    timer_enabled;
    double                  timer_preset_sec;
    double                  timer_accum_sec;
    uint32_t                transition_index;  /* Next transition to evaluate */
} sfc_step_t;

typedef struct {
    sfc_action_qualifier_t  qualifier;
    char                    action_name[40];
    char                    action_tag[40];     /* BOOL tag to control */
    double                  time_parameter_sec;
    bool                    active;
} sfc_action_t;

typedef struct {
    uint32_t    transition_number;
    char        condition_tag[40];   /* BOOL tag for transition condition */
    bool        condition_true;
    bool        evaluated;
} sfc_transition_t;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief Map IEC 61131-3 compliance level for a given Logix type
 *
 * Returns compliance information for the type mapping.
 * Complexity: O(1)
 */
iec_compliance_level_t logix_iec_get_compliance(logix_atomic_type_t type);

/**
 * @brief Get IEC 61131-3 standard name for a Logix data type
 * Complexity: O(1)
 */
const char *logix_iec_get_standard_name(logix_atomic_type_t type);

/**
 * @brief Check if a tag name is a reserved IEC 61131-3 keyword
 *
 * Reserved keywords cannot be used as tag/UDT/AOI names:
 *   PROGRAM, FUNCTION_BLOCK, FUNCTION, VAR, VAR_INPUT, VAR_OUTPUT,
 *   IF, THEN, ELSE, CASE, FOR, WHILE, REPEAT, RETURN, TRUE, FALSE,
 *   TON, TOF, CTU, CTD, etc.
 *
 * @return true if name is reserved
 * Complexity: O(1) hash lookup
 */
bool logix_iec_is_reserved_keyword(const char *name);

/**
 * @brief Validate a ladder rung structure
 *
 * Checks:
 *   - At least one output element
 *   - No orphaned contacts (must be connected to output)
 *   - Proper series/parallel topology
 *   - Element count within limits
 *
 * @return number of errors (0 = valid)
 * Complexity: O(n) where n = element count
 */
uint32_t logix_iec_validate_ladder_rung(const ld_rung_t *rung);

/**
 * @brief Evaluate a ladder rung (compute output states)
 *
 * Traverses the series-parallel network left-to-right to compute
 * the rung condition in/out and all output states.
 *
 * @param rung Rung to evaluate
 * @return Rung condition out (TRUE if outputs should energize)
 * Complexity: O(n)
 * Reference: 1756-RM003, "Ladder Logic Execution"
 */
bool logix_iec_evaluate_ladder_rung(ld_rung_t *rung);

/**
 * @brief Validate IEC 61131-3 compliance of a POU definition
 *
 * Checks against IEC 61131-3 standard rules:
 *   - Name follows identifier rules
 *   - No circular dependencies
 *   - All variables have declared types
 *   - EN/ENO pattern properly implemented
 *   - FUNCTION must have exactly one output
 *
 * @return compliance level
 * Complexity: O(n)
 */
iec_compliance_level_t logix_iec_validate_pou(const iec_pou_interface_t *pou);

/**
 * @brief Get the standard IEC 61131-3 type size for a given type
 *
 * Useful for validating that Logix sizes match or exceed IEC requirements.
 * Complexity: O(1)
 */
uint32_t logix_iec_standard_type_size(logix_atomic_type_t type);

/**
 * @brief Translate a ladder rung to Structured Text equivalent
 *
 * Generates equivalent ST code for a given ladder rung.
 * Useful for documentation, review, and code export.
 *
 * @param rung      Ladder rung to translate
 * @param st_buffer [out] Buffer for generated ST code
 * @param buf_size  Size of output buffer
 * @return Number of characters written (0 on error)
 * Complexity: O(n)
 */
uint32_t logix_iec_ladder_to_st(const ld_rung_t *rung,
                                  char *st_buffer,
                                  uint32_t buf_size);

/**
 * @brief Check SFC structure for IEC 61131-3 compliance
 *
 * Validates:
 *   - Exactly one initial step
 *   - All transitions have associated conditions
 *   - No orphaned steps (unreachable from initial step)
 *   - Select branches have proper converge
 *   - Parallel branches have proper converge
 *   - Maximum nesting depth within limits
 *
 * @return number of errors (0 = valid)
 * Complexity: O(n) with reachability analysis
 */
uint32_t logix_iec_validate_sfc(const sfc_step_t *steps,
                                  uint32_t step_count,
                                  const sfc_transition_t *transitions,
                                  uint32_t transition_count);

#endif /* LOGIX_IEC61131_COMPLIANCE_H */