#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "db.h"
#include "auth.h"
#include "materials.h"
#include "subjects.h"

#include "civetweb.h"

#define PORT "8080"
#define BUFFER_SIZE 4096

static struct mg_context *ctx = NULL;

// Helper function to validate token from Authorization header
static int validate_token_from_header(struct mg_connection *conn) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    const char *auth_header = mg_get_header(conn, "Authorization");
    if (!auth_header || strncmp(auth_header, "Bearer ", 7) != 0) {
        return -1; // No token or invalid format
    }
    const char *token = auth_header + 7; // Skip "Bearer "
    return auth_validate_token(token);
}

// Send HTTP response with status code, content type, and body including CORS
static void send_response(struct mg_connection *conn, int status_code,
                          const char *content_type, const char *body) {
    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Access-Control-Allow-Origin: *\r\n"
              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
              "Access-Control-Allow-Headers: Content-Type\r\n"
              "Content-Type: %s\r\n"
              "Content-Length: %lu\r\n"
              "Connection: close\r\n"
              "\r\n"
              "%s",
              status_code,
              (status_code == 200) ? "OK" :
              (status_code == 400) ? "Bad Request" :
              (status_code == 401) ? "Unauthorized" :
              (status_code == 404) ? "Not Found" : "Error",
              content_type, (unsigned long)strlen(body), body);
}

// Handler for /health GET endpoint
static int handle_health(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "text/plain", "Method Not Allowed");
        return 405;
    }
    send_response(conn, 200, "text/plain", "OK");
    return 200;
}

// Handler for /login POST endpoint - expects form data username=...&password=...
static int handle_login(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char username[100] = {0};
    char password[100] = {0};
    int res = mg_get_var(post_data, post_data_len, "username", username, sizeof(username));
    if (res <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing username\"}");
        return 400;
    }
    res = mg_get_var(post_data, post_data_len, "password", password, sizeof(password));
    if (res <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing password\"}");
        return 400;
    }

    int success = auth_handle_login(username, password);
    if (success) {
        send_response(conn, 200, "application/json", "{\"message\": \"Login successful\"}");
    } else {
        send_response(conn, 401, "application/json", "{\"message\": \"Invalid credentials\"}");
    }
    return success ? 200 : 401;
}

// Simple JSON credentials parser - expects {"username":"...","password":"..."}
static int parse_json_credentials(const char *json, char *username, char *password) {
    const char *u_key = "\"username\":\"";
    const char *p_key = "\"password\":\"";
    const char *u_start = strstr(json, u_key);
    const char *p_start = strstr(json, p_key);
    if (!u_start || !p_start) return 0;
    u_start += strlen(u_key);
    p_start += strlen(p_key);
    const char *u_end = strchr(u_start, '"');
    const char *p_end = strchr(p_start, '"');
    if (!u_end || !p_end) return 0;
    int u_len = (int)(u_end - u_start);
    int p_len = (int)(p_end - p_start);
    if (u_len >= 100 || p_len >= 100) return 0;
    strncpy(username, u_start, u_len);
    username[u_len] = '\0';
    strncpy(password, p_start, p_len);
    password[p_len] = '\0';
    return 1;
}

// Helper function to parse subject JSON - expects {"program":"...","grade_level":"...","semester":"...","subject":"...","teacher_id":...}
static int parse_subject_json(const char *json, char *program, char *grade_level, char *semester, char *subject, int *teacher_id) {
    const char *prog_key = "\"program\":\"";
    const char *prog_start = strstr(json, prog_key);
    if (prog_start) {
        prog_start += strlen(prog_key);
        const char *prog_end = strchr(prog_start, '"');
        if (prog_end) {
            int len = prog_end - prog_start;
            if (len < 100) {
                strncpy(program, prog_start, len);
                program[len] = '\0';
            }
        }
    }

    const char *gl_key = "\"grade_level\":\"";
    const char *gl_start = strstr(json, gl_key);
    if (gl_start) {
        gl_start += strlen(gl_key);
        const char *gl_end = strchr(gl_start, '"');
        if (gl_end) {
            int len = gl_end - gl_start;
            if (len < 50) {
                strncpy(grade_level, gl_start, len);
                grade_level[len] = '\0';
            }
        }
    }

    const char *sem_key = "\"semester\":\"";
    const char *sem_start = strstr(json, sem_key);
    if (sem_start) {
        sem_start += strlen(sem_key);
        const char *sem_end = strchr(sem_start, '"');
        if (sem_end) {
            int len = sem_end - sem_start;
            if (len < 50) {
                strncpy(semester, sem_start, len);
                semester[len] = '\0';
            }
        }
    }

    const char *sub_key = "\"subject\":\"";
    const char *sub_start = strstr(json, sub_key);
    if (sub_start) {
        sub_start += strlen(sub_key);
        const char *sub_end = strchr(sub_start, '"');
        if (sub_end) {
            int len = sub_end - sub_start;
            if (len < 100) {
                strncpy(subject, sub_start, len);
                subject[len] = '\0';
            }
        }
    }

    const char *tid_key = "\"teacher_id\":";
    const char *tid_start = strstr(json, tid_key);
    if (tid_start) {
        tid_start += strlen(tid_key);
        *teacher_id = atoi(tid_start);
    }

    return (program[0] && grade_level[0] && semester[0] && subject[0] && *teacher_id != 0) ? 1 : 0;
}

// Helper function to parse delete JSON - expects {"id":...}
static int parse_delete_json(const char *json, int *id) {
    const char *id_key = "\"id\":";
    const char *id_start = strstr(json, id_key);
    if (!id_start) return 0;
    id_start += strlen(id_key);
    *id = atoi(id_start);
    return (*id != 0) ? 1 : 0;
}

// Helper function to parse assign JSON - expects {"subject_id":...,"teacher_id":...}
static int parse_assign_json(const char *json, int *subject_id, int *teacher_id) {
    const char *sid_key = "\"subject_id\":";
    const char *sid_start = strstr(json, sid_key);
    if (sid_start) {
        sid_start += strlen(sid_key);
        *subject_id = atoi(sid_start);
    }

    const char *tid_key = "\"teacher_id\":";
    const char *tid_start = strstr(json, tid_key);
    if (tid_start) {
        tid_start += strlen(tid_key);
        *teacher_id = atoi(tid_start);
    }

    return (*subject_id != 0 && *teacher_id != 0) ? 1 : 0;
}

// Handler for /api/admin/login POST with JSON body
static int handle_api_admin_login(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char username[100] = {0};
    char password[100] = {0};
    if (!parse_json_credentials(post_data, username, password)) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return 400;
    }

    int login_result = auth_handle_login(username, password);
    if (login_result == 1) {
        int user_id = auth_get_user_id(username);
        char token[65];
        auth_generate_token(token, sizeof(token));
        auth_store_token(token, user_id);
        char response[256];
        sprintf(response, "{\"success\":true,\"id\":%d,\"token\":\"%s\",\"name\":\"Admin\",\"redirect\":\"./admin_panel.html\"}", user_id, token);
        send_response(conn, 200, "application/json", response);
        return 200;
    } else if (login_result == -2) {
        send_response(conn, 423, "application/json", "{\"success\":false,\"message\":\"Account locked due to too many failed attempts\"}");
        return 423;
    } else {
        send_response(conn, 401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
        return 401;
    }
}

// Handler for /api/teacher/login POST with JSON body
static int handle_api_teacher_login(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char username[100] = {0};
    char password[100] = {0};
    if (!parse_json_credentials(post_data, username, password)) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return 400;
    }

    int login_result = auth_handle_login(username, password);
    if (login_result == 1) {
        int user_id = auth_get_user_id(username);
        char token[65];
        auth_generate_token(token, sizeof(token));
        auth_store_token(token, user_id);
        char response[256];
        sprintf(response, "{\"success\":true,\"token\":\"%s\",\"name\":\"Teacher\",\"redirect\":\"./teacher_panel.html\"}", token);
        send_response(conn, 200, "application/json", response);
        return 200;
    } else if (login_result == -2) {
        send_response(conn, 423, "application/json", "{\"success\":false,\"message\":\"Account locked due to too many failed attempts\"}");
        return 423;
    } else {
        send_response(conn, 401, "application/json", "{\"success\":false,\"message\":\"Invalid credentials\"}");
        return 401;
    }
}

// Handler for /api/teacher/dashboard-data POST with JSON body
static int handle_api_teacher_dashboard_data(struct mg_connection *conn, void *cbdata) {
    int user_id = validate_token_from_header(conn);
    if (user_id == -1) {
        send_response(conn, 401, "application/json", "{\"message\":\"Unauthorized\"}");
        return 401;
    }

    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    // Parse teacher_id from JSON
    const char *tid_key = "\"teacher_id\":";
    const char *tid_start = strstr(post_data, tid_key);
    if (!tid_start) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing teacher_id\"}");
        return 400;
    }
    tid_start += strlen(tid_key);
    int teacher_id = atoi(tid_start);

    char *json = db_get_dashboard_data_json(teacher_id);
    if (json) {
        send_response(conn, 200, "application/json", json);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}



// Handler for /upload-material POST with JSON body
static int handle_upload_material(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    // Parse JSON: {"subject_id":1,"category":"...","file_name":"...","file_base64":"..."}
    int subject_id = 0;
    char category[100] = {0};
    char file_name[256] = {0};
    char file_base64[BUFFER_SIZE] = {0};

    // Simple parsing
    const char *sid_key = "\"subject_id\":";
    const char *sid_start = strstr(post_data, sid_key);
    if (sid_start) {
        sid_start += strlen(sid_key);
        subject_id = atoi(sid_start);
    }

    const char *cat_key = "\"category\":\"";
    const char *cat_start = strstr(post_data, cat_key);
    if (cat_start) {
        cat_start += strlen(cat_key);
        const char *cat_end = strchr(cat_start, '"');
        if (cat_end) {
            int len = cat_end - cat_start;
            if (len < sizeof(category)) {
                strncpy(category, cat_start, len);
                category[len] = '\0';
            }
        }
    }

    const char *fn_key = "\"file_name\":\"";
    const char *fn_start = strstr(post_data, fn_key);
    if (fn_start) {
        fn_start += strlen(fn_key);
        const char *fn_end = strchr(fn_start, '"');
        if (fn_end) {
            int len = fn_end - fn_start;
            if (len < sizeof(file_name)) {
                strncpy(file_name, fn_start, len);
                file_name[len] = '\0';
            }
        }
    }

    const char *fb_key = "\"file_base64\":\"";
    const char *fb_start = strstr(post_data, fb_key);
    if (fb_start) {
        fb_start += strlen(fb_key);
        const char *fb_end = strchr(fb_start, '"');
        if (fb_end) {
            int len = fb_end - fb_start;
            if (len < sizeof(file_base64)) {
                strncpy(file_base64, fb_start, len);
                file_base64[len] = '\0';
            }
        }
    }

    if (subject_id == 0 || !category[0] || !file_name[0] || !file_base64[0]) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        return 400;
    }

    int rc = materials_create(subject_id, category, file_name, file_base64);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Material uploaded\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /delete-material POST with JSON body
static int handle_delete_material(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    const char *id_key = "\"id\":";
    const char *id_start = strstr(post_data, id_key);
    if (!id_start) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing id\"}");
        return 400;
    }
    id_start += strlen(id_key);
    int id = atoi(id_start);

    int rc = materials_delete(id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"message\":\"Deleted\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /download GET with id query
static int handle_download_material(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "text/plain", "Method Not Allowed");
        return 405;
    }

    char id_str[32] = {0};
    mg_get_var(req_info->query_string, strlen(req_info->query_string), "id", id_str, sizeof(id_str));
    if (!id_str[0]) {
        send_response(conn, 400, "text/plain", "Missing id");
        return 400;
    }
    int id = atoi(id_str);

    int subject_id;
    char category[100];
    char original_filename[256];
    char file_data[BUFFER_SIZE];
    int rc = materials_read(id, &subject_id, category, original_filename, file_data);
    if (rc != SQLITE_OK) {
        send_response(conn, 404, "text/plain", "Not Found");
        return 404;
    }

    // Assume base64, but for simplicity, send as is. In real, decode if needed.
    mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "Content-Disposition: attachment; filename=\"%s\"\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    original_filename, strlen(file_data), file_data);
    return 200;
}

// Handler for /get-subjects GET with teacher_id query
static int handle_get_subjects(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }

    char teacher_id_str[32] = {0};
    mg_get_var(req_info->query_string, strlen(req_info->query_string), "teacher_id", teacher_id_str, sizeof(teacher_id_str));
    if (!teacher_id_str[0]) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing teacher_id\"}");
        return 400;
    }
    int teacher_id = atoi(teacher_id_str);

    char *json = db_get_subjects_by_teacher_json(teacher_id);
    if (json) {
        size_t json_len = strlen(json);
        size_t response_len = json_len + 13; // strlen("{\"subjects\":}") + 1 for null
        char *response = malloc(response_len);
        if (!response) {
            send_response(conn, 500, "application/json", "{\"message\":\"Memory error\"}");
            free(json);
            return 500;
        }
        sprintf(response, "{\"subjects\":%s}", json);
        send_response(conn, 200, "application/json", response);
        free(response);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /create-subject POST with JSON body
static int handle_create_subject(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char program[100] = {0};
    char grade_level[50] = {0};
    char semester[50] = {0};
    char subject[100] = {0};
    int teacher_id = 0;

    if (!parse_subject_json(post_data, program, grade_level, semester, subject, &teacher_id)) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        return 400;
    }

    int rc = subjects_create(program, grade_level, semester, subject, teacher_id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Subject created\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /update-subject POST with JSON body
static int handle_update_subject(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    int id = 0;
    if (!parse_delete_json(post_data, &id)) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing id\"}");
        return 400;
    }

    char program[100] = {0};
    char grade_level[50] = {0};
    char semester[50] = {0};
    char subject[100] = {0};
    int teacher_id = 0;
    if (!parse_subject_json(post_data, program, grade_level, semester, subject, &teacher_id)) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        return 400;
    }

    if (id == 0 || !program[0] || !grade_level[0] || !semester[0] || !subject[0] || teacher_id == 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        return 400;
    }

    int rc = subjects_update(id, program, grade_level, semester, subject, teacher_id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Subject updated\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /delete-subject POST with JSON body
static int handle_delete_subject(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    const char *id_key = "\"id\":";
    const char *id_start = strstr(post_data, id_key);
    if (!id_start) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing id\"}");
        return 400;
    }
    id_start += strlen(id_key);
    int id = atoi(id_start);

    int rc = subjects_delete(id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"message\":\"Deleted\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /assign-subject POST with JSON body
static int handle_assign_subject(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    int subject_id = 0;
    int teacher_id = 0;

    const char *sid_key = "\"subject_id\":";
    const char *sid_start = strstr(post_data, sid_key);
    if (sid_start) {
        sid_start += strlen(sid_key);
        subject_id = atoi(sid_start);
    }

    const char *tid_key = "\"teacher_id\":";
    const char *tid_start = strstr(post_data, tid_key);
    if (tid_start) {
        tid_start += strlen(tid_key);
        teacher_id = atoi(tid_start);
    }

    if (subject_id == 0 || teacher_id == 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing ids\"}");
        return 400;
    }

    int rc = db_assign_subject_to_teacher(subject_id, teacher_id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Assigned\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}







// Handler for /get-materials GET endpoint - expects query teacher_id
static int handle_get_materials(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char teacher_id_str[32] = {0};
    mg_get_var(req_info->query_string, strlen(req_info->query_string), "teacher_id", teacher_id_str, sizeof(teacher_id_str));
    int teacher_id = atoi(teacher_id_str);
    if (teacher_id <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing or invalid teacher_id\"}");
        return 400;
    }

    char *json = db_get_materials_by_teacher_json(teacher_id);
    if (json) {
        send_response(conn, 200, "application/json", json);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /api/teacher/get-subjects GET endpoint - expects query teacher_id
static int handle_api_teacher_get_subjects(struct mg_connection *conn, void *cbdata) {
    int user_id = validate_token_from_header(conn);
    if (user_id == -1) {
        send_response(conn, 401, "application/json", "{\"message\":\"Unauthorized\"}");
        return 401;
    }

    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char teacher_id_str[32] = {0};
    mg_get_var(req_info->query_string, strlen(req_info->query_string), "teacher_id", teacher_id_str, sizeof(teacher_id_str));
    int teacher_id = atoi(teacher_id_str);
    if (teacher_id <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing or invalid teacher_id\"}");
        return 400;
    }

    char *json = db_get_subjects_by_teacher_json(teacher_id);
    if (json) {
        size_t json_len = strlen(json);
        size_t response_len = json_len + 13; // strlen("{\"subjects\":}") + 1 for null
        char *response = malloc(response_len);
        if (!response) {
            send_response(conn, 500, "application/json", "{\"message\":\"Memory error\"}");
            free(json);
            return 500;
        }
        sprintf(response, "{\"subjects\":%s}", json);
        send_response(conn, 200, "application/json", response);
        free(response);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /api/teacher/add-subject POST endpoint - expects JSON {program, grade_level, semester, subject, teacher_id}
static int handle_api_teacher_add_subject(struct mg_connection *conn, void *cbdata) {
    int user_id = validate_token_from_header(conn);
    if (user_id == -1) {
        send_response(conn, 401, "application/json", "{\"message\":\"Unauthorized\"}");
        return 401;
    }

    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char program[100] = {0};
    char grade_level[50] = {0};
    char semester[50] = {0};
    char subject[100] = {0};
    int teacher_id = 0;

    // Simple parsing
    const char *prog_key = "\"program\":\"";
    const char *prog_start = strstr(post_data, prog_key);
    if (prog_start) {
        prog_start += strlen(prog_key);
        const char *prog_end = strchr(prog_start, '"');
        if (prog_end) {
            int len = prog_end - prog_start;
            if (len < sizeof(program)) {
                strncpy(program, prog_start, len);
                program[len] = '\0';
            }
        }
    }

    const char *gl_key = "\"grade_level\":\"";
    const char *gl_start = strstr(post_data, gl_key);
    if (gl_start) {
        gl_start += strlen(gl_key);
        const char *gl_end = strchr(gl_start, '"');
        if (gl_end) {
            int len = gl_end - gl_start;
            if (len < sizeof(grade_level)) {
                strncpy(grade_level, gl_start, len);
                grade_level[len] = '\0';
            }
        }
    }

    const char *sem_key = "\"semester\":\"";
    const char *sem_start = strstr(post_data, sem_key);
    if (sem_start) {
        sem_start += strlen(sem_key);
        const char *sem_end = strchr(sem_start, '"');
        if (sem_end) {
            int len = sem_end - sem_start;
            if (len < sizeof(semester)) {
                strncpy(semester, sem_start, len);
                semester[len] = '\0';
            }
        }
    }

    const char *sub_key = "\"subject\":\"";
    const char *sub_start = strstr(post_data, sub_key);
    if (sub_start) {
        sub_start += strlen(sub_key);
        const char *sub_end = strchr(sub_start, '"');
        if (sub_end) {
            int len = sub_end - sub_start;
            if (len < sizeof(subject)) {
                strncpy(subject, sub_start, len);
                subject[len] = '\0';
            }
        }
    }

    const char *tid_key = "\"teacher_id\":";
    const char *tid_start = strstr(post_data, tid_key);
    if (tid_start) {
        tid_start += strlen(tid_key);
        teacher_id = atoi(tid_start);
    }

    if (!program[0] || !grade_level[0] || !semester[0] || !subject[0] || teacher_id == 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        return 400;
    }

    int rc = subjects_create(program, grade_level, semester, subject, teacher_id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Subject added\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /api/teacher/delete-subject POST endpoint - expects JSON {id}
static int handle_api_teacher_delete_subject(struct mg_connection *conn, void *cbdata) {
    int user_id = validate_token_from_header(conn);
    if (user_id == -1) {
        send_response(conn, 401, "application/json", "{\"message\":\"Unauthorized\"}");
        return 401;
    }

    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    const char *id_key = "\"id\":";
    const char *id_start = strstr(post_data, id_key);
    if (!id_start) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing id\"}");
        return 400;
    }
    id_start += strlen(id_key);
    int id = atoi(id_start);

    int rc = subjects_delete(id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"message\":\"Deleted\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /api/admin/get-subjects GET endpoint - returns all subjects
static int handle_api_admin_get_subjects(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }

    char *json = db_get_all_subjects_json();
    if (json) {
        size_t json_len = strlen(json);
        size_t response_len = json_len + 13; // strlen("{\"subjects\":}") + 1 for null
        char *response = malloc(response_len);
        if (!response) {
            send_response(conn, 500, "application/json", "{\"message\":\"Memory error\"}");
            free(json);
            return 500;
        }
        sprintf(response, "{\"subjects\":%s}", json);
        send_response(conn, 200, "application/json", response);
        free(response);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /api/teacher/assign-subject POST endpoint - expects JSON {subject_id, teacher_id}
static int handle_api_teacher_assign_subject(struct mg_connection *conn, void *cbdata) {
    int user_id = validate_token_from_header(conn);
    if (user_id == -1) {
        send_response(conn, 401, "application/json", "{\"message\":\"Unauthorized\"}");
        return 401;
    }

    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    int subject_id = 0;
    int teacher_id = 0;

    const char *sid_key = "\"subject_id\":";
    const char *sid_start = strstr(post_data, sid_key);
    if (sid_start) {
        sid_start += strlen(sid_key);
        subject_id = atoi(sid_start);
    }

    const char *tid_key = "\"teacher_id\":";
    const char *tid_start = strstr(post_data, tid_key);
    if (tid_start) {
        tid_start += strlen(tid_key);
        teacher_id = atoi(tid_start);
    }

    if (subject_id == 0 || teacher_id == 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing ids\"}");
        return 400;
    }

    int rc = db_assign_subject_to_teacher(subject_id, teacher_id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Assigned\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /api/admin/get-programs GET endpoint
static int handle_api_admin_get_programs(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }

    char *json = db_get_all_programs_json();
    if (json) {
        char response[4096];
        sprintf(response, "{\"programs\":%s}", json);
        send_response(conn, 200, "application/json", response);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /api/admin/get-teachers GET endpoint
static int handle_api_admin_get_teachers(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }

    char *json = db_get_all_teachers_json();
    if (json) {
        char response[4096];
        sprintf(response, "{\"teachers\":%s}", json);
        send_response(conn, 200, "application/json", response);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /api/admin/get-tracking-data GET endpoint
static int handle_api_admin_get_tracking_data(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "GET") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }

    char *json = db_get_tracking_data_json();
    if (json) {
        char response[4096];
        sprintf(response, "{\"tracking_data\":%s}", json);
        send_response(conn, 200, "application/json", response);
        free(json);
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return 200;
}

// Handler for /api/admin/add-program POST endpoint - expects JSON {name, subjects_json}
static int handle_api_admin_add_program(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char name[100] = {0};
    char subjects_json[BUFFER_SIZE] = {0};

    const char *name_key = "\"name\":\"";
    const char *name_start = strstr(post_data, name_key);
    if (name_start) {
        name_start += strlen(name_key);
        const char *name_end = strchr(name_start, '"');
        if (name_end) {
            int len = name_end - name_start;
            if (len < sizeof(name)) {
                strncpy(name, name_start, len);
                name[len] = '\0';
            }
        }
    }

    const char *subjects_key = "\"subjects_json\":\"";
    const char *subjects_start = strstr(post_data, subjects_key);
    if (subjects_start) {
        subjects_start += strlen(subjects_key);
        const char *subjects_end = strchr(subjects_start, '"');
        if (subjects_end) {
            int len = subjects_end - subjects_start;
            if (len < sizeof(subjects_json)) {
                strncpy(subjects_json, subjects_start, len);
                subjects_json[len] = '\0';
            }
        }
    }

    if (!name[0]) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing name\"}");
        return 400;
    }

    int rc = db_create_program(name, subjects_json);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Program added\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /api/admin/delete-program POST endpoint - expects JSON {id}
static int handle_api_admin_delete_program(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    int id = 0;
    if (!parse_delete_json(post_data, &id)) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing id\"}");
        return 400;
    }

    int rc = db_delete_program(id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"message\":\"Deleted\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /api/admin/add-teacher POST endpoint - expects JSON {name, username, password, access_code}
static int handle_api_admin_add_teacher(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"success\":false,\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    char name[100] = {0};
    char username[100] = {0};
    char password[100] = {0};
    char access_code[100] = {0};

    const char *name_key = "\"name\":\"";
    const char *name_start = strstr(post_data, name_key);
    if (name_start) {
        name_start += strlen(name_key);
        const char *name_end = strchr(name_start, '"');
        if (name_end) {
            int len = name_end - name_start;
            if (len < sizeof(name)) {
                strncpy(name, name_start, len);
                name[len] = '\0';
            }
        }
    }

    const char *username_key = "\"username\":\"";
    const char *username_start = strstr(post_data, username_key);
    if (username_start) {
        username_start += strlen(username_key);
        const char *username_end = strchr(username_start, '"');
        if (username_end) {
            int len = username_end - username_start;
            if (len < sizeof(username)) {
                strncpy(username, username_start, len);
                username[len] = '\0';
            }
        }
    }

    const char *password_key = "\"password\":\"";
    const char *password_start = strstr(post_data, password_key);
    if (password_start) {
        password_start += strlen(password_key);
        const char *password_end = strchr(password_start, '"');
        if (password_end) {
            int len = password_end - password_start;
            if (len < sizeof(password)) {
                strncpy(password, password_start, len);
                password[len] = '\0';
            }
        }
    }

    const char *access_code_key = "\"access_code\":\"";
    const char *access_code_start = strstr(post_data, access_code_key);
    if (access_code_start) {
        access_code_start += strlen(access_code_key);
        const char *access_code_end = strchr(access_code_start, '"');
        if (access_code_end) {
            int len = access_code_end - access_code_start;
            if (len < sizeof(access_code)) {
                strncpy(access_code, access_code_start, len);
                access_code[len] = '\0';
            }
        }
    }

    if (!name[0] || !username[0] || !password[0] || !access_code[0]) {
        send_response(conn, 400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
        return 400;
    }

    int rc = db_create_teacher(name, username, password, access_code);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"success\":true,\"message\":\"Teacher added\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"success\":false,\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Handler for /api/admin/delete-teacher POST endpoint - expects JSON {id}
static int handle_api_admin_delete_teacher(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    if (strcmp(req_info->request_method, "POST") != 0) {
        send_response(conn, 405, "application/json", "{\"message\":\"Method Not Allowed\"}");
        return 405;
    }
    char post_data[BUFFER_SIZE] = {0};
    int post_data_len = mg_read(conn, post_data, sizeof(post_data) - 1);
    if (post_data_len <= 0) {
        send_response(conn, 400, "application/json", "{\"message\":\"No body data\"}");
        return 400;
    }
    post_data[post_data_len] = '\0';

    int id = 0;
    if (!parse_delete_json(post_data, &id)) {
        send_response(conn, 400, "application/json", "{\"message\":\"Missing id\"}");
        return 400;
    }

    int rc = db_delete_teacher(id);
    if (rc == SQLITE_OK) {
        send_response(conn, 200, "application/json", "{\"message\":\"Deleted\"}");
    } else {
        send_response(conn, 500, "application/json", "{\"message\":\"Database error\"}");
    }
    return rc == SQLITE_OK ? 200 : 500;
}

// Placeholder handlers for materials and subjects endpoints
static int handle_materials(struct mg_connection *conn, void *cbdata) {
    // TODO: Implement CRUD operations based on request method
    send_response(conn, 200, "application/json", "{\"message\": \"Materials endpoint (not implemented yet)\"}");
    return 200;
}

static int handle_subjects(struct mg_connection *conn, void *cbdata) {
    // TODO: Implement CRUD operations based on request method
    send_response(conn, 200, "application/json", "{\"message\": \"Subjects endpoint (not implemented yet)\"}");
    return 200;
}

int main() {
    if (db_init("eknows.db") != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    if (auth_init() != 0) {
        fprintf(stderr, "Failed to initialize authentication module\n");
        db_close();
        return 1;
    }

    const char *options[] = {
        "listening_ports", "127.0.0.1:8080",
        "document_root", "../frontend",
        "request_timeout_ms", "5000",
        NULL
    };

    ctx = mg_start(NULL, NULL, options);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to start CivetWeb server\n");
        db_close();
        return 1;
    }

    mg_set_request_handler(ctx, "/health", handle_health, NULL);
    mg_set_request_handler(ctx, "/login", handle_login, NULL);
    mg_set_request_handler(ctx, "/api/admin/login", handle_api_admin_login, NULL);
    mg_set_request_handler(ctx, "/api/teacher/login", handle_api_teacher_login, NULL);
    mg_set_request_handler(ctx, "/api/teacher/dashboard-data", handle_api_teacher_dashboard_data, NULL);
    mg_set_request_handler(ctx, "/api/teacher/get-subjects", handle_api_teacher_get_subjects, NULL);
    mg_set_request_handler(ctx, "/api/teacher/add-subject", handle_api_teacher_add_subject, NULL);
    mg_set_request_handler(ctx, "/api/teacher/delete-subject", handle_api_teacher_delete_subject, NULL);
    mg_set_request_handler(ctx, "/api/admin/get-subjects", handle_api_admin_get_subjects, NULL);
    mg_set_request_handler(ctx, "/api/teacher/assign-subject", handle_api_teacher_assign_subject, NULL);
    mg_set_request_handler(ctx, "/api/admin/get-programs", handle_api_admin_get_programs, NULL);
    mg_set_request_handler(ctx, "/api/admin/get-teachers", handle_api_admin_get_teachers, NULL);
    mg_set_request_handler(ctx, "/api/admin/get-tracking-data", handle_api_admin_get_tracking_data, NULL);
    mg_set_request_handler(ctx, "/api/admin/add-program", handle_api_admin_add_program, NULL);
    mg_set_request_handler(ctx, "/api/admin/delete-program", handle_api_admin_delete_program, NULL);
    mg_set_request_handler(ctx, "/api/admin/add-teacher", handle_api_admin_add_teacher, NULL);
    mg_set_request_handler(ctx, "/api/admin/delete-teacher", handle_api_admin_delete_teacher, NULL);
    mg_set_request_handler(ctx, "/get-materials", handle_get_materials, NULL);
    mg_set_request_handler(ctx, "/upload-material", handle_upload_material, NULL);
    mg_set_request_handler(ctx, "/delete-material", handle_delete_material, NULL);
    mg_set_request_handler(ctx, "/download", handle_download_material, NULL);
    mg_set_request_handler(ctx, "/get-subjects", handle_get_subjects, NULL);
    mg_set_request_handler(ctx, "/create-subject", handle_create_subject, NULL);
    mg_set_request_handler(ctx, "/update-subject", handle_update_subject, NULL);
    mg_set_request_handler(ctx, "/delete-subject", handle_delete_subject, NULL);
    mg_set_request_handler(ctx, "/assign-subject", handle_assign_subject, NULL);

    printf("Server running on port %s\n", PORT);
    printf("Server is running. Press Ctrl+C to stop.\n");

    // Keep the server running indefinitely
    while (1) {
        // Sleep for a short time to avoid busy waiting
        #ifdef _WIN32
        Sleep(1000); // Windows sleep in milliseconds
        #else
        sleep(1); // Unix sleep in seconds
        #endif
    }

    mg_stop(ctx);
    db_close();

    return 0;
}
