#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../include/ftview_security.h"
#include "../include/ftview_common.h"

static int tests_run = 0, tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return; } while(0)
#define CHECK(cond) do { if (!(cond)) { FAIL(#cond); return; } } while(0)
#define CHECK_EQ(a,b) do { if ((a)!=(b)) { printf("FAIL: %s==%s (%d vs %d)\n",#a,#b,(int)(a),(int)(b)); return; } } while(0)

static void test_password_hash(void) {
    TEST("Password hashing and verification");
    uint8_t salt[FTVIEW_SEC_SALT_LEN];
    uint8_t hash[FTVIEW_SEC_PASSHASH_LEN];
    memset(salt, 0xAB, FTVIEW_SEC_SALT_LEN);

    ftview_security_hash_password("MySecurePass123", salt, FTVIEW_SEC_SALT_LEN, hash);
    /* Hash must not be all zeros */
    int zero_count = 0;
    for (int i = 0; i < 32; i++) { if (hash[i] == 0) zero_count++; }
    CHECK(zero_count < 32);

    /* Verify correct password */
    CHECK(ftview_security_verify_password("MySecurePass123", hash, salt, FTVIEW_SEC_SALT_LEN) == true);

    /* Verify wrong password */
    CHECK(ftview_security_verify_password("WrongPassword", hash, salt, FTVIEW_SEC_SALT_LEN) == false);
    PASS();
}

static void test_security_mgr_init(void) {
    TEST("Security manager initialisation");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);
    CHECK(mgr.user_count == 0);
    CHECK(mgr.role_count == 0);
    CHECK(mgr.max_failed_attempts == 5);
    PASS();
}

static void test_add_role(void) {
    TEST("Add security role");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);

    ftview_role_t role;
    memset(&role, 0, sizeof(role));
    strcpy(role.name, "Operator");
    role.privilege_mask = FTVIEW_PRIV_VIEW | FTVIEW_PRIV_ACK_ALARMS | FTVIEW_PRIV_CHANGE_SP;

    CHECK(ftview_security_add_role(&mgr, &role) == FTVIEW_OK);
    CHECK(mgr.role_count == 1);
    CHECK(mgr.roles[0].privilege_mask ==
          (FTVIEW_PRIV_VIEW | FTVIEW_PRIV_ACK_ALARMS | FTVIEW_PRIV_CHANGE_SP));
    PASS();
}

static void test_add_user(void) {
    TEST("Add user with password");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);

    /* Create role first */
    ftview_role_t role;
    memset(&role, 0, sizeof(role));
    strcpy(role.name, "Admin");
    role.privilege_mask = FTVIEW_PRIV_ALL;
    ftview_security_add_role(&mgr, &role);

    CHECK(ftview_security_add_user(&mgr, "admin", "admin123", 1) == FTVIEW_OK);
    CHECK(mgr.user_count == 1);
    CHECK(strcmp(mgr.users[0].username, "admin") == 0);
    CHECK(mgr.users[0].enabled == true);

    /* Duplicate */
    CHECK(ftview_security_add_user(&mgr, "admin", "pwd", 1) == FTVIEW_ERR_DUPLICATE_TAG);
    PASS();
}

static void test_login_logout(void) {
    TEST("Login and logout");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);

    ftview_role_t role;
    memset(&role, 0, sizeof(role));
    strcpy(role.name, "Operator");
    role.privilege_mask = FTVIEW_PRIV_VIEW | FTVIEW_PRIV_ACK_ALARMS;
    ftview_security_add_role(&mgr, &role);

    ftview_security_add_user(&mgr, "op1", "password", 1);

    /* Successful login */
    uint32_t sid = ftview_security_login(&mgr, "op1", "password", "STATION1");
    CHECK(sid > 0);

    /* Check privilege */
    CHECK(ftview_security_check_privilege(&mgr, sid, FTVIEW_PRIV_VIEW) == true);
    CHECK(ftview_security_check_privilege(&mgr, sid, FTVIEW_PRIV_ACK_ALARMS) == true);
    CHECK(ftview_security_check_privilege(&mgr, sid, FTVIEW_PRIV_ADMIN) == false);

    /* Logout */
    CHECK(ftview_security_logout(&mgr, sid, "STATION1") == FTVIEW_OK);
    /* Privilege check after logout */
    CHECK(ftview_security_check_privilege(&mgr, sid, FTVIEW_PRIV_VIEW) == false);
    PASS();
}

static void test_login_failures(void) {
    TEST("Login failure handling");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);
    mgr.max_failed_attempts = 3;

    ftview_role_t role;
    memset(&role, 0, sizeof(role));
    strcpy(role.name, "Op");
    role.privilege_mask = FTVIEW_PRIV_VIEW;
    ftview_security_add_role(&mgr, &role);

    ftview_security_add_user(&mgr, "user1", "correct", 1);

    /* Wrong password 3 times → lockout on 3rd */
    CHECK(ftview_security_login(&mgr, "user1", "wrong1", "") == 0);
    CHECK(mgr.users[0].failed_attempts == 1);

    CHECK(ftview_security_login(&mgr, "user1", "wrong2", "") == 0);
    CHECK(mgr.users[0].failed_attempts == 2);

    CHECK(ftview_security_login(&mgr, "user1", "wrong3", "") == 0);
    CHECK(mgr.users[0].locked_out == true);

    /* Locked out: even correct password fails */
    CHECK(ftview_security_login(&mgr, "user1", "correct", "") == 0);
    PASS();
}

static void test_audit_trail(void) {
    TEST("Audit trail logging");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);

    CHECK(ftview_security_audit_log(&mgr, FTVIEW_AUDIT_LOGIN, "op1",
                                     "Login successful", "STN01", true) == FTVIEW_OK);
    CHECK(mgr.audit_count == 1);
    CHECK(mgr.audit[0].event == FTVIEW_AUDIT_LOGIN);
    CHECK(mgr.audit[0].success == true);

    /* Query */
    ftview_audit_entry_t out[10];
    uint32_t n = ftview_security_audit_query(&mgr, 0, 100000, out, 10);
    CHECK(n == 1);
    PASS();
}

static void test_esign(void) {
    TEST("21 CFR Part 11 electronic signature");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);

    ftview_role_t role;
    memset(&role, 0, sizeof(role));
    strcpy(role.name, "Supervisor");
    role.privilege_mask = FTVIEW_PRIV_VIEW | FTVIEW_PRIV_ESIGN;
    ftview_security_add_role(&mgr, &role);

    ftview_security_add_user(&mgr, "super1", "pass", 1);
    uint32_t sid = ftview_security_login(&mgr, "super1", "pass", "STN01");
    CHECK(sid > 0);

    char sig[256];
    CHECK(ftview_security_esign(&mgr, sid, "Approved batch release", sig, sizeof(sig)) == FTVIEW_OK);
    CHECK(strlen(sig) > 0);
    PASS();
}

static void test_session_ping_expire(void) {
    TEST("Session ping and expiry");
    ftview_security_mgr_t mgr;
    ftview_security_mgr_init(&mgr);

    ftview_role_t role;
    memset(&role, 0, sizeof(role));
    strcpy(role.name, "Op");
    role.privilege_mask = FTVIEW_PRIV_VIEW;
    ftview_security_add_role(&mgr, &role);

    ftview_security_add_user(&mgr, "op2", "pass", 1);
    uint32_t sid = ftview_security_login(&mgr, "op2", "pass", "STN02");
    CHECK(sid > 0);

    /* Ping */
    CHECK(ftview_security_session_ping(&mgr, sid) == FTVIEW_OK);

    /* Expire sessions with now_ms far in future */
    uint32_t expired = ftview_security_expire_sessions(&mgr,
        FTVIEW_SEC_SESSION_TIMEOUT_MS + 1000);
    CHECK(expired >= 1);
    PASS();
}

int main(void) {
    printf("=== mini-factorytalk-view-hmi: Security Tests ===\n\n");

    test_password_hash();
    test_security_mgr_init();
    test_add_role();
    test_add_user();
    test_login_logout();
    test_login_failures();
    test_audit_trail();
    test_esign();
    test_session_ping_expire();

    printf("\nResults: %d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
