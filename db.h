#ifndef DB_H
#define DB_H

#include "sqlite-amalgamation-3460100/sqlite3.h"

extern sqlite3 *db;

// Initialize the SQLite database connection and create tables if not exist
int db_init(const char *filename);

// Close the SQLite database connection
void db_close(void);

// User-related database functions
int db_check_user_credentials(const char *username, const char *password);
int db_get_login_attempts(const char *username);
int db_increment_login_attempts(const char *username);
int db_reset_login_attempts(const char *username);

// Materials-related database functions
int db_create_material(int subject_id, const char *category, const char *original_filename, const char *file_data);
int db_read_material(int id, int *subject_id, char *category, char *original_filename, char *file_data);
int db_update_material(int id, int subject_id, const char *category, const char *original_filename, const char *file_data);
int db_delete_material(int id);

// Subjects-related database functions
int db_create_subject(const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id);
int db_read_subject(int id, char *program, char *grade_level, char *semester, char *subject, int *teacher_id);
int db_update_subject(int id, const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id);
int db_delete_subject(int id);

// Additional query functions for API
char* db_get_materials_by_teacher_json(int teacher_id);
char* db_get_subjects_by_teacher_json(int teacher_id);
char* db_get_all_subjects_json(void);
char* db_get_dashboard_data_json(int teacher_id);
int db_assign_subject_to_teacher(int subject_id, int teacher_id);
int db_get_user_id_by_username(const char *username);
const char* db_get_user_role(const char *username);

// Admin functions
char* db_get_all_programs_json(void);
char* db_get_all_teachers_json(void);
char* db_get_tracking_data_json(void);
int db_create_program(const char *name, const char *subjects_json);
int db_delete_program(int id);
int db_create_teacher(const char *name, const char *username, const char *password, const char *access_code);
int db_delete_teacher(int id);

#endif // DB_H