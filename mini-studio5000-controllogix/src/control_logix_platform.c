/**
 * @file    control_logix_platform.c
 * @brief   Implementation of ControlLogix 1756 Platform Architecture
 *
 * Knowledge Layers: L1-L3, L7
 *
 * Reference: 1756-UM001, 1756-TD003, 1756-SG001
 */

#include <string.h>
#include "control_logix_platform.h"

/* ========================================================================
 * L1: Chassis Management
 * ======================================================================== */

bool logix_chassis_init(logix_chassis_t *chassis,
                         logix_chassis_size_t size,
                         logix_chassis_type_t type)
{
    if (!chassis) return false;
    if (size > CHASSIS_17_SLOT || size < CHASSIS_4_SLOT) return false;

    memset(chassis, 0, sizeof(*chassis));
    chassis->size = size;
    chassis->type = type;
    chassis->backplane_speed_mbps = 100.0;  /* Default L7x/L8x */

    /* Controller typically in slot 0, but configurable */
    chassis->controller_slot = 0;
    chassis->partner_slot = 0xFF;  /* No partner until configured */

    /* Default power supply: 1756-PA75 (high-capacity) */
    chassis->power_supply.model_number = 0x0075;
    chassis->power_supply.rated_5v_amps = 13.0;
    chassis->power_supply.rated_3v3_amps = 4.0;
    chassis->power_supply.rated_24v_amps = 2.8;
    chassis->power_supply.total_power_watts = 75.0;

    return true;
}

bool logix_chassis_verify_power_budget(const logix_chassis_t *chassis)
{
    if (!chassis) return false;

    double total_5v = 0.0, total_3v3 = 0.0, total_24v = 0.0;

    for (uint8_t i = 0; i < chassis->size; i++) {
        if (chassis->slots[i].is_occupied) {
            total_5v  += chassis->slots[i].current_draw_5v;
            total_3v3 += chassis->slots[i].current_draw_3v3;
            total_24v += chassis->slots[i].current_draw_24v;
        }
    }

    /* Power supply rating check */
    return (total_5v  <= chassis->power_supply.rated_5v_amps) &&
           (total_3v3 <= chassis->power_supply.rated_3v3_amps) &&
           (total_24v <= chassis->power_supply.rated_24v_amps);
}

bool logix_chassis_install_module(logix_chassis_t *chassis,
                                   uint8_t slot_number,
                                   uint32_t product_code,
                                   double draw_5v,
                                   double draw_3v3,
                                   double draw_24v)
{
    if (!chassis) return false;
    if (slot_number >= chassis->size) return false;
    if (chassis->slots[slot_number].is_occupied) return false;

    chassis->slots[slot_number].is_occupied = true;
    chassis->slots[slot_number].module_product_code = product_code;
    chassis->slots[slot_number].current_draw_5v = draw_5v;
    chassis->slots[slot_number].current_draw_3v3 = draw_3v3;
    chassis->slots[slot_number].current_draw_24v = draw_24v;

    return true;
}

bool logix_chassis_remove_module(logix_chassis_t *chassis, uint8_t slot_number)
{
    if (!chassis) return false;
    if (slot_number >= chassis->size) return false;

    memset(&chassis->slots[slot_number], 0, sizeof(logix_slot_t));
    return true;
}

uint8_t logix_chassis_get_controller_slot(const logix_chassis_t *chassis)
{
    if (!chassis) return 0;
    return chassis->controller_slot;
}

uint8_t logix_chassis_occupied_slot_count(const logix_chassis_t *chassis)
{
    if (!chassis) return 0;

    uint8_t count = 0;
    for (uint8_t i = 0; i < chassis->size; i++) {
        if (chassis->slots[i].is_occupied) count++;
    }
    return count;
}

bool logix_chassis_is_redundancy_capable(const logix_chassis_t *chassis)
{
    if (!chassis) return false;

    /* Redundancy requires:
     * - Chassis marked as redundancy-capable
     * - Controller in the slot
     */
    return chassis->redundancy_capable &&
           chassis->slots[chassis->controller_slot].is_occupied;
}

void logix_chassis_power_remaining(const logix_chassis_t *chassis,
                                    double *rem_5v,
                                    double *rem_3v3,
                                    double *rem_24v)
{
    if (!chassis || !rem_5v || !rem_3v3 || !rem_24v) return;

    double total_5v = 0.0, total_3v3 = 0.0, total_24v = 0.0;

    for (uint8_t i = 0; i < chassis->size; i++) {
        if (chassis->slots[i].is_occupied) {
            total_5v  += chassis->slots[i].current_draw_5v;
            total_3v3 += chassis->slots[i].current_draw_3v3;
            total_24v += chassis->slots[i].current_draw_24v;
        }
    }

    *rem_5v  = chassis->power_supply.rated_5v_amps  - total_5v;
    *rem_3v3 = chassis->power_supply.rated_3v3_amps - total_3v3;
    *rem_24v = chassis->power_supply.rated_24v_amps - total_24v;

    /* Clamp to zero — cannot have negative remaining */
    if (*rem_5v  < 0.0) *rem_5v  = 0.0;
    if (*rem_3v3 < 0.0) *rem_3v3 = 0.0;
    if (*rem_24v < 0.0) *rem_24v = 0.0;
}

const char *logix_controller_type_name(logix_controller_type_t type)
{
    switch (type) {
        case CTLR_1756_L61:  return "1756-L61 ControlLogix 5561";
        case CTLR_1756_L62:  return "1756-L62 ControlLogix 5562";
        case CTLR_1756_L63:  return "1756-L63 ControlLogix 5563";
        case CTLR_1756_L64:  return "1756-L64 ControlLogix 5564";
        case CTLR_1756_L71:  return "1756-L71 ControlLogix 5571";
        case CTLR_1756_L72:  return "1756-L72 ControlLogix 5572";
        case CTLR_1756_L73:  return "1756-L73 ControlLogix 5573";
        case CTLR_1756_L74:  return "1756-L74 ControlLogix 5574";
        case CTLR_1756_L75:  return "1756-L75 ControlLogix 5575 (SIL2)";
        case CTLR_1756_L81E: return "1756-L81E ControlLogix 5580";
        case CTLR_1756_L82E: return "1756-L82E ControlLogix 5580 DLR";
        case CTLR_1756_L83E: return "1756-L83E ControlLogix 5580";
        case CTLR_1756_L84E: return "1756-L84E ControlLogix 5580 (SIL2)";
        case CTLR_1756_L85E: return "1756-L85E ControlLogix 5580 (SIL3)";
        case CTLR_1769_L16ER:  return "1769-L16ER-BB1B CompactLogix 5370 L1";
        case CTLR_1769_L18ER:  return "1769-L18ER-BB1B CompactLogix 5370 L1";
        case CTLR_1769_L24ER:  return "1769-L24ER-QB1B CompactLogix 5370 L2";
        case CTLR_1769_L27ERM: return "1769-L27ERM-QBFC1B CompactLogix 5370 L2";
        case CTLR_1769_L30ER:  return "1769-L30ER CompactLogix 5370 L3";
        case CTLR_1769_L33ER:  return "1769-L33ER CompactLogix 5370 L3";
        case CTLR_1769_L36ERM: return "1769-L36ERM CompactLogix 5370 L3";
        case CTLR_5069_L306ER: return "5069-L306ER CompactLogix 5380";
        case CTLR_5069_L310ER: return "5069-L310ER CompactLogix 5380";
        case CTLR_5069_L320ER: return "5069-L320ER CompactLogix 5380";
        case CTLR_5069_L330ER: return "5069-L330ER CompactLogix 5380";
        case CTLR_5069_L340ER: return "5069-L340ER CompactLogix 5380";
        default: return "Unknown Controller";
    }
}

void logix_controller_firmware_range(logix_controller_type_t type,
                                      uint32_t *min_major, uint32_t *min_minor,
                                      uint32_t *max_major, uint32_t *max_minor)
{
    if (!min_major || !min_minor || !max_major || !max_minor) return;

    /* Firmware compatibility ranges based on Rockwell PCDC */
    switch (type) {
        case CTLR_1756_L61:
        case CTLR_1756_L62:
        case CTLR_1756_L63:
        case CTLR_1756_L64:
            *min_major = 10; *min_minor = 0;
            *max_major = 20; *max_minor = 19;
            break;
        case CTLR_1756_L71:
        case CTLR_1756_L72:
        case CTLR_1756_L73:
        case CTLR_1756_L74:
        case CTLR_1756_L75:
            *min_major = 20; *min_minor = 0;
            *max_major = 32; *max_minor = 4;
            break;
        case CTLR_1756_L81E:
        case CTLR_1756_L82E:
        case CTLR_1756_L83E:
        case CTLR_1756_L84E:
        case CTLR_1756_L85E:
            *min_major = 28; *min_minor = 0;
            *max_major = 36; *max_minor = 0;
            break;
        case CTLR_1769_L16ER:
        case CTLR_1769_L18ER:
        case CTLR_1769_L24ER:
        case CTLR_1769_L27ERM:
        case CTLR_1769_L30ER:
        case CTLR_1769_L33ER:
        case CTLR_1769_L36ERM:
            *min_major = 20; *min_minor = 0;
            *max_major = 34; *max_minor = 0;
            break;
        case CTLR_5069_L306ER:
        case CTLR_5069_L310ER:
        case CTLR_5069_L320ER:
        case CTLR_5069_L330ER:
        case CTLR_5069_L340ER:
            *min_major = 30; *min_minor = 0;
            *max_major = 36; *max_minor = 0;
            break;
        default:
            *min_major = 0; *min_minor = 0;
            *max_major = 0; *max_minor = 0;
            break;
    }
}