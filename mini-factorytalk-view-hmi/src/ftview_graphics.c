/**
 * @file ftview_graphics.c
 * @brief HMI Graphics Engine — display objects, animation, navigation
 *
 * Implements:
 *   L1.12 — Display object types and management
 *   L1.13 — Screen definition and lifecycle
 *   L2.8  — Property binding (expression -> tag mapping)
 *   L2.9  — Animation evaluation (visibility, colour, position, fill, rotation)
 *   L3.7  — Z-order sorting for correct 2D rendering
 *   L3.8  — Screen navigation graph
 *   L5.5  — AABB hit-test for touch/click detection
 *   L5.6  — Bilinear colour interpolation for gradient fills
 */

#include "ftview_graphics.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* =====================================================================
 * Display Manager Initialisation
 * ===================================================================== */

void ftview_display_mgr_init(ftview_display_mgr_t *mgr)
{
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
}

ftview_err_t ftview_display_add_screen(ftview_display_mgr_t *mgr,
                                        const ftview_display_screen_t *screen)
{
    if (!mgr || !screen) return FTVIEW_ERR_NULL_PTR;
    if (mgr->screen_count >= FTVIEW_DISPLAY_MAX_SCREENS) return FTVIEW_ERR_OUT_OF_MEMORY;

    /* Check for duplicate name */
    for (uint32_t i = 0; i < mgr->screen_count; i++) {
        if (strcmp(mgr->screens[i].name, screen->name) == 0) {
            return FTVIEW_ERR_DUPLICATE_TAG;
        }
    }

    memcpy(&mgr->screens[mgr->screen_count], screen, sizeof(ftview_display_screen_t));
    mgr->screens[mgr->screen_count].id = mgr->screen_count + 1;
    mgr->screen_count++;

    /* Also register in navigation graph */
    if (mgr->nav_graph.node_count < FTVIEW_DISPLAY_MAX_SCREENS) {
        ftview_nav_node_t *node = &mgr->nav_graph.nodes[mgr->nav_graph.node_count++];
        memset(node, 0, sizeof(*node));
        node->screen_id = screen->id;
        memcpy(node->screen_name, screen->name, FTVIEW_DISPLAY_NAME_LEN - 1);
        node->screen_name[FTVIEW_DISPLAY_NAME_LEN - 1] = '\0';
        node->isa_level = (uint32_t)screen->isa_level;
    }

    return FTVIEW_OK;
}

uint32_t ftview_display_add_object(ftview_display_mgr_t *mgr,
                                    uint32_t screen_id,
                                    const ftview_display_object_t *obj)
{
    if (!mgr || !obj || screen_id == 0 || screen_id > mgr->screen_count) return 0;

    ftview_display_screen_t *screen = &mgr->screens[screen_id - 1];
    if (screen->object_count >= FTVIEW_DISPLAY_MAX_OBJECTS) return 0;

    uint32_t idx = screen->object_count;
    memcpy(&screen->objects[idx], obj, sizeof(ftview_display_object_t));
    screen->objects[idx].id = screen->object_count + 1;
    screen->object_count++;

    return screen->objects[idx].id;
}

ftview_err_t ftview_display_bind_property(ftview_display_mgr_t *mgr,
                                           uint32_t screen_id,
                                           uint32_t object_id,
                                           const ftview_property_binding_t *binding)
{
    if (!mgr || !binding || screen_id == 0 || screen_id > mgr->screen_count)
        return FTVIEW_ERR_INVALID_PARAM;

    ftview_display_screen_t *screen = &mgr->screens[screen_id - 1];

    for (uint32_t i = 0; i < screen->object_count; i++) {
        if (screen->objects[i].id == object_id) {
            ftview_display_object_t *obj = &screen->objects[i];
            if (obj->binding_count >= 8) return FTVIEW_ERR_BUFFER_FULL;
            memcpy(&obj->bindings[obj->binding_count], binding,
                   sizeof(ftview_property_binding_t));
            obj->binding_count++;
            return FTVIEW_OK;
        }
    }
    return FTVIEW_ERR_TAG_NOT_FOUND;
}

ftview_err_t ftview_display_add_animation(ftview_display_mgr_t *mgr,
                                           uint32_t screen_id,
                                           uint32_t object_id,
                                           const ftview_animation_t *anim)
{
    if (!mgr || !anim || screen_id == 0 || screen_id > mgr->screen_count)
        return FTVIEW_ERR_INVALID_PARAM;

    ftview_display_screen_t *screen = &mgr->screens[screen_id - 1];

    if (screen->animation_count >= FTVIEW_DISPLAY_MAX_OBJECTS)
        return FTVIEW_ERR_BUFFER_FULL;

    memcpy(&screen->animations[screen->animation_count], anim,
           sizeof(ftview_animation_t));
    screen->animation_count++;
    return FTVIEW_OK;
}

/* =====================================================================
 * L3.7 — Z-order Sorting
 *
 * Sorts display objects by z_order ascending (stable sort via insertion).
 * Objects with lower z_order are rendered first (beneath).
 *
 * Complexity: O(n^2) insertion sort; acceptable for n < 512.
 * For larger displays, use qsort or merge sort.
 * ===================================================================== */

void ftview_display_sort_zorder(ftview_display_mgr_t *mgr, uint32_t screen_id)
{
    if (!mgr || screen_id == 0 || screen_id > mgr->screen_count) return;

    ftview_display_screen_t *screen = &mgr->screens[screen_id - 1];
    uint32_t n = screen->object_count;
    if (n < 2) return;

    /* Insertion sort on z_order */
    for (uint32_t i = 1; i < n; i++) {
        ftview_display_object_t key = screen->objects[i];
        int32_t j = (int32_t)i - 1;

        while (j >= 0 && screen->objects[j].z_order > key.z_order) {
            screen->objects[j + 1] = screen->objects[j];
            j--;
        }
        screen->objects[j + 1] = key;
    }
}

/* =====================================================================
 * L3.8 — Navigation
 * ===================================================================== */

ftview_err_t ftview_display_add_nav_link(ftview_display_mgr_t *mgr,
                                          uint32_t from_screen_id,
                                          uint32_t to_screen_id)
{
    if (!mgr || from_screen_id == 0 || to_screen_id == 0) return FTVIEW_ERR_INVALID_PARAM;
    if (from_screen_id > mgr->screen_count || to_screen_id > mgr->screen_count)
        return FTVIEW_ERR_TAG_NOT_FOUND;

    ftview_display_screen_t *from = &mgr->screens[from_screen_id - 1];
    if (from->nav_count >= FTVIEW_NAV_LINK_MAX) return FTVIEW_ERR_BUFFER_FULL;

    /* Check for duplicate link */
    for (uint32_t i = 0; i < from->nav_count; i++) {
        if (from->nav_targets[i] == to_screen_id) return FTVIEW_OK;
    }

    from->nav_targets[from->nav_count++] = to_screen_id;

    /* Also update navigation graph node */
    for (uint32_t i = 0; i < mgr->nav_graph.node_count; i++) {
        if (mgr->nav_graph.nodes[i].screen_id == from_screen_id) {
            ftview_nav_node_t *node = &mgr->nav_graph.nodes[i];
            if (node->target_count < FTVIEW_NAV_LINK_MAX) {
                node->targets[node->target_count++] = to_screen_id;
            }
            break;
        }
    }

    return FTVIEW_OK;
}

ftview_err_t ftview_display_navigate(ftview_display_mgr_t *mgr,
                                      uint32_t screen_id)
{
    if (!mgr || screen_id == 0 || screen_id > mgr->screen_count)
        return FTVIEW_ERR_INVALID_PARAM;

    mgr->previous_screen_id = mgr->active_screen_id;
    mgr->active_screen_id = screen_id;

    /* Update nav graph */
    for (uint32_t i = 0; i < mgr->nav_graph.node_count; i++) {
        if (mgr->nav_graph.nodes[i].screen_id == screen_id) {
            mgr->nav_graph.current_node_idx = i;
            break;
        }
    }

    return FTVIEW_OK;
}

ftview_err_t ftview_display_navigate_back(ftview_display_mgr_t *mgr)
{
    if (!mgr) return FTVIEW_ERR_NULL_PTR;
    if (mgr->previous_screen_id == 0) return FTVIEW_ERR_TAG_NOT_FOUND;

    uint32_t temp = mgr->active_screen_id;
    mgr->active_screen_id = mgr->previous_screen_id;
    mgr->previous_screen_id = temp;

    return FTVIEW_OK;
}

/* =====================================================================
 * L5.5 — AABB Hit-Test
 *
 * Determines if a point (px, py) falls within the axis-aligned bounding
 * box of any display object. Returns the first matching object ID
 * (highest z-order frontmost), or 0 if no hit.
 *
 * Algorithm: point-in-rectangle test for each object, sorted by
 * descending z_order to return the frontmost object first.
 *
 * Complexity: O(n) linear scan of all objects.
 * Reference: Ericson, "Real-Time Collision Detection" (2005), §4.2.2.
 * ===================================================================== */

uint32_t ftview_display_hit_test(const ftview_display_screen_t *screen,
                                  double px, double py)
{
    if (!screen) return 0;

    uint32_t best_id = 0;
    int32_t best_z = -2147483647;

    for (uint32_t i = 0; i < screen->object_count; i++) {
        const ftview_display_object_t *obj = &screen->objects[i];
        if (!obj->visible || !obj->enabled) continue;

        /* AABB test */
        if (px >= obj->bounds.x &&
            px <= obj->bounds.x + obj->bounds.w &&
            py >= obj->bounds.y &&
            py <= obj->bounds.y + obj->bounds.h) {

            /* Take the highest z-order among hits */
            if (obj->z_order > best_z) {
                best_z = obj->z_order;
                best_id = obj->id;
            }
        }
    }
    return best_id;
}

/* =====================================================================
 * L5.6 — Bilinear Colour Interpolation
 *
 * Interpolates a colour within a quadrilateral defined by four corner
 * colours. Uses bilinear interpolation:
 *
 *   C(u,v) = (1-u)(1-v)*C_TL + u*(1-v)*C_TR + (1-u)*v*C_BL + u*v*C_BR
 *
 * where u, v are normalized coordinates [0,1] within the cell.
 *
 * Application: smooth gradient fills on HMI bars, gauges, tanks.
 *
 * Complexity: O(1).
 * Reference: Foley, van Dam, Feiner, Hughes. "Computer Graphics:
 *   Principles and Practice" (1990), §17.3.3.
 * ===================================================================== */

ftview_color_t ftview_color_bilinear(const ftview_color_t *ccTL,
                                      const ftview_color_t *ccTR,
                                      const ftview_color_t *ccBL,
                                      const ftview_color_t *ccBR,
                                      double u, double v)
{
    ftview_color_t result;

    /* Clamp u, v to [0,1] */
    if (u < 0.0) u = 0.0;
    if (u > 1.0) u = 1.0;
    if (v < 0.0) v = 0.0;
    if (v > 1.0) v = 1.0;

    double u1 = 1.0 - u;
    double v1 = 1.0 - v;

    double r = u1 * v1 * ccTL->r + u * v1 * ccTR->r +
               u1 * v  * ccBL->r + u * v  * ccBR->r;
    double g = u1 * v1 * ccTL->g + u * v1 * ccTR->g +
               u1 * v  * ccBL->g + u * v  * ccBR->g;
    double b = u1 * v1 * ccTL->b + u * v1 * ccTR->b +
               u1 * v  * ccBL->b + u * v  * ccBR->b;

    result.r = (uint8_t)(r + 0.5);
    result.g = (uint8_t)(g + 0.5);
    result.b = (uint8_t)(b + 0.5);

    return result;
}

/* =====================================================================
 * L2.9 — Animation Evaluation
 *
 * Evaluates an animation based on a tag value and computes the rendered
 * visual property (colour, visibility, position offset, etc.).
 *
 * Animation types:
 *   FTVIEW_ANIM_VISIBILITY  — value >= threshold -> visible
 *   FTVIEW_ANIM_FILL_COLOR  — map value to colour band
 *   FTVIEW_ANIM_FILL_PCT    — map value to [0,1] fill percentage
 *   FTVIEW_ANIM_ROTATION    — map value to rotation angle
 *   FTVIEW_ANIM_HORIZ_POS   — map value to horizontal offset
 *   FTVIEW_ANIM_VERT_POS    — map value to vertical offset
 *
 * Linear mapping: output = (value - range_min) / (range_max - range_min)
 *                          * (output_max - output_min) + output_min
 *
 * Complexity: O(t) where t = threshold_count (≤ 8).
 * ===================================================================== */

double ftview_animation_evaluate(const ftview_animation_t *anim,
                                  double tag_value,
                                  ftview_color_t *color_out,
                                  bool *visible_out)
{
    if (!anim) return 0.0;

    /* Handle threshold-based animations (colour bands, visibility) */
    if (anim->threshold_count > 0 && color_out) {
        for (uint32_t i = 0; i < anim->threshold_count; i++) {
            const ftview_animation_threshold_t *th = &anim->thresholds[i];
            if (tag_value >= th->min_value && tag_value < th->max_value) {
                *color_out = th->color;
                if (visible_out) *visible_out = th->visible;
                break;
            }
        }
    }

    /* Handle linear-mapped animations */
    double range = anim->range_max - anim->range_min;
    double out_range = anim->output_max - anim->output_min;

    /* Guard zero span */
    if (fabs(range) < 1.0e-12) return anim->output_min;

    double fraction = (tag_value - anim->range_min) / range;

    /* Clamp */
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;

    double output = fraction * out_range + anim->output_min;

    /* Specific animation type post-processing */
    switch (anim->type) {
    case FTVIEW_ANIM_FILL_PCT:
        /* output is already [0,1] or user-specified range */
        if (visible_out && output < 0.01) *visible_out = false;
        break;

    case FTVIEW_ANIM_VISIBILITY:
        if (visible_out) *visible_out = (tag_value >= anim->range_min);
        break;

    case FTVIEW_ANIM_ROTATION:
        /* output in degrees, wrap to [0,360) */
        output = fmod(output, 360.0);
        if (output < 0) output += 360.0;
        break;

    default:
        break;
    }

    return output;
}

/* Find display screen by name */
ftview_display_screen_t *ftview_display_find_screen(ftview_display_mgr_t *mgr,
                                                     const char *name)
{
    if (!mgr || !name) return NULL;

    for (uint32_t i = 0; i < mgr->screen_count; i++) {
        if (strcmp(mgr->screens[i].name, name) == 0) {
            return &mgr->screens[i];
        }
    }
    return NULL;
}
