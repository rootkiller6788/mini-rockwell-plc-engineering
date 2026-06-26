/**
 * example02_tag_data_model.c
 *
 * ControlLogix Tag Data Model — Create, Write, Read, Alias, Produced/Consumed
 *
 * Demonstrates:
 *   - Controller-scope and Program-scope tag creation
 *   - Tag write/read for REAL, DINT, BOOL types
 *   - Tag name validation rules
 *   - Alias tag creation
 *   - Produced/Consumed tag setup
 *   - Memory statistics
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/logix_tag_model.h"

int main(void)
{
    printf("=== Example 02: Tag Data Model ===\n\n");

    /* --- Controller-scope tags --- */
    logix_tag_t tags[8];
    int n = 0;

    /* REAL tag: tank level */
    logix_tag_create_controller_scope(&tags[n++], "Tank101_Level", LOGIX_TYPE_REAL);
    float level = 75.5f;
    logix_tag_write(&tags[n-1], &level);
    printf("Created: %s (REAL) = %.1f\n", tags[n-1].name, level);

    /* DINT tag: batch counter */
    logix_tag_create_controller_scope(&tags[n++], "Batch_Count", LOGIX_TYPE_DINT);
    int32_t count = 42;
    logix_tag_write(&tags[n-1], &count);

    /* BOOL tag: motor run */
    logix_tag_create_controller_scope(&tags[n++], "Motor_Run_Cmd", LOGIX_TYPE_BOOL);
    bool run = true;
    logix_tag_write(&tags[n-1], &run);

    /* Program-scope tag */
    logix_tag_create_program_scope(&tags[n++], "Local_SP", LOGIX_TYPE_REAL);

    /* --- Alias tag --- */
    logix_tag_t alias;
    logix_tag_create_controller_scope(&alias, "PV_Alias", LOGIX_TYPE_ALIAS);
    logix_tag_set_alias(&alias, "Tank101_Level");
    printf("Alias: %s -> %s\n", alias.name, alias.alias_target);

    /* --- Produced/Consumed tags --- */
    logix_tag_t prod_tag, cons_tag;
    logix_tag_create_controller_scope(&prod_tag, "Produced_Data", LOGIX_TYPE_DINT);
    logix_tag_set_produced(&prod_tag, 20.0);  /* RPI = 20ms */
    printf("Produced: %s (RPI=20ms, ConnID=%u)\n",
           prod_tag.name, prod_tag.produced_connection_id);

    logix_tag_create_controller_scope(&cons_tag, "Consumed_Data", LOGIX_TYPE_DINT);
    logix_tag_set_consumed(&cons_tag, 20.0);
    printf("Consumed: %s (RPI=20ms)\n", cons_tag.name);

    /* --- Tag name validation --- */
    printf("\n--- Name Validation ---\n");
    const char *names[] = {"Tank101_Level", "123Bad", "", "My_Tag_1",
                           "A_Very_Long_Tag_Name_That_Exceeds_Max_Length"};
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
        printf("  '%s' -> %s\n", names[i],
               logix_tag_validate_name(names[i]) ? "OK" : "INVALID");
    }

    /* --- Type information --- */
    printf("\n--- Type Sizes ---\n");
    logix_atomic_type_t tlist[] = {LOGIX_TYPE_BOOL, LOGIX_TYPE_SINT,
        LOGIX_TYPE_INT, LOGIX_TYPE_DINT, LOGIX_TYPE_LINT,
        LOGIX_TYPE_REAL, LOGIX_TYPE_LREAL};
    for (size_t i = 0; i < sizeof(tlist)/sizeof(tlist[0]); i++) {
        printf("  %-10s: %u bytes\n",
               logix_type_name(tlist[i]), logix_type_size(tlist[i]));
    }

    /* --- Readback test --- */
    printf("\n--- Readback ---\n");
    float rval = 0.0f;
    logix_tag_read(&tags[0], &rval);
    printf("  Read %s = %.1f (expected 75.5)\n", tags[0].name, rval);

    /* --- Memory stats --- */
    logix_memory_stats_t stats;
    logix_memory_update_stats(tags, n, &stats);
    printf("\n--- Memory ---\n");
    printf("  Tags: %u controller + %u program\n",
           stats.tag_count_controller_scope, stats.tag_count_program_scope);

    printf("\n=== Tag model demo complete ===\n");
    return 0;
}
