#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../include/ftview_tag.h"

int main(void) {
    ftview_tagdb_t db;
    ftview_tagdb_init(&db);
    ftview_tag_t tag;
    
    clock_t start = clock();
    for (int i = 0; i < 1000; i++) {
        memset(&tag, 0, sizeof(tag));
        snprintf(tag.name, FTVIEW_TAG_NAME_MAX_LEN, "TAG_%d", i);
        tag.type = FTVIEW_TYPE_ANALOG;
        ftview_tagdb_add(&db, &tag);
    }
    clock_t t_add = clock();
    
    for (int i = 0; i < 100000; i++) {
        char name[128];
        snprintf(name, sizeof(name), "TAG_%d", i % 1000);
        ftview_tagdb_find(&db, name);
    }
    clock_t t_find = clock();
    
    double add_ms = 1000.0 * (t_add - start) / CLOCKS_PER_SEC;
    double find_ms = 1000.0 * (t_find - t_add) / CLOCKS_PER_SEC;
    
    printf("Tag DB Benchmark:\n");
    printf("  Add 1000 tags: %8.2f ms (%.2f us/tag)\n", add_ms, add_ms * 1000.0 / 1000.0);
    printf("  Find 100k:     %8.2f ms (%.2f ns/lookup)\n", find_ms, find_ms * 1000.0 / 100.0);
    return 0;
}
