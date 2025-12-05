@echo off
gcc -Wall -Wextra -std=c11 -I. -DNO_SSL -D_WIN32_WINNT=0x0600 sqlite-amalgamation-3460100/sqlite3.c civetweb.c main.c db.c auth.c materials.c subjects.c -o eknows_backend.exe -lmingw32 -lws2_32
if %errorlevel% neq 0 (
    echo Compilation failed
    pause
) else (
    echo Compilation successful
    pause
)
