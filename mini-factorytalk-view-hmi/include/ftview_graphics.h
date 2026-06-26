/**
 * @file ftview_graphics.h
 * @brief FactoryTalk View HMI -- Graphics & Display Management (L1/L2/L3/L5)
 *
 * Knowledge points:
 *   L1.12 -- HMI display object types: numeric, bar, gauge, button, symbol, text
 *   L1.13 -- Display screen definition: objects, navigation, security binding
 *   L2.8  -- Object-property binding: property -> tag expression
 *   L2.9  -- Animation types: visibility, colour, position, rotation, fill
 *   L3.7  -- Z-ordered display object tree with spatial indexing
 *   L3.8  -- Screen navigation graph (Display A -> Display B transitions)
 *   L5.5  -- AABB (axis-aligned bounding box) hit-test for touch regions
 *   L5.6  -- Bilinear interpolation for gradient fills
 *   L7.1  -- PlantPAx HMI object template pattern
 *
 * Reference: Rockwell Automation View Site Edition Graphics Guide
 *            ISA-101.01-2015, Annex B: Display Element Design Guidance
 */

#ifndef FTVIEW_GRAPHICS_H
#define FTVIEW_GRAPHICS_H

#include "ftview_common.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FTVIEW_DISPLAY_MAX_OBJECTS    32
#define FTVIEW_DISPLAY_MAX_SCREENS    16
#define FTVIEW_DISPLAY_NAME_LEN       64
#define FTVIEW_PROP_EXPR_MAX_LEN     256
#define FTVIEW_NAV_LINK_MAX           32

/* =====================================================================
 * L1.12 -- HMI Display Object Types
 * ===================================================================== */

typedef enum {
    FTVIEW_OBJ_RECTANGLE      = 0,
    FTVIEW_OBJ_ELLIPSE        = 1,
    FTVIEW_OBJ_POLYGON        = 2,
    FTVIEW_OBJ_LINE           = 3,
    FTVIEW_OBJ_TEXT           = 4,
    FTVIEW_OBJ_NUMERIC_DISP   = 5,  /**< numeric display (PV, SP, etc.) */
    FTVIEW_OBJ_BUTTON         = 6,
    FTVIEW_OBJ_BAR_GRAPH      = 7,  /**< horizontal/vertical bar */
    FTVIEW_OBJ_GAUGE          = 8,  /**< circular/arc gauge */
    FTVIEW_OBJ_SYMBOL         = 9,  /**< embedded SVG-like symbol */
    FTVIEW_OBJ_TREND          = 10, /**< embedded trend */
    FTVIEW_OBJ_ALARM_LIST     = 11, /**< embedded alarm table */
    FTVIEW_OBJ_GROUP          = 12  /**< composite / group object */
} ftview_object_type_t;

/* =====================================================================
 * L2.9 -- Animation Types
 * ===================================================================== */

typedef enum {
    FTVIEW_ANIM_NONE          = 0,
    FTVIEW_ANIM_VISIBILITY    = 1,   /**< show/hide based on expression */
    FTVIEW_ANIM_FILL_COLOR    = 2,   /**< colour by value threshold */
    FTVIEW_ANIM_STROKE_COLOR  = 3,   /**< outline colour animation */
    FTVIEW_ANIM_TEXT_COLOR    = 4,   /**< text foreground colour */
    FTVIEW_ANIM_HORIZ_POS     = 5,   /**< horizontal position shift */
    FTVIEW_ANIM_VERT_POS      = 6,   /**< vertical position shift */
    FTVIEW_ANIM_ROTATION      = 7,   /**< angle rotation (degrees) */
    FTVIEW_ANIM_WIDTH         = 8,   /**< width scaling */
    FTVIEW_ANIM_HEIGHT        = 9,   /**< height scaling */
    FTVIEW_ANIM_FILL_PCT      = 10,  /**< percentage fill (bar graphs) */
    FTVIEW_ANIM_TOOLTIP       = 11,  /**< dynamic tooltip text */
    FTVIEW_ANIM_TOUCH         = 12   /**< touch/press animation */
} ftview_animation_type_t;

/* =====================================================================
 * L2.8 -- Property Binding (expression to tag mapping)
 *
 * Binds an object property to a value expression.
 * Expression syntax: ={[PLC1]N7:0}*0.1+32  (evaluated at runtime)
 * Simple form: =TAG_NAME  (direct tag reference)
 * ===================================================================== */

typedef struct {
    char                  property_name[32];  /**< e.g. "Caption", "Value", "FillColor" */
    char                  expression[FTVIEW_PROP_EXPR_MAX_LEN];
    bool                  is_const;          /**< constant value, no tag binding */
    double                const_value;        /**< effective const if is_const */
} ftview_property_binding_t;

/* =====================================================================
 * Display Object Instance
 * ===================================================================== */

/** 2D coordinate (floating-point for smooth scaling) */
typedef struct { double x, y; } ftview_point2d_t;

/** Axis-aligned bounding box */
typedef struct {
    double x, y, w, h;
} ftview_rect_t;

/** L1.12 -- Display object instance */
typedef struct {
    uint32_t                id;
    ftview_object_type_t    type;
    char                    name[32];
    ftview_rect_t           bounds;          /**< position and size */
    int32_t                 z_order;         /**< rendering depth (higher = on top) */
    double                  rotation_deg;    /**< rotation around centre */
    double                  opacity;         /**< 0.0 (transparent) .. 1.0 (opaque) */
    bool                    visible;
    bool                    enabled;         /**< interactive (touch/click) */
    uint32_t                security_mask;   /**< required privileges to interact */
    /* Styling */
    ftview_color_t          fill_color;
    ftview_color_t          stroke_color;
    double                  stroke_width;
    char                    caption[128];
    /* Bindings */
    ftview_property_binding_t bindings[8];
    uint32_t                binding_count;
    /* Children (for GROUP type) */
    uint32_t                first_child_id;
    uint32_t                child_count;
    /* L7.1 -- PlantPAx template reference */
    char                    template_name[64];
} ftview_display_object_t;

/* =====================================================================
 * L2.9 -- Animation Threshold Definition
 *
 * Each animation has a set of thresholds mapping value ranges
 * to visual states (colours, positions, visibility, etc.).
 * ===================================================================== */

typedef struct {
    double                  min_value;       /**< inclusive lower bound */
    double                  max_value;       /**< exclusive upper bound */
    ftview_color_t          color;            /**< colour for this band */
    bool                    visible;
} ftview_animation_threshold_t;

#define FTVIEW_ANIM_MAX_THRESHOLDS 8

/** Animation instance bound to a display object */
typedef struct {
    ftview_animation_type_t type;
    char                    tag_expression[FTVIEW_PROP_EXPR_MAX_LEN];
    ftview_animation_threshold_t thresholds[FTVIEW_ANIM_MAX_THRESHOLDS];
    uint32_t                threshold_count;
    double                  range_min;       /**< animation mapping domain */
    double                  range_max;
    double                  output_min;      /**< animation mapping codomain */
    double                  output_max;
} ftview_animation_t;

/* =====================================================================
 * L1.13 -- Display Screen Definition
 * ===================================================================== */

typedef struct {
    uint32_t                id;
    char                    name[FTVIEW_DISPLAY_NAME_LEN];
    isa101_level_t          isa_level;
    isa101_display_t        archetype;
    ftview_resolution_t     resolution;
    uint16_t                width;
    uint16_t                height;
    ftview_color_t          background_color;
    ftview_display_object_t objects[FTVIEW_DISPLAY_MAX_OBJECTS];
    uint32_t                object_count;
    ftview_animation_t      animations[FTVIEW_DISPLAY_MAX_OBJECTS];
    uint32_t                animation_count;
    /* L3.8 -- Navigation */
    uint32_t                nav_targets[FTVIEW_NAV_LINK_MAX];
    uint32_t                nav_count;
    uint32_t                security_mask;   /**< required privilege to open */
    char                    cache_key[64];   /**< display caching identifier */
} ftview_display_screen_t;

/* =====================================================================
 * L3.8 -- Screen Navigation Graph
 * ===================================================================== */

typedef struct {
    uint32_t                screen_id;
    char                    screen_name[FTVIEW_DISPLAY_NAME_LEN];
    uint32_t                targets[FTVIEW_NAV_LINK_MAX];
    uint32_t                target_count;
    uint32_t                isa_level;
    bool                    is_home;
} ftview_nav_node_t;

typedef struct {
    ftview_nav_node_t       nodes[FTVIEW_DISPLAY_MAX_SCREENS];
    uint32_t                node_count;
    uint32_t                home_node_idx;
    uint32_t                current_node_idx;
} ftview_nav_graph_t;

/* =====================================================================
 * Display Manager
 * ===================================================================== */

typedef struct {
    ftview_display_screen_t screens[FTVIEW_DISPLAY_MAX_SCREENS];
    uint32_t                screen_count;
    ftview_nav_graph_t      nav_graph;
    uint32_t                active_screen_id;
    uint32_t                previous_screen_id;
} ftview_display_mgr_t;

/* ───────── API ───────── */

void ftview_display_mgr_init(ftview_display_mgr_t *mgr);

/** Add a display screen. Returns FTVIEW_ERR_DUPLICATE_TAG if name conflict. */
ftview_err_t ftview_display_add_screen(ftview_display_mgr_t *mgr,
                                        const ftview_display_screen_t *screen);

/** L1.12 -- Add an object to a display. Returns object ID on success. */
uint32_t ftview_display_add_object(ftview_display_mgr_t *mgr,
                                    uint32_t screen_id,
                                    const ftview_display_object_t *obj);

/** L2.8 -- Bind a property to a tag expression */
ftview_err_t ftview_display_bind_property(ftview_display_mgr_t *mgr,
                                           uint32_t screen_id,
                                           uint32_t object_id,
                                           const ftview_property_binding_t *binding);

/** L2.9 -- Add animation to an object */
ftview_err_t ftview_display_add_animation(ftview_display_mgr_t *mgr,
                                           uint32_t screen_id,
                                           uint32_t object_id,
                                           const ftview_animation_t *anim);

/** L3.7 -- Sort objects by Z-order for correct rendering sequence */
void ftview_display_sort_zorder(ftview_display_mgr_t *mgr, uint32_t screen_id);

/** L3.8 -- Add a navigation link between screens */
ftview_err_t ftview_display_add_nav_link(ftview_display_mgr_t *mgr,
                                          uint32_t from_screen_id,
                                          uint32_t to_screen_id);

/** Navigate to a screen (updates nav state) */
ftview_err_t ftview_display_navigate(ftview_display_mgr_t *mgr,
                                      uint32_t screen_id);

/** Go back to previous display */
ftview_err_t ftview_display_navigate_back(ftview_display_mgr_t *mgr);

/** L5.5 -- AABB hit-test: check if point (px, py) is inside object bounds.
 *  Used for touch/click detection. Returns first matching object ID. */
uint32_t ftview_display_hit_test(const ftview_display_screen_t *screen,
                                  double px, double py);

/** L5.6 -- Bilinear interpolation for smooth gradient colour transitions
 *  Input: four corner colours (ccTL..ccBR), normalized coordinates (u,v) in [0,1]
 *  Output: interpolated colour at (u,v) */
ftview_color_t ftview_color_bilinear(const ftview_color_t *ccTL,
                                      const ftview_color_t *ccTR,
                                      const ftview_color_t *ccBL,
                                      const ftview_color_t *ccBR,
                                      double u, double v);

/** Evaluate an animation and return the effective visual property.
 *  e.g. given a tag value of 75 and fill_pct animation [0,100]->[0,1],
 *  computes the output value for rendering. */
double ftview_animation_evaluate(const ftview_animation_t *anim,
                                  double tag_value,
                                  ftview_color_t *color_out,
                                  bool *visible_out);

/** Get display screen by name */
ftview_display_screen_t *ftview_display_find_screen(ftview_display_mgr_t *mgr,
                                                     const char *name);

#endif /* FTVIEW_GRAPHICS_H */
