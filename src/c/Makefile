# Compiled by the root Makefile via: $(MAKE) -C src/c
# Can also be run standalone from the src/c/ directory.

CC      ?= clang
OBJ_DIR ?= ../../obj
INC_DIR := ../include

SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.obj,$(SOURCES))

.PHONY: all

all: $(OBJECTS)

$(OBJ_DIR)/%.obj: %.c
	"$(CC)" -c -m64 -I$(INC_DIR) -o $@ $<
