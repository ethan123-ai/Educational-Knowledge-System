#include "materials.h"
#include "db.h"

int materials_create(int subject_id, const char *category, const char *original_filename, const char *file_data) {
    return db_create_material(subject_id, category, original_filename, file_data);
}

int materials_read(int id, int *subject_id, char *category, char *original_filename, char *file_data) {
    return db_read_material(id, subject_id, category, original_filename, file_data);
}

int materials_update(int id, int subject_id, const char *category, const char *original_filename, const char *file_data) {
    return db_update_material(id, subject_id, category, original_filename, file_data);
}

int materials_delete(int id) {
    return db_delete_material(id);
}
