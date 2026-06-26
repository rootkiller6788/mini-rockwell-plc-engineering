/**
 * @file ftview_tag.h
 * @brief FactoryTalk View HMI -- Tag Management (L1/L2/L3/L5)
 *
 * Knowledge points:
 *   L1.6 -- HMI Tag definition (name, type, scaling, addressing)
 *   L1.7 -- Direct Reference Tag (DRT) format {[shortcut]address}
 *   L2.2 -- Tag resolution: HMI tags vs direct references
 *   L2.3 -- Tag scaling and engineering unit conversion (linear, sqrt)
 *   L3.1 -- Hash-table based tag database with collision chaining
 *   L3.2 -- Tag subscription / on-change update model
 *   L5.1 -- FNV-1a hash for tag name indexing (proven low-collision)
 *   L5.2 -- Linear interpolation scaling with clamp: y = m*x + b
 *
 * Reference: Rockwell Automation FactoryTalk View SE User Guide (FTALK-SE001)
 *            IEC 61131-3 Section 2.4: Directly Represented Variables
 */

#ifndef FTVIEW_TAG_H
#define FTVIEW_TAG_H

#include "ftview_common.h"
#include <stdint.h>
#include <stddef.h>

/* Maximum tag database capacity */
#define FTVIEW_TAG_MAX_COUNT       256
#define FTVIEW_TAG_NAME_MAX_LEN    128
#define FTVIEW_TAG_DESC_MAX_LEN    256
#define FTVIEW_TAG_ADDR_MAX_LEN    128
#define FTVIEW_TAG_HASH_BUCKETS    256

/* =====================================================================
 * L1.6 -- HMI Tag Definition
 *
 * An HMI Tag is a named data container with optional
 * I/O address binding and engineering-unit metadata.
 * It is the atomic unit of HMI data management.
 * ===================================================================== */

/** Tag addressing mode */
typedef enum {
    FTVIEW_TAG_MODE_MEMORY    = 0,  /**< HMI-internal memory tag */
    FTVIEW_TAG_MODE_DEVICE    = 1,  /**< Direct reference to PLC/device */
    FTVIEW_TAG_MODE_DERIVED   = 2   /**< Derived/expression tag */
} ftview_tag_mode_t;

/** Tag descriptor -- the core HMI data atom */
typedef struct {
    uint32_t         id;                          /**< unique database ID */
    char             name[FTVIEW_TAG_NAME_MAX_LEN]; /**< tag name (unique) */
    char             description[FTVIEW_TAG_DESC_MAX_LEN];
    char             address[FTVIEW_TAG_ADDR_MAX_LEN]; /**< DRT or internal */
    ftview_tag_mode_t mode;
    ftview_type_t     type;
    ftview_eu_t       eu;                         /**< engineering unit metadata */
    ftview_value_t    current_value;              /**< last known value */
    bool              subscribed;                 /**< actively polled */
    uint32_t          update_rate_ms;             /**< polling interval */
    int64_t           last_update_ms;             /**< timestamp of last update */
    uint32_t          hash_next;                  /**< chaining index in hash table */
} ftview_tag_t;

/* =====================================================================
 * Tag Database (hash-table with open chaining)
 * ===================================================================== */

typedef struct {
    ftview_tag_t tags[FTVIEW_TAG_MAX_COUNT];      /**< tag storage pool */
    uint32_t     bucket[FTVIEW_TAG_HASH_BUCKETS]; /**< hash bucket heads (0-based indices + 1; 0 = empty) */
    uint32_t     count;                           /**< number of tags allocated */
    uint32_t     next_free;                       /**< next free slot hint */
    bool         sorted;                          /**< dirty flag for sorted iteration */
} ftview_tagdb_t;

/* =====================================================================
 * L3.1 / L5.1 -- Hash-based Tag Database Operations
 *
 * Time complexity: O(1) average lookup via FNV-1a hash
 * ===================================================================== */

/** Initialise an empty tag database */
void ftview_tagdb_init(ftview_tagdb_t *db);

/** L5.1 -- FNV-1a 32-bit hash for tag names.
 *  Fowler-Noll-Vo variant 1a; low collision on ASCII keys. */
uint32_t ftview_tag_name_hash(const char *name);

/** Add a tag definition to the database.
 *  Returns FTVIEW_ERR_DUPLICATE_TAG if name collision.
 *  Complexity: O(1) average, O(n) worst-case chain. */
ftview_err_t ftview_tagdb_add(ftview_tagdb_t *db, const ftview_tag_t *tag);

/** Find a tag by name. Returns NULL if not found.
 *  Complexity: O(1) average. */
ftview_tag_t *ftview_tagdb_find(ftview_tagdb_t *db, const char *name);

/** Remove a tag by name. Returns FTVIEW_ERR_TAG_NOT_FOUND if absent. */
ftview_err_t ftview_tagdb_remove(ftview_tagdb_t *db, const char *name);

/** Get tag count */
uint32_t ftview_tagdb_count(const ftview_tagdb_t *db);

/* =====================================================================
 * L2.3 / L5.2 -- Tag Value Scaling
 *
 * Engineering unit conversion: y = ((raw - raw_lo)/(raw_hi - raw_lo)) * (eu_hi - eu_lo) + eu_lo
 *
 * Supports linear and square-root modes (flow measurement via differential pressure).
 * ===================================================================== */

typedef enum {
    FTVIEW_SCALE_LINEAR = 0,
    FTVIEW_SCALE_SQRT   = 1,  /**< sqrt extraction for DP flow */
} ftview_scale_mode_t;

/** Scale a raw device value to engineering units.
 *  raw_lo, raw_hi: raw A/D range (e.g. 4-20 mA -> 0-4095 or 4000-20000)
 *  eu_lo, eu_hi:   engineering unit range
 *  mode:           FTVIEW_SCALE_LINEAR or FTVIEW_SCALE_SQRT
 *  Returns: scaled value clamped to [eu_lo, eu_hi] */
double ftview_tag_scale_raw_to_eu(double raw, double raw_lo, double raw_hi,
                                  double eu_lo, double eu_hi,
                                  ftview_scale_mode_t mode);

/** Scale backwards: engineering units to raw device value */
double ftview_tag_scale_eu_to_raw(double eu, double raw_lo, double raw_hi,
                                  double eu_lo, double eu_hi,
                                  ftview_scale_mode_t mode);

/** Apply tag's own EU metadata to scale a raw value */
double ftview_tag_apply_scaling(const ftview_tag_t *tag, double raw_value);

/** L2.2 -- Parse a Direct Reference Tag address string
 *  Format: {[shortcut]address}  e.g. "{[PLC1]N7:0}"
 *  Returns: 0 on success; fills shortcut and addr buffers */
ftview_err_t ftview_tag_parse_drt(const char *drt_string,
                                   char *shortcut_out, size_t sc_len,
                                   char *addr_out, size_t ad_len);

/** Update a tag's current value and timestamp */
ftview_err_t ftview_tag_update_value(ftview_tag_t *tag,
                                      const ftview_value_t *new_value);

/** Check if a tag's data is stale (last update > threshold) */
bool ftview_tag_is_stale(const ftview_tag_t *tag, int64_t threshold_ms);

/** L2.2 -- Tag resolution: resolve an HMI tag name to its current value.
 *  If the tag is a DRT, the actual I/O resolution happens in the
 *  communication layer (see ftview_communication.h).
 *  Returns FTVIEW_ERR_TAG_NOT_FOUND if absent. */
ftview_err_t ftview_tagdb_resolve(const ftview_tagdb_t *db,
                                   const char *name,
                                   ftview_value_t *value_out);

#endif /* FTVIEW_TAG_H */
