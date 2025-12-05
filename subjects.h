#ifndef SUBJECTS_H
#define SUBJECTS_H

// Create a subject
int subjects_create(const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id);

// Read a subject by id
int subjects_read(int id, char *program, char *grade_level, char *semester, char *subject, int *teacher_id);

// Update a subject by id
int subjects_update(int id, const char *program, const char *grade_level, const char *semester, const char *subject, int teacher_id);

// Delete a subject by id
int subjects_delete(int id);

#endif // SUBJECTS_H

