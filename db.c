#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sqlite3 *db = NULL;

static int execute_sql(const char *sql) {
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    return rc;
}

int db_init(const char *filename) {
    int rc = sqlite3_open(filename, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    // Create users table
    const char *sql_users = "CREATE TABLE IF NOT EXISTS users ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "username TEXT UNIQUE NOT NULL,"
                            "password TEXT NOT NULL,"
                            "role TEXT NOT NULL DEFAULT 'teacher',"
                            "access_code TEXT,"
                            "login_attempts INTEGER DEFAULT 0,"
                            "name TEXT"
                            ");";

    rc = execute_sql(sql_users);
    if (rc != SQLITE_OK) return rc;

    // Add name column if it doesn't exist (for backward compatibility)
    execute_sql("ALTER TABLE users ADD COLUMN name TEXT;");

    // Create programs table
    const char *sql_programs = "CREATE TABLE IF NOT EXISTS programs ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "name TEXT UNIQUE NOT NULL"
                               ");";

    rc = execute_sql(sql_programs);
    if (rc != SQLITE_OK) return rc;

    // Create subjects table
    const char *sql_subjects = "CREATE TABLE IF NOT EXISTS subjects ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               "program TEXT NOT NULL,"
                               "grade_level TEXT NOT NULL,"
                               "semester TEXT NOT NULL,"
                               "subject TEXT NOT NULL,"
                               "teacher_id INTEGER,"
                               "FOREIGN KEY (teacher_id) REFERENCES users(id) ON DELETE SET NULL"
                               ");";

    rc = execute_sql(sql_subjects);
    if (rc != SQLITE_OK) return rc;

    // Create materials table
    const char *sql_materials = "CREATE TABLE IF NOT EXISTS materials ("
                                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                "subject_id INTEGER NOT NULL,"
                                "category TEXT NOT NULL,"
                                "original_filename TEXT NOT NULL,"
                                "file_data TEXT NOT NULL,"  // Base64 encoded
                                "uploaded_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                                "FOREIGN KEY (subject_id) REFERENCES subjects(id) ON DELETE CASCADE"
                                ");";

    rc = execute_sql(sql_materials);
    if (rc != SQLITE_OK) return rc;

    // Indexes for performance
    execute_sql("CREATE INDEX IF NOT EXISTS idx_subjects_program ON subjects(program);");
    execute_sql("CREATE INDEX IF NOT EXISTS idx_materials_subject ON materials(subject_id);");

    // Insert sample data
    // Sample users (passwords stored in plain text)
    // Insert new teacher account
    execute_sql("INSERT OR IGNORE INTO users (name, username, password, role, access_code) VALUES ('Benna Mae Oyangorin', 'bennamae', 'teacher123', 'teacher', 'benna123');");

    // Sample programs
    execute_sql("INSERT OR IGNORE INTO programs (name) VALUES ('Computer Science');");
    execute_sql("INSERT OR IGNORE INTO programs (name) VALUES ('Mathematics');");
    execute_sql("INSERT OR IGNORE INTO programs (name) VALUES ('Physics');");

    // Sample subjects
    execute_sql("INSERT OR IGNORE INTO subjects (program, grade_level, semester, subject, teacher_id) VALUES ('Computer Science', 'Grade 10', 'Semester 1', 'Programming Basics', 1);");
    execute_sql("INSERT OR IGNORE INTO subjects (program, grade_level, semester, subject, teacher_id) VALUES ('Mathematics', 'Grade 11', 'Semester 2', 'Calculus', 2);");

    // Sample materials
    execute_sql("INSERT OR IGNORE INTO materials (subject_id, category, original_filename, file_data) VALUES (1, 'Lecture Notes', 'notes.pdf', 'base64encodeddata');");
    execute_sql("INSERT OR IGNORE INTO materials (subject_id, category, original_filename, file_data) VALUES (2, 'Assignments', 'hw1.pdf', 'base64encodeddata');");

    return SQLITE_OK;
}

void db_close(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

int db_check_user_credentials(const char *username, const char *password) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM users WHERE username = ? AND password = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement\n");
        return 0;
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    int count = 0;
    if (rc == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count > 0;
}

// Materials CRUD
int db_create_material(int subject_id, const char *category, const char *original_filename, const char *file_data) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO materials (subject_id, category, original_filename, file_data) VALUES (?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, subject_id);
    sqlite3_bind_text(stmt, 2, category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, original_filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, file_data, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_read_material(int id, int *subject_id, char *category, char *original_filename, char *file_data) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT subject_id, category, original_filename, file_data FROM materials WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        *subject_id = sqlite3_column_int(stmt, 0);
        strcpy(category, (const char *)sqlite3_column_text(stmt, 1));
        strcpy(original_filename, (const char *)sqlite3_column_text(stmt, 2));
        strcpy(file_data, (const char *)sqlite3_column_text(stmt, 3));
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW ? SQLITE_OK : SQLITE_NOTFOUND;
}

int db_update_material(int id, int subject_id, const char *category, const char *original_filename, const char *file_data) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE materials SET subject_id = ?, category = ?, original_filename = ?, file_data = ? WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, subject_id);
    sqlite3_bind_text(stmt, 2, category, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, original_filename, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, file_data, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_delete_material(int id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM materials WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

// Subjects CRUD
int db_create_subject(const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO subjects (program, grade_level, semester, subject, teacher_id) VALUES (?, ?, ?, ?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(stmt, 1, program, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, grade_level, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, semester, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, subject, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, teacher_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_read_subject(int id, char *program, char *grade_level, char *semester, char *subject, int *teacher_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT program, grade_level, semester, subject, teacher_id FROM subjects WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        strcpy(program, (const char *)sqlite3_column_text(stmt, 0));
        strcpy(grade_level, (const char *)sqlite3_column_text(stmt, 1));
        strcpy(semester, (const char *)sqlite3_column_text(stmt, 2));
        strcpy(subject, (const char *)sqlite3_column_text(stmt, 3));
        *teacher_id = sqlite3_column_int(stmt, 4);
    }
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW ? SQLITE_OK : SQLITE_NOTFOUND;
}

int db_update_subject(int id, const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE subjects SET program = ?, grade_level = ?, semester = ?, subject = ?, teacher_id = ? WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(stmt, 1, program, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, grade_level, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, semester, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, subject, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, teacher_id);
    sqlite3_bind_int(stmt, 6, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_delete_subject(int id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM subjects WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

// Additional query functions
char* db_get_materials_by_teacher_json(int teacher_id) {
    const char *sql = "SELECT m.id, m.subject_id, m.category, m.original_filename, m.uploaded_at, s.program, s.subject "
                      "FROM materials m JOIN subjects s ON m.subject_id = s.id WHERE s.teacher_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, teacher_id);

    char *json = malloc(4096); // Allocate buffer
    strcpy(json, "[");
    int first = 1;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (!first) strcat(json, ",");
        char item[512];
        sprintf(item, "{\"id\":%d,\"subject_id\":%d,\"category\":\"%s\",\"file_name\":\"%s\",\"uploaded_at\":\"%s\",\"program_name\":\"%s\",\"subject_name\":\"%s\"}",
                sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4), sqlite3_column_text(stmt, 5), sqlite3_column_text(stmt, 6));
        strcat(json, item);
        first = 0;
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);
    return json;
}

char* db_get_subjects_by_teacher_json(int teacher_id) {
    const char *sql = "SELECT id, program, grade_level, semester, subject FROM subjects WHERE teacher_id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, teacher_id);

    char *json = malloc(4096);
    strcpy(json, "[");
    int first = 1;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (!first) strcat(json, ",");
        char item[512];
        sprintf(item, "{\"id\":%d,\"program\":\"%s\",\"grade_level\":\"%s\",\"semester\":\"%s\",\"subject\":\"%s\"}",
                sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4));
        strcat(json, item);
        first = 0;
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);
    return json;
}

char* db_get_all_subjects_json(void) {
    const char *sql = "SELECT id, program, grade_level, semester, subject, teacher_id FROM subjects;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    char *json = malloc(4096);
    strcpy(json, "[");
    int first = 1;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (!first) strcat(json, ",");
        char item[512];
        sprintf(item, "{\"id\":%d,\"program\":\"%s\",\"grade_level\":\"%s\",\"semester\":\"%s\",\"subject\":\"%s\",\"teacher_id\":%d}",
                sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1),
                sqlite3_column_text(stmt, 2), sqlite3_column_text(stmt, 3),
                sqlite3_column_text(stmt, 4), sqlite3_column_int(stmt, 5));
        strcat(json, item);
        first = 0;
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);
    return json;
}

char* db_get_dashboard_data_json(int teacher_id) {
    // Get total materials
    const char *sql_total = "SELECT COUNT(*) FROM materials m JOIN subjects s ON m.subject_id = s.id WHERE s.teacher_id = ?;";
    sqlite3_stmt *stmt_total;
    int rc = sqlite3_prepare_v2(db, sql_total, -1, &stmt_total, NULL);
    if (rc != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt_total, 1, teacher_id);
    rc = sqlite3_step(stmt_total);
    int total = 0;
    if (rc == SQLITE_ROW) total = sqlite3_column_int(stmt_total, 0);
    sqlite3_finalize(stmt_total);

    // Get stats by category
    const char *sql_stats = "SELECT m.category, COUNT(*) FROM materials m JOIN subjects s ON m.subject_id = s.id WHERE s.teacher_id = ? GROUP BY m.category;";
    sqlite3_stmt *stmt_stats;
    rc = sqlite3_prepare_v2(db, sql_stats, -1, &stmt_stats, NULL);
    if (rc != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt_stats, 1, teacher_id);

    char stats[1024] = "{";
    int first = 1;
    while ((rc = sqlite3_step(stmt_stats)) == SQLITE_ROW) {
        if (!first) strcat(stats, ",");
        char item[128];
        sprintf(item, "\"%s\":%d", sqlite3_column_text(stmt_stats, 0), sqlite3_column_int(stmt_stats, 1));
        strcat(stats, item);
        first = 0;
    }
    strcat(stats, "}");
    sqlite3_finalize(stmt_stats);

    char *json = malloc(2048);
    sprintf(json, "{\"total\":%d,\"stats\":%s}", total, stats);
    return json;
}

int db_assign_subject_to_teacher(int subject_id, int teacher_id) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE subjects SET teacher_id = ? WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, teacher_id);
    sqlite3_bind_int(stmt, 2, subject_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_get_user_id_by_username(const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id FROM users WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int id = -1;
    if (rc == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return id;
}

const char* db_get_user_role(const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT role FROM users WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    const char *role = NULL;
    if (rc == SQLITE_ROW) {
        role = (const char *)sqlite3_column_text(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return role;
}

int db_get_login_attempts(const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT login_attempts FROM users WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    int attempts = -1;
    if (rc == SQLITE_ROW) {
        attempts = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return attempts;
}

int db_increment_login_attempts(const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE users SET login_attempts = login_attempts + 1 WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_reset_login_attempts(const char *username) {
    sqlite3_stmt *stmt;
    const char *sql = "UPDATE users SET login_attempts = 0 WHERE username = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

// Admin functions
char* db_get_all_programs_json(void) {
    const char *sql = "SELECT id, name FROM programs;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    char *json = malloc(4096);
    strcpy(json, "[");
    int first = 1;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (!first) strcat(json, ",");
        char item[256];
        sprintf(item, "{\"id\":%d,\"name\":\"%s\"}",
                sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1));
        strcat(json, item);
        first = 0;
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);
    return json;
}

char* db_get_all_teachers_json(void) {
    const char *sql = "SELECT id, name, username, password, access_code FROM users WHERE role = 'teacher';";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;

    char *json = malloc(4096);
    strcpy(json, "[");
    int first = 1;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (!first) strcat(json, ",");
        char item[512];
        sprintf(item, "{\"id\":%d,\"name\":\"%s\",\"username\":\"%s\",\"password\":\"%s\",\"access_code\":\"%s\"}",
                sqlite3_column_int(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_text(stmt, 2),
                sqlite3_column_text(stmt, 3), sqlite3_column_text(stmt, 4));
        strcat(json, item);
        first = 0;
    }
    strcat(json, "]");
    sqlite3_finalize(stmt);
    return json;
}

char* db_get_tracking_data_json(void) {
    // Get total users, subjects, materials
    const char *sql_stats = "SELECT "
                            "(SELECT COUNT(*) FROM users WHERE role = 'teacher') as teachers,"
                            "(SELECT COUNT(*) FROM subjects) as subjects,"
                            "(SELECT COUNT(*) FROM materials) as materials;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql_stats, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return NULL;
    rc = sqlite3_step(stmt);
    int teachers = 0, subjects = 0, materials = 0;
    if (rc == SQLITE_ROW) {
        teachers = sqlite3_column_int(stmt, 0);
        subjects = sqlite3_column_int(stmt, 1);
        materials = sqlite3_column_int(stmt, 2);
    }
    sqlite3_finalize(stmt);

    char *json = malloc(512);
    sprintf(json, "{\"teachers\":%d,\"subjects\":%d,\"materials\":%d}", teachers, subjects, materials);
    return json;
}

int db_create_program(const char *name, const char *subjects_json) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO programs (name) VALUES (?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_delete_program(int id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM programs WHERE id = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_create_teacher(const char *name, const char *username, const char *password, const char *access_code) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO users (name, username, password, role, access_code) VALUES (?, ?, ?, 'teacher', ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, password, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, access_code, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}

int db_delete_teacher(int id) {
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM users WHERE id = ? AND role = 'teacher';";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : rc;
}
