#ifndef MATERIALS_H
#define MATERIALS_H

// Create a material
int materials_create(int subject_id, const char *category, const char *original_filename, const char *file_data);

// Read a material by id
int materials_read(int id, int *subject_id, char *category, char *original_filename, char *file_data);

// Update a material by id
int materials_update(int id, int subject_id, const char *category, const char *original_filename, const char *file_data);

// Delete a material by id
int materials_delete(int id);

#endif // MATERIALS_H
