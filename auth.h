#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>

#include <stddef.h>

// Initialize authentication module, e.g., create default admin user if needed
int auth_init(void);

// Handle login request: returns 1 on success, 0 on failure
int auth_handle_login(const char *username, const char *password);

// Get user ID by username
int auth_get_user_id(const char *username);

// Token management
void auth_generate_token(char *token, size_t len);
int auth_validate_token(const char *token);
void auth_store_token(const char *token, int user_id);

#endif // AUTH_H
