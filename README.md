# Backend in C with SQLite

This backend is implemented in C using SQLite for database storage.

It supports basic HTTP REST API for user authentication, material and subject management.

## Build Instructions

Requires gcc and SQLite development libraries.

## API Endpoints

- Health check: GET /health
- User authentication: POST /login
- Materials CRUD: GET/POST/PUT/DELETE /materials
- Subjects CRUD: GET/POST/PUT/DELETE /subjects

More endpoints to be added.
