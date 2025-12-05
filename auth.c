#include "auth.h"
#include "db.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define MD5_STATIC static
#include "md5.inl"

void hash_password(const char *password, char *hashed) {
    md5_state_t state;
    md5_init(&state);
    md5_append(&state, (const md5_byte_t *)password, strlen(password));
    md5_byte_t digest[16];
    md5_finish(&state, digest);
    for (int i = 0; i < 16; i++) {
        sprintf(hashed + i*2, "%02x", digest[i]);
    }
    hashed[32] = '\0';
}

static int create_default_admin() {
    // Create default admin user with username=billyjohnlendio10@gmail.com and password=admin123 if not exists
    const char *check_sql = "SELECT COUNT(*) FROM users WHERE username='billyjohnlendio10@gmail.com';";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare check default admin statement\n");
        return rc;
    }
    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        // Insert default admin user
        char insert_sql[256];
        sprintf(insert_sql, "INSERT INTO users (username, password, role) VALUES ('billyjohnlendio10@gmail.com', 'admin123', 'admin');");
        return sqlite3_exec(db, insert_sql, NULL, NULL, NULL);
    }
    return SQLITE_OK;
}

static int create_default_teacher() {
    // Create default teacher user with name=Benna Mae Oyangorin, username=bennamae, password=teacher123, access_code=benna123 if not exists
    const char *check_sql = "SELECT COUNT(*) FROM users WHERE username='bennamae';";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare check default teacher statement\n");
        return rc;
    }
    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        // Insert default teacher user
        char insert_sql[512];
        sprintf(insert_sql, "INSERT INTO users (name, username, password, role, access_code) VALUES ('Benna Mae Oyangorin', 'bennamae', 'teacher123', 'teacher', 'benna123');");
        return sqlite3_exec(db, insert_sql, NULL, NULL, NULL);
    }
    return SQLITE_OK;
}

int auth_init(void) {
    int rc = create_default_admin();
    if (rc != SQLITE_OK) return rc;
    return create_default_teacher();
}

int auth_handle_login(const char *username, const char *password) {
    // Check if user is locked out (3 attempts)
    int attempts = db_get_login_attempts(username);
    if (attempts >= 3) {
        return -2; // Account locked
    }

    // Check plain text password for all users
    int valid = db_check_user_credentials(username, password);

    if (valid) {
        // Successful login, reset attempts
        db_reset_login_attempts(username);
        return 1;
    } else {
        // Failed login, increment attempts
        db_increment_login_attempts(username);
        return 0;
    }
}

int auth_get_user_id(const char *username) {
    return db_get_user_id_by_username(username);
}

// Simple token storage (in memory, not persistent)
#define MAX_TOKENS 100
static char tokens[MAX_TOKENS][65];
static int user_ids[MAX_TOKENS];
static int token_count = 0;

void auth_generate_token(char *token, size_t len) {
    // Generate a simple random token
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (size_t i = 0; i < len - 1; ++i) {
        token[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    token[len - 1] = '\0';
}

int auth_validate_token(const char *token) {
    for (int i = 0; i < token_count; ++i) {
        if (strcmp(tokens[i], token) == 0) {
            return user_ids[i];
        }
    }
    return -1;
}

// Helper to store token
void auth_store_token(const char *token, int user_id) {
    if (token_count < MAX_TOKENS) {
        strcpy(tokens[token_count], token);
        user_ids[token_count] = user_id;
        token_count++;
    }
}
