# Assembled by the root Makefile via: $(MAKE) -C src/asm
# Can also be run standalone from the src/asm/ directory.

NASM    ?= nasm
OBJ_DIR ?= ../../obj

# Explicit source list to avoid collisions with C files
SOURCES := enable.asm int.asm main.asm control.asm
OBJECTS := $(patsubst %.asm,$(OBJ_DIR)/%.obj,$(SOURCES))

.PHONY: all

all: $(OBJECTS)

$(OBJ_DIR)/%.obj: %.asm
	"$(NASM)" -f win64 -o $@ $<
