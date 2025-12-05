#include "subjects.h"
#include "db.h"

int subjects_create(const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id) {
    return db_create_subject(program, grade_level, semester, subject, teacher_id);
}

int subjects_read(int id, char *program, char *grade_level, char *semester, char *subject, int *teacher_id) {
    return db_read_subject(id, program, grade_level, semester, subject, teacher_id);
}

int subjects_update(int id, const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id) {
    return db_update_subject(id, program, grade_level, semester, subject, teacher_id);
}

int subjects_delete(int id) {
    return db_delete_subject(id);
}
