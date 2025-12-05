CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I.
LDFLAGS = -lsqlite3

SRC = civetweb.c main.c db.c auth.c materials.c subjects.c
OBJ = $(SRC:.c=.o)
TARGET = eknows_backend

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
