#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "../include/ftview_tag.h"
#include "../include/ftview_common.h"

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)
#define CHECK_EQ(a,b) do { if ((a)!=(b)) { printf("FAIL: %s == %s (got %d, expected %d)\n", #a, #b, (int)(a), (int)(b)); return; } } while(0)
#define CHECK_DBL_EQ(a,b,tol) do { if (fabs((a)-(b)) > (tol)) { printf("FAIL: %s ≈ %s (got %.6f, expected %.6f)\n", #a, #b, a, b); return; } } while(0)

static void test_fnv1a_hash(void) {
    TEST("FNV-1a hash basic");
    uint32_t h1 = ftview_tag_name_hash("Hello");
    uint32_t h2 = ftview_tag_name_hash("Hello");
    CHECK(h1 == h2); /* deterministic */
    CHECK(ftview_tag_name_hash("Hello") != ftview_tag_name_hash("World"));
    CHECK(ftview_tag_name_hash("") != 0);
    CHECK(ftview_tag_name_hash(NULL) == 0);
    PASS();
}

static void test_tagdb_init(void) {
    TEST("Tag DB initialisation");
    ftview_tagdb_t db;
    ftview_tagdb_init(&db);
    CHECK(ftview_tagdb_count(&db) == 0);
    /* All buckets should be empty */
    for (uint32_t i = 0; i < FTVIEW_TAG_HASH_BUCKETS; i++) {
        CHECK(db.bucket[i] == 0);
    }
    PASS();
}

static void test_tagdb_add_find(void) {
    TEST("Tag DB add and find");
    ftview_tagdb_t db;
    ftview_tagdb_init(&db);

    ftview_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    strcpy(tag.name, "PV1001");
    strcpy(tag.description, "Reactor temperature");
    tag.type = FTVIEW_TYPE_ANALOG;
    tag.mode = FTVIEW_TAG_MODE_MEMORY;
    tag.eu.range_lo = 0.0;
    tag.eu.range_hi = 300.0;
    tag.eu.clamp_lo = 0.0;
    tag.eu.clamp_hi = 300.0;
    tag.eu.precision = 1;
    strcpy(tag.eu.label, "degC");

    CHECK(ftview_tagdb_add(&db, &tag) == FTVIEW_OK);
    CHECK(ftview_tagdb_count(&db) == 1);

    ftview_tag_t *found = ftview_tagdb_find(&db, "PV1001");
    CHECK(found != NULL);
    CHECK(strcmp(found->name, "PV1001") == 0);
    CHECK(found->type == FTVIEW_TYPE_ANALOG);

    /* Find non-existent */
    CHECK(ftview_tagdb_find(&db, "NONEXISTENT") == NULL);
    PASS();
}

static void test_tagdb_duplicate(void) {
    TEST("Tag DB duplicate detection");
    ftview_tagdb_t db;
    ftview_tagdb_init(&db);

    ftview_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    strcpy(tag.name, "TAG1");
    tag.type = FTVIEW_TYPE_DIGITAL;
    CHECK(ftview_tagdb_add(&db, &tag) == FTVIEW_OK);
    /* Second add of same name must fail */
    CHECK(ftview_tagdb_add(&db, &tag) == FTVIEW_ERR_DUPLICATE_TAG);
    PASS();
}

static void test_tagdb_remove(void) {
    TEST("Tag DB remove");
    ftview_tagdb_t db;
    ftview_tagdb_init(&db);

    ftview_tag_t tags[3];
    for (int i = 0; i < 3; i++) {
        memset(&tags[i], 0, sizeof(tags[i]));
        snprintf(tags[i].name, FTVIEW_TAG_NAME_MAX_LEN, "TAG%d", i);
        tags[i].type = FTVIEW_TYPE_ANALOG;
        CHECK(ftview_tagdb_add(&db, &tags[i]) == FTVIEW_OK);
    }
    CHECK(ftview_tagdb_count(&db) == 3);

    CHECK(ftview_tagdb_remove(&db, "TAG1") == FTVIEW_OK);
    CHECK(ftview_tagdb_count(&db) == 2);
    CHECK(ftview_tagdb_find(&db, "TAG1") == NULL);
    CHECK(ftview_tagdb_find(&db, "TAG0") != NULL);
    CHECK(ftview_tagdb_find(&db, "TAG2") != NULL);

    /* Remove non-existent */
    CHECK(ftview_tagdb_remove(&db, "NONEXISTENT") == FTVIEW_ERR_TAG_NOT_FOUND);
    PASS();
}

static void test_scale_linear(void) {
    TEST("Linear scaling 4-20mA → 0-300 degC");
    /* 4-20 mA mapped to 4000-20000 raw counts */
    double result = ftview_tag_scale_raw_to_eu(4000.0, 4000.0, 20000.0,
                                                 0.0, 300.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 0.0, 0.001);

    result = ftview_tag_scale_raw_to_eu(20000.0, 4000.0, 20000.0,
                                         0.0, 300.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 300.0, 0.001);

    /* Mid-scale */
    result = ftview_tag_scale_raw_to_eu(12000.0, 4000.0, 20000.0,
                                         0.0, 300.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 150.0, 0.001);

    /* Clamp below range */
    result = ftview_tag_scale_raw_to_eu(0.0, 4000.0, 20000.0,
                                         0.0, 300.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 0.0, 0.001);
    PASS();
}

static void test_scale_sqrt(void) {
    TEST("Square-root scaling for DP flow");
    /* raw 4-20mA, EU 0-1000 L/h, sqrt extraction */
    double result = ftview_tag_scale_raw_to_eu(4000.0, 4000.0, 20000.0,
                                                 0.0, 1000.0, FTVIEW_SCALE_SQRT);
    CHECK_DBL_EQ(result, 0.0, 0.01);

    result = ftview_tag_scale_raw_to_eu(20000.0, 4000.0, 20000.0,
                                         0.0, 1000.0, FTVIEW_SCALE_SQRT);
    CHECK_DBL_EQ(result, 1000.0, 0.01);

    /* 25% = sqrt(0.25)*1000 = 500 */
    result = ftview_tag_scale_raw_to_eu(8000.0, 4000.0, 20000.0,
                                         0.0, 1000.0, FTVIEW_SCALE_SQRT);
    CHECK_DBL_EQ(result, 500.0, 0.1);
    PASS();
}

static void test_scale_zero_span(void) {
    TEST("Scaling with zero span");
    /* Zero raw span: should return eu_lo */
    double result = ftview_tag_scale_raw_to_eu(5000.0, 5000.0, 5000.0,
                                                 0.0, 100.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 0.0, 0.001);
    PASS();
}

static void test_reverse_scale(void) {
    TEST("Reverse scaling EU → raw");
    double result = ftview_tag_scale_eu_to_raw(150.0, 4000.0, 20000.0,
                                                 0.0, 300.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 12000.0, 1.0);

    result = ftview_tag_scale_eu_to_raw(0.0, 4000.0, 20000.0,
                                         0.0, 300.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 4000.0, 1.0);
    PASS();
}

static void test_drt_parse(void) {
    TEST("DRT parsing");
    char shortcut[64];
    char addr[FTVIEW_TAG_ADDR_MAX_LEN];

    ftview_err_t err = ftview_tag_parse_drt("{[PLC1]N7:0}", shortcut,
                                              sizeof(shortcut), addr, sizeof(addr));
    CHECK(err == FTVIEW_OK);
    CHECK(strcmp(shortcut, "PLC1") == 0);
    CHECK(strcmp(addr, "N7:0") == 0);

    /* Bad format */
    err = ftview_tag_parse_drt("NOT_A_DRT", shortcut,
                                 sizeof(shortcut), addr, sizeof(addr));
    CHECK(err == FTVIEW_ERR_INVALID_PARAM);

    /* NULL safety */
    err = ftview_tag_parse_drt(NULL, shortcut, sizeof(shortcut), addr, sizeof(addr));
    CHECK(err == FTVIEW_ERR_NULL_PTR);
    PASS();
}

static void test_tag_resolve(void) {
    TEST("Tag resolution");
    ftview_tagdb_t db;
    ftview_tagdb_init(&db);

    ftview_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    strcpy(tag.name, "SP1001");
    tag.type = FTVIEW_TYPE_ANALOG;
    tag.current_value.type = FTVIEW_TYPE_ANALOG;
    tag.current_value.data.analog = 150.0;
    tag.current_value.quality = FTVIEW_QUALITY_GOOD;
    ftview_tagdb_add(&db, &tag);

    ftview_value_t val;
    memset(&val, 0, sizeof(val));
    ftview_err_t err = ftview_tagdb_resolve(&db, "SP1001", &val);
    CHECK(err == FTVIEW_OK);
    CHECK(val.type == FTVIEW_TYPE_ANALOG);
    CHECK_DBL_EQ(val.data.analog, 150.0, 0.001);

    err = ftview_tagdb_resolve(&db, "NONEXISTENT", &val);
    CHECK(err == FTVIEW_ERR_TAG_NOT_FOUND);
    PASS();
}

static void test_tag_update_value(void) {
    TEST("Tag value update");
    ftview_tag_t tag;
    memset(&tag, 0, sizeof(tag));
    tag.type = FTVIEW_TYPE_ANALOG;

    ftview_value_t new_val;
    memset(&new_val, 0, sizeof(new_val));
    new_val.type = FTVIEW_TYPE_ANALOG;
    new_val.data.analog = 275.5;
    new_val.quality = FTVIEW_QUALITY_GOOD;
    new_val.timestamp_ms = 1000;

    CHECK(ftview_tag_update_value(&tag, &new_val) == FTVIEW_OK);
    CHECK_DBL_EQ(tag.current_value.data.analog, 275.5, 0.001);
    CHECK(tag.current_value.quality == FTVIEW_QUALITY_GOOD);

    /* Type mismatch */
    new_val.type = FTVIEW_TYPE_DIGITAL;
    CHECK(ftview_tag_update_value(&tag, &new_val) == FTVIEW_ERR_INVALID_PARAM);
    PASS();
}

static void test_strerror(void) {
    TEST("Error string lookup");
    CHECK(strcmp(ftview_strerror(FTVIEW_OK), "Success") == 0);
    CHECK(strcmp(ftview_strerror(FTVIEW_ERR_TAG_NOT_FOUND), "Tag not found") == 0);
    CHECK(strcmp(ftview_strerror(FTVIEW_ERR_AUTH_FAILED), "Authentication failed") == 0);
    PASS();
}

static void test_scaling_edge_cases(void) {
    TEST("Scaling edge cases");
    /* Negative values */
    double result = ftview_tag_scale_raw_to_eu(-10.0, 0.0, 100.0,
                                                 -50.0, 50.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, -50.0, 0.001);

    /* All zeros */
    result = ftview_tag_scale_raw_to_eu(0.0, 0.0, 0.0, 0.0, 0.0, FTVIEW_SCALE_LINEAR);
    CHECK_DBL_EQ(result, 0.0, 0.001);
    PASS();
}

int main(void) {
    printf("=== mini-factorytalk-view-hmi: Tag Database Tests ===\n\n");

    test_fnv1a_hash();
    test_tagdb_init();
    test_tagdb_add_find();
    test_tagdb_duplicate();
    test_tagdb_remove();
    test_scale_linear();
    test_scale_sqrt();
    test_scale_zero_span();
    test_reverse_scale();
    test_drt_parse();
    test_tag_resolve();
    test_tag_update_value();
    test_strerror();
    test_scaling_edge_cases();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
