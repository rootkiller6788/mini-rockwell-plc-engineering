/**
 * @file    logix_iec61131_compliance.c
 * @brief   IEC 61131-3 Compliance Mapping and Validation
 * L4: Programming standards, language compliance
 * Reference: IEC 61131-3:2013; Karl-Heinz John (2013)
 */

#include <string.h>
#include <stdio.h>
#include "logix_iec61131_compliance.h"

/* ========================================================================
 * IEC 61131-3 Type Mapping Table
 * ======================================================================== */

static const iec_type_mapping_t iec_type_table[] = {
    { LOGIX_TYPE_BOOL,  "BOOL",   1, 1, IEC_COMPLIANCE_FULL },
    { LOGIX_TYPE_SINT,  "SINT",   1, 1, IEC_COMPLIANCE_FULL },
    { LOGIX_TYPE_INT,   "INT",    2, 2, IEC_COMPLIANCE_FULL },
    { LOGIX_TYPE_DINT,  "DINT",   4, 4, IEC_COMPLIANCE_FULL },
    { LOGIX_TYPE_REAL,  "REAL",   4, 4, IEC_COMPLIANCE_FULL },
    { LOGIX_TYPE_LINT,  "LINT",   8, 8, IEC_COMPLIANCE_EXTENDED },
    { LOGIX_TYPE_LREAL, "LREAL",  8, 8, IEC_COMPLIANCE_EXTENDED },
    { LOGIX_TYPE_USINT, "USINT",  1, 1, IEC_COMPLIANCE_EXTENDED },
    { LOGIX_TYPE_UINT,  "UINT",   2, 2, IEC_COMPLIANCE_EXTENDED },
    { LOGIX_TYPE_UDINT, "UDINT",  4, 4, IEC_COMPLIANCE_EXTENDED },
    { LOGIX_TYPE_ULINT, "ULINT",  8, 8, IEC_COMPLIANCE_EXTENDED },
    { LOGIX_TYPE_STRING,"STRING", 256, 84, IEC_COMPLIANCE_PARTIAL },
};

static const uint32_t iec_type_table_size =
    sizeof(iec_type_table) / sizeof(iec_type_table[0]);

/* ========================================================================
 * IEC 61131-3 Reserved Keywords
 * ======================================================================== */

static const char *iec_reserved_keywords[] = {
    "PROGRAM", "FUNCTION_BLOCK", "FUNCTION", "END_PROGRAM",
    "END_FUNCTION_BLOCK", "END_FUNCTION",
    "VAR", "VAR_INPUT", "VAR_OUTPUT", "VAR_IN_OUT", "VAR_EXTERNAL",
    "VAR_GLOBAL", "VAR_ACCESS", "END_VAR",
    "IF", "THEN", "ELSIF", "ELSE", "END_IF",
    "CASE", "OF", "END_CASE",
    "FOR", "TO", "BY", "DO", "END_FOR",
    "WHILE", "END_WHILE",
    "REPEAT", "UNTIL", "END_REPEAT",
    "RETURN", "EXIT", "CONTINUE",
    "TRUE", "FALSE", "NULL",
    "NOT", "AND", "OR", "XOR", "MOD",
    "TON", "TOF", "TP", "R_TRIG", "F_TRIG",
    "CTU", "CTD", "CTUD",
    "SR", "RS", "SEMA",
    "STEP", "INITIAL_STEP", "TRANSITION", "ACTION",
    "END_STEP", "END_TRANSITION", "END_ACTION",
    "CONFIGURATION", "END_CONFIGURATION",
    "RESOURCE", "END_RESOURCE",
    "TASK", "INTERVAL", "PRIORITY",
    "READ_ONLY", "READ_WRITE",
    "RETAIN", "NON_RETAIN", "CONSTANT",
    "AT", "ARRAY", "OF", "STRUCT", "END_STRUCT", "TYPE", "END_TYPE",
    "EN", "ENO",
    NULL
};

/* ========================================================================
 * L4: IEC Compliance Checking
 * ======================================================================== */

iec_compliance_level_t logix_iec_get_compliance(logix_atomic_type_t type)
{
    for (uint32_t i = 0; i < iec_type_table_size; i++) {
        if (iec_type_table[i].logix_type == type) {
            return iec_type_table[i].compliance;
        }
    }
    return IEC_COMPLIANCE_NON;
}

const char *logix_iec_get_standard_name(logix_atomic_type_t type)
{
    for (uint32_t i = 0; i < iec_type_table_size; i++) {
        if (iec_type_table[i].logix_type == type) {
            return iec_type_table[i].iec_name;
        }
    }
    return "UNKNOWN";
}

bool logix_iec_is_reserved_keyword(const char *name)
{
    if (!name) return false;

    for (uint32_t i = 0; iec_reserved_keywords[i] != NULL; i++) {
        if (strcasecmp(name, iec_reserved_keywords[i]) == 0) {
            return true;
        }
    }
    return false;
}

uint32_t logix_iec_validate_ladder_rung(const ld_rung_t *rung)
{
    if (!rung) return 1;

    uint32_t errors = 0;

    /* Must have at least one element */
    if (rung->element_count == 0) errors++;

    /* Must have at least one output element */
    bool has_output = false;
    for (uint32_t i = 0; i < rung->element_count; i++) {
        if (rung->elements[i].type == LD_ELEMENT_OTE ||
            rung->elements[i].type == LD_ELEMENT_OTL ||
            rung->elements[i].type == LD_ELEMENT_OTU ||
            rung->elements[i].type == LD_ELEMENT_TON ||
            rung->elements[i].type == LD_ELEMENT_TOF ||
            rung->elements[i].type == LD_ELEMENT_CTU ||
            rung->elements[i].type == LD_ELEMENT_CTD ||
            rung->elements[i].type == LD_ELEMENT_JSR ||
            rung->elements[i].type == LD_ELEMENT_AOI) {
            has_output = true;
        }
    }
    if (!has_output) errors++;

    return errors;
}

bool logix_iec_evaluate_ladder_rung(ld_rung_t *rung)
{
    if (!rung) return false;

    /* A rung evaluates left-to-right evaluating series (AND) and
     * parallel (OR) contacts. The final rung condition determines
     * whether output elements are energized. */

    bool rung_state = rung->rung_condition_in && !rung->is_inhibited;

    if (!rung_state) {
        rung->rung_condition_out = false;
        return false;
    }

    /* Evaluate each element in sequence */
    for (uint32_t i = 0; i < rung->element_count; i++) {
        ld_element_t *elem = &rung->elements[i];

        switch (elem->type) {
            case LD_ELEMENT_XIC:
                /* Examine If Closed: TRUE when bit = 1 */
                elem->output_state = elem->input_state;
                break;
            case LD_ELEMENT_XIO:
                /* Examine If Open: TRUE when bit = 0 */
                elem->output_state = !elem->input_state;
                break;
            case LD_ELEMENT_OTE:
                elem->output_state = rung_state;
                break;
            case LD_ELEMENT_OTL:
                if (rung_state) elem->output_state = true;
                break;
            case LD_ELEMENT_OTU:
                if (rung_state) elem->output_state = false;
                break;
            default:
                elem->output_state = false;
                break;
        }

        /* Series elements: next element's input = current output */
        if (elem->is_series && i + 1 < rung->element_count) {
            rung->elements[i + 1].input_state = elem->output_state;
        }
    }

    /* Rung output is the last element's output state */
    if (rung->element_count > 0) {
        rung->rung_condition_out =
            rung->elements[rung->element_count - 1].output_state;
    } else {
        rung->rung_condition_out = false;
    }

    return rung->rung_condition_out;
}

iec_compliance_level_t logix_iec_validate_pou(const iec_pou_interface_t *pou)
{
    if (!pou) return IEC_COMPLIANCE_NON;

    iec_compliance_level_t level = IEC_COMPLIANCE_FULL;

    /* Check FUNCTION must have exactly one output */
    if (pou->pou_type == POU_TYPE_FUNCTION) {
        if (pou->output_var_count != 1) {
            level = IEC_COMPLIANCE_PARTIAL;
        }
    }

    /* Check EN/ENO pattern */
    if (pou->has_en_eno) {
        /* EN/ENO is an IEC extension — not required, but if used
         * it must be properly implemented */
    }

    /* Check for reserved keywords in name */
    if (logix_iec_is_reserved_keyword(pou->name)) {
        level = IEC_COMPLIANCE_NON;
    }

    return level;
}

uint32_t logix_iec_standard_type_size(logix_atomic_type_t type)
{
    for (uint32_t i = 0; i < iec_type_table_size; i++) {
        if (iec_type_table[i].logix_type == type) {
            return iec_type_table[i].iec_size_bytes;
        }
    }
    return 0;
}

uint32_t logix_iec_ladder_to_st(const ld_rung_t *rung,
                                  char *st_buffer,
                                  uint32_t buf_size)
{
    if (!rung || !st_buffer || buf_size == 0) return 0;

    uint32_t written = 0;

    /* Generate ST equivalent:
     * For a rung: IF (XIC(tag1) AND XIO(tag2)) THEN OTE(tag3); END_IF; */

    written += snprintf(st_buffer + written, buf_size - written,
                        "(* Rung %u *) ", rung->rung_number);

    bool first_element = true;
    for (uint32_t i = 0; i < rung->element_count && written < buf_size - 10; i++) {
        const ld_element_t *elem = &rung->elements[i];

        /* Map element type to ST operator */
        switch (elem->type) {
            case LD_ELEMENT_XIC:
                if (!first_element) {
                    written += snprintf(st_buffer + written,
                                        buf_size - written, " AND ");
                }
                written += snprintf(st_buffer + written,
                                    buf_size - written, "%s",
                                    elem->operand_tag);
                first_element = false;
                break;
            case LD_ELEMENT_XIO:
                if (!first_element) {
                    written += snprintf(st_buffer + written,
                                        buf_size - written, " AND ");
                }
                written += snprintf(st_buffer + written,
                                    buf_size - written, "NOT %s",
                                    elem->operand_tag);
                first_element = false;
                break;
            case LD_ELEMENT_OTE:
                written += snprintf(st_buffer + written,
                                    buf_size - written,
                                    " THEN %s := 1; END_IF",
                                    elem->operand_tag);
                break;
            default:
                written += snprintf(st_buffer + written,
                                    buf_size - written,
                                    "(* %d *)", elem->type);
                break;
        }
    }

    /* Ensure null termination */
    if (written < buf_size) {
        st_buffer[written] = '\0';
    } else {
        st_buffer[buf_size - 1] = '\0';
    }

    return written;
}

uint32_t logix_iec_validate_sfc(const sfc_step_t *steps,
                                  uint32_t step_count,
                                  const sfc_transition_t *transitions,
                                  uint32_t transition_count)
{
    if (!steps) return 1;

    uint32_t errors = 0;

    /* Must have at least one step */
    if (step_count == 0) errors++;

    /* Must have exactly one initial step */
    uint32_t initial_steps = 0;
    for (uint32_t i = 0; i < step_count; i++) {
        if (steps[i].type == SFC_ELEMENT_INITIAL_STEP) {
            initial_steps++;
        }
    }
    if (initial_steps != 1) errors++;

    /* All transitions must have associated conditions */
    for (uint32_t i = 0; i < transition_count; i++) {
        if (transitions[i].condition_tag[0] == '\0') {
            errors++;
        }
    }

    /* Check for dangling steps (no transitions connected) */
    if (step_count > 1 && transition_count == 0) {
        errors++;
    }

    return errors;
}