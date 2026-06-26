/**
 * @file plc_rockwell_tag.h
 * @brief Rockwell Tag System ? Tag-Based Addressing, Memory Model, Data Types
 *
 * Knowledge Coverage: L1 Definitions ? L2 Core Concepts ? L3 Engineering Structures
 *
 * Rockwell Logix 5000 uses a tag-based (name-value) memory model instead of
 * the traditional register-based addressing (N7:0, B3:1). Tags are text-named
 * variables with data types, scopes, and optionally array dimensions.
 *
 * Tag Scopes:
 *   - Controller-scope (global): Accessible from any task/program
 *   - Program-scope: Accessible only within the declaring program
 *   - Local (AOI/UDT): Scoped to an AOI instance or UDT member
 *
 * This is a fundamental differentiator from Siemens (DB-based) and Schneider
 * (%MW-based). Tags enable self-documenting code and are a prerequisite for
 * ISA-88 modular programming.
 *
 * Reference: Rockwell 1756-PM004 (Logix 5000 Controllers I/O and Tag Data)
 * Ref: IEC 61131-3 ?2.4.1.1 (Representation of data types)
 */

#ifndef PLC_ROCKWELL_TAG_H
#define PLC_ROCKWELL_TAG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1: Core Definitions ? Value storage for all Rockwell native types
 * ============================================================================ */

/** Union providing raw access to all Rockwell atomic data types */
typedef union {
    bool     bool_val;
    int8_t   sint_val;
    int16_t  int_val;
    int32_t  dint_val;
    int64_t  lint_val;
    float    real_val;
    double   lreal_val;
    uint32_t dword_val;
    uint64_t lword_val;
    /* For string: first 7 bytes + null of up to 16-char short string */
    char     short_str[16];
} plc_atomic_value_t;

/* ============================================================================
 * L1: Rockwell Data Type Classification
 * ============================================================================
 * The Logix 5000 type system has four categories:
 *   1. Atomic (Elementary) ? BOOL, SINT, INT, DINT, LINT, REAL, LREAL
 *   2. Predefined Structures ? TIMER, COUNTER, CONTROL, PID, etc.
 *   3. User-Defined Types (UDT) ? Custom struct with named members
 *   4. Array ? Multi-dimensional collection of any base type
 *
 * Type Identity: each type is uniquely identified by a 16-bit type code.
 * In Studio 5000, these codes are embedded in .ACD and .L5K files.
 */

/** Rockwell type classification */
typedef enum {
    PLC_TYPE_ATOMIC    = 0,   /**< Elementary type (BOOL, DINT, REAL, etc.)     */
    PLC_TYPE_PREDEF    = 1,   /**< Predefined structure (TIMER, MOTION, etc.)    */
    PLC_TYPE_UDT       = 2,   /**< User-Defined Type (custom aggregate)          */
    PLC_TYPE_ARRAY     = 3,   /**< Array of any base type                        */
    PLC_TYPE_STRING    = 4,   /**< STRING type with .LEN member                  */
    PLC_TYPE_ALIAS     = 5    /**< Alias to another tag (bidirectional link)     */
} plc_type_class_t;

/* ============================================================================
 * L1: Predefined Data Types ? Rockwell built-in structures
 * ============================================================================
 * These correspond to predefined data types in the Logix 5000 environment.
 * Each has a fixed memory layout documented in the Logix instruction set.
 */

/** TIMER structure: TON/TOF/RTO built-in, fixed 3-DINT layout */
typedef struct {
    int32_t     pre;     /**< Preset value (.PRE)                           */
    int32_t     acc;     /**< Accumulated value (.ACC)                      */
    uint32_t    status;  /**< Status bits: .EN(bit15)=0x8000 .TT(14)=0x4000
                              .DN(13)=0x2000                                */
} plc_timer_t;

/** COUNTER structure: CTU/CTD built-in, fixed 3-DINT layout */
typedef struct {
    int32_t     pre;     /**< Preset value (.PRE)                           */
    int32_t     acc;     /**< Accumulated value (.ACC)                      */
    uint32_t    status;  /**< Status bits: .CU(bit15) .CD(14) .DN(13)
                              .OV(12) .UN(11)                               */
} plc_counter_t;

/** CONTROL structure: used by FAL/FFL/FFU/LFL/LFU instructions */
typedef struct {
    int32_t     len;     /**< Number of elements to operate on (.LEN)        */
    int32_t     pos;     /**< Current position index (.POS)                 */
    uint32_t    status;  /**< .EN(bit15) .DN(13) .ER(11)                   */
} plc_control_t;

/** PID structure: built-in PID instruction, 18-DINT layout */
typedef struct {
    int32_t     pid_data[18];   /**< PID control block (82 REALs worth)     */
} plc_pid_t;

/** MOTION_GROUP: motion coordination group */
typedef struct {
    uint32_t    axis_count;
    uint32_t    group_status;
    double      command_position;
    double      actual_position;
} plc_motion_group_t;

/* ============================================================================
 * L2: Tag Scope ? Controller/Program/Local visibility
 * ============================================================================ */

/** Tag scope enumeration */
typedef enum {
    TAG_SCOPE_CONTROLLER = 0, /**< Global to all tasks/programs             */
    TAG_SCOPE_PROGRAM    = 1, /**< Scoped to a single program unit          */
    TAG_SCOPE_LOCAL      = 2  /**< Scoped to an AOI instance or UDT member */
} plc_tag_scope_t;

/* ============================================================================
 * L1: Complete Tag Definition Structure
 * ============================================================================
 * In Logix 5000, a tag is a named memory location with:
 *   - Name: Unique within its scope
 *   - Data Type: Atomic, predefined, UDT, or array
 *   - Scope: Controller (global) or Program (local)
 *   - Dimensions: For arrays, up to 3 dimensions
 *   - Value: The current runtime value
 *   - Style: Display radix (decimal, hex, binary, ASCII, float, exponential)
 *   - Class: Standard, Safety, Consumed, Produced
 *
 * Important engineering distinction: Logix tags are 32-bit aligned in memory.
 * A BOOL tag occupies 32 bits but only uses bit 0. This wastes space but
 * aligns with the 32-bit DINT-native CPU architecture.
 */

/** Display radix (formatting) for tag monitoring */
typedef enum {
    TAG_STYLE_DECIMAL     = 0,   /**< Signed decimal (default for DINT)     */
    TAG_STYLE_HEX         = 1,   /**< Hexadecimal                           */
    TAG_STYLE_BINARY      = 2,   /**< Binary string (0b...)                 */
    TAG_STYLE_OCTAL       = 3,   /**< Octal                                 */
    TAG_STYLE_ASCII       = 4,   /**< ASCII character                       */
    TAG_STYLE_FLOAT       = 5,   /**< Floating point (default for REAL)     */
    TAG_STYLE_EXPONENTIAL = 6,    /**< Scientific notation                   */
    TAG_STYLE_BOOLEAN     = 7     /**< 0/1 display                          */
} plc_tag_style_t;

/** Tag type class */
typedef enum {
    TAG_CLASS_STANDARD    = 0,    /**< Standard memory tag                  */
    TAG_CLASS_SAFETY      = 1,    /**< Safety tag (GuardLogix only)        */
    TAG_CLASS_CONSUMED    = 2,    /**< Consumed tag (from another controller) */
    TAG_CLASS_PRODUCED    = 3,    /**< Produced tag (sent to consumers)     */
    TAG_CLASS_CONSTANT    = 4     /**< Immutable constant                   */
} plc_tag_class_t;

/** External access type for produced/consumed tags */
typedef enum {
    TAG_EXTERNAL_NONE     = 0,    /**< No external access                   */
    TAG_EXTERNAL_READONLY = 1,    /**< Remote read-only                     */
    TAG_EXTERNAL_READWRITE= 2     /**< Remote read-write                    */
} plc_tag_external_t;

/** Single tag definition in the Logix tag database */
typedef struct {
    char                name[64];         /**< Tag name (case-insensitive)    */
    char                description[256]; /**< Tag description                */
    plc_tag_scope_t     scope;            /**< Controller/Program/Local      */
    char                owner[64];        /**< Program name for scope PROGRAM */
    plc_type_class_t    type_class;       /**< Atomic/Predf/UDT/Array/String */
    uint16_t            type_code;        /**< Numeric type identifier        */
    char                udt_type_name[40];/**< UDT name if type_class=UDT    */
    plc_tag_class_t     tag_class;        /**< Standard/Safety/Consumed/Prod */
    plc_tag_style_t     style;            /**< Default display style          */
    plc_tag_external_t  external_access;  /**< Produced/Consumed settings     */
    bool                is_array;         /**< Tag is array type             */
    uint16_t            array_dim[3];     /**< Dimensions, dims[0]=innermost */
    uint8_t             array_dims_count; /**< Number of array dimensions    */
    bool                retain_on_powerup;/**< Persists across power cycles   */
    uint32_t            memory_offset;    /**< Byte offset in tag memory     */
    uint32_t            memory_size;      /**< Allocated bytes (32-bit align) */
    plc_atomic_value_t  initial_value;    /**< Initial value for atomic tags */
    /* UDT member info */
    bool                is_udt_member;    /**< This tag is a member of a UDT */
    char                parent_udt[64];   /**< Parent UDT tag name           */
} plc_tag_t;

/* ============================================================================
 * L2: User-Defined Type (UDT) Definition
 * ============================================================================
 * A UDT (User-Defined Type) is a custom structured data type that groups
 * related data into a single named entity. UDTs are the Rockwell analog of
 * struct in C or TYPE...END_TYPE in IEC 61131-3.
 *
 * Key engineering concept: UDT members are stored contiguously in memory,
 * aligned to 32-bit boundaries (or 64-bit for LREAL on 64-bit controllers).
 * The total size is the sum of member sizes plus alignment padding.
 *
 * UDT limitations (Studio 5000 v36):
 *   - Max 65535 members per UDT
 *   - Max nesting depth: 8 levels
 *   - Max total size: 2 MB
 *   - Cannot contain AOI instances (use AOI_REF type instead)
 */

#define UDT_MAX_MEMBERS     256
#define UDT_MAX_NAME_LEN    40
#define UDT_MAX_MEMBER_NAME 40

/** Single UDT member definition */
typedef struct {
    char            name[UDT_MAX_MEMBER_NAME];
    plc_type_class_t type_class; /**< Atomic/Predf/UDT/Array/String       */
    uint16_t        type_code;
    char            nested_udt_name[40]; /**< For nested UDT members     */
    uint32_t        byte_offset;    /**< Byte offset within UDT struct    */
    uint32_t        byte_size;      /**< Size in bytes (32-bit aligned)   */
    bool            is_array;
    uint16_t        array_dim[3];
    uint8_t         array_dims_count;
    char            description[256];
    bool            hidden;         /**< Hidden member (internal use)      */
    bool            required;       /**< Must be assigned non-default value*/
    double          min_value;      /**< Bounds for numeric types          */
    double          max_value;
    char            engineering_units[16];
} plc_udt_member_t;

/** Complete UDT definition */
typedef struct {
    char                name[UDT_MAX_NAME_LEN];
    char                description[512];
    uint16_t            member_count;
    plc_udt_member_t    members[UDT_MAX_MEMBERS];
    uint32_t            total_byte_size; /**< Total padded size             */
    uint8_t             nesting_depth;   /**< UDT-within-UDT depth          */
    bool                is_certified;    /**< Safety-certified UDT          */
    uint32_t            signature_hash;  /**< 32-bit structural hash        */
} plc_udt_t;

/* ============================================================================
 * L3: Tag Database ? Internal representation of Logix memory
 * ============================================================================
 * The tag database manages all tags in a controller. In a real Logix 5000,
 * this is a hierarchical symbol table. The C model uses a flat array for
 * simplicity with scope-based filtering.
 *
 * Memory model:
 *   - Tags stored in a contiguous virtual memory space
 *   - Each tag occupies memory_offset .. memory_offset + memory_size
 *   - Alignment: 4 bytes for SINT, 4 for BOOL, 8 for LINT/LREAL (64-bit ctrl)
 *   - The tag database handles allocation, deallocation, and lookup
 */

#define TAG_DB_MAX_TAGS     4096
#define TAG_DB_MAX_UDTS     256

/** Tag database structure */
typedef struct {
    uint16_t        tag_count;
    plc_tag_t       tags[TAG_DB_MAX_TAGS];
    uint16_t        udt_count;
    plc_udt_t       udts[TAG_DB_MAX_UDTS];
    uint32_t        memory_pool_size;    /**< Total allocated tag memory     */
    uint32_t        memory_used;         /**< Currently consumed memory      */
    bool            is_safety_locked;    /**< Safety signature lock active   */
    uint32_t        safety_task_slot;    /**< Safety task memory partition   */
} plc_tag_database_t;

/* ============================================================================
 * L4: Engineering Laws ? IEC 61131-3 Data Type Correspondence
 * ============================================================================
 * Rockwell data types map to IEC 61131-3 elementary types with known
 * deviations. This structure documents the mapping for cross-platform
 * engineering.
 *
 * Reference: IEC 61131-3 Table 12 (Generic data types)
 */

/** IEC 61131-3 to Rockwell type mapping */
typedef struct {
    const char* iec_type_name;   /**< e.g., "ANY_INT", "ANY_REAL"           */
    const char* rockwell_type;   /**< e.g., "DINT", "REAL"                  */
    uint16_t    rockwell_code;   /**< Numeric type code in Logix             */
    uint8_t     bit_size;        /**< Effective bit width (SINT=8 in memory 32) */
    bool        exact_match;     /**< Semantic equivalence: true=identical   */
    const char* deviation_note;  /**< Description of any deviations          */
} iec_rockwell_type_map_t;

/* ============================================================================
 * L5: Algorithms ? Tag Search and Resolution
 * ============================================================================ */

/**
 * Tag Resolution Algorithm:
 *
 * In Logix 5000, tag references use dotted notation: "ProgramName.TagName"
 * or "TagName.SubMember" or "ArrayTag[5]". The resolution algorithm follows
 * the Controller -> Program -> Local search order.
 *
 * Resolution steps:
 *   1. Check for explicit scope prefix (e.g., "MyProgram.MyTag")
 *   2. If no prefix, search the current program scope
 *   3. If not found, search controller scope
 *   4. Resolve member access (dot notation for UDT members)
 *   5. Resolve array indexing (square bracket notation)
 *
 * This algorithm is O(n) in worst case (n = total tags) but uses scope
 * filtering to reduce search space.
 *
 * Ref: Logix 5000 Tag Addressing (Rockwell 1756-PM004 ?2)
 */

/** Tag search result ? used by tag resolution algorithm */
typedef struct {
    bool            found;
    uint16_t        tag_index;       /**< Index into tags[] or -1 if not found */
    uint16_t        udt_member_index;/**< If resolving member, index into UDT   */
    bool            is_program_scoped;/**< Found in program scope               */
    const char*     unresolved_suffix;/**< Portion of name not yet resolved      */
} plc_tag_search_result_t;

/* ============================================================================
 * L6: Canonical Problem ? Tag Management Operations
 * ============================================================================ */

/**
 * @brief Initialize an empty tag database with default settings.
 * @param db Pointer to uninitialized tag database
 * @param memory_pool_size Total bytes allocated for tag storage
 *
 * Initializes the tag database with zero tags and zero UDTs. The memory pool
 * is a contiguous allocation that tags consume from during creation.
 *
 * This models the Logix 5000 memory layout during controller initialization
 * (power-on or program download). In reality, the firmware allocates the
 * tag table from the controller's RAM pool (typically 2-32 MB depending on
 * the controller model).
 *
 * Ref: Logix 5000 Controllers Common Procedures (1756-PM001)
 */
void plc_tag_database_init(plc_tag_database_t* db, uint32_t memory_pool_size);

/**
 * @brief Create (define) a new tag in the tag database.
 * @param db Tag database
 * @param name Tag name (must be unique within scope)
 * @param scope Controller or Program scope
 * @param type_class Atomic, predefined, or UDT reference
 * @param type_code Numeric type code
 * @param owner Program name for program-scoped tags (NULL for controller)
 * @return Pointer to created tag, or NULL on failure (dup name, db full).
 *
 * This function models the "New Tag" dialog in Studio 5000. It validates
 * uniqueness, allocates memory from the pool, and initializes the tag
 * definition structure.
 *
 * Computational complexity: O(n) where n = number of tags (uniqueness check).
 */
plc_tag_t* plc_tag_database_create_tag(plc_tag_database_t* db,
                                        const char* name,
                                        plc_tag_scope_t scope,
                                        plc_type_class_t type_class,
                                        uint16_t type_code,
                                        const char* owner);

/**
 * @brief Delete a tag from the database (by name).
 * @param db Tag database
 * @param name Tag name to delete
 * @return true if tag was deleted, false if not found.
 *
 * Deleting a tag frees its allocated memory back to the pool. Note: in
 * Logix 5000, deleting a tag while it is used in logic generates a
 * verification error; this library does not enforce that constraint.
 *
 * Complexity: O(n) where n = tag count.
 */
bool plc_tag_database_delete_tag(plc_tag_database_t* db, const char* name);

/**
 * @brief Resolve a tag name to its database index.
 * @param db Tag database
 * @param name Fully-qualified or relative tag name
 * @param current_program Name of current program (for scope resolution)
 * @param result Pointer to receive search results
 *
 * Implements the full Logix 5000 tag resolution algorithm including
 * scope hierarchy, UDT member traversal, and array indexing.
 *
 * Ref: Logix 5000 Tag Resolution Algorithm (internal RA doc).
 */
void plc_tag_resolve(const plc_tag_database_t* db,
                     const char* name,
                     const char* current_program,
                     plc_tag_search_result_t* result);

/**
 * @brief Create a new User-Defined Type.
 * @param db Tag database
 * @param name UDT name
 * @param description UDT description
 * @return Pointer to created UDT, or NULL on failure.
 */
plc_udt_t* plc_udt_create(plc_tag_database_t* db, const char* name,
                           const char* description);

/**
 * @brief Add a member to a UDT definition.
 * @param udt UDT to modify
 * @param name Member name
 * @param type_class Data type class
 * @param type_code Data type code
 * @return true on success, false if UDT is full or name duplicate.
 *
 * Each member is assigned a byte offset based on 32-bit alignment rules.
 * Adding a member recomputes total_byte_size.
 */
bool plc_udt_add_member(plc_udt_t* udt, const char* name,
                         plc_type_class_t type_class, uint16_t type_code);

/**
 * @brief Find a UDT by name in the database.
 * @return Pointer to UDT, or NULL if not found.
 */
plc_udt_t* plc_udt_find(plc_tag_database_t* db, const char* name);

/* ============================================================================
 * L5: Advanced ? Tag Memory Layout Utilities
 * ============================================================================ */

/**
 * @brief Calculate the memory size of a Rockwell atomic data type.
 * @param type_code Numeric type code (AOI_TYPE_* compatible)
 * @return Size in bytes (32-bit aligned: BOOL=4, SINT=4, DINT=4, LREAL=8).
 *
 * This function captures the fundamental Rockwell memory model rule:
 * everything is DINT-aligned. Even a BOOL costs 4 bytes. This is a key
 * difference from Siemens (DB-based) and IEC 61131-3 POU which use
 * packed storage.
 *
 * Theorem: For any atomic type t, sizeof_logix(t) = max(4, natural_size(t))
 * except LREAL=8 (64-bit controllers) and LINT=8.
 */
uint32_t plc_type_atomic_size(uint16_t type_code);

/**
 * @brief Calculate total byte size of a UDT including alignment padding.
 * @param udt UDT definition
 * @return Total size in bytes (ceil to 4-byte boundary).
 *
 * Alignment rules:
 *   - BOOL members: 4-byte aligned
 *   - SINT members: 4-byte aligned
 *   - DINT members: 4-byte aligned (native)
 *   - LINT members: 8-byte aligned
 *   - LREAL members: 8-byte aligned
 *   - Nested UDT: starts on largest member alignment (4 or 8)
 *
 * Complexity: O(n) where n = member count.
 */
uint32_t plc_udt_compute_byte_size(const plc_udt_t* udt);

/**
 * @brief Serialize a tag database to L5K text format.
 * @param db Tag database
 * @param buffer Output buffer (L5K text)
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or 0 if buffer too small.
 *
 * L5K is the text-based Logix 5000 export format. This function generates
 * the TAG section of an L5K file, which is the primary interchange format
 * between Studio 5000, version control systems, and automated tools.
 *
 * The L5K format is a domain-specific language with KEYWORD/VALUE pairs.
 * It is structurally similar to XML but uses Rockwell's proprietary syntax.
 *
 * Ref: Logix 5000 Import/Export (L5K) File Format specification.
 */
uint32_t plc_tag_database_export_l5k(const plc_tag_database_t* db,
                                      char* buffer, uint32_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* PLC_ROCKWELL_TAG_H */
