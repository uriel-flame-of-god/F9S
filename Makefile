# Detect tools
NASM ?= nasm
CC   := clang

OBJ_DIR := obj
BIN_DIR := bin
TARGET  := $(BIN_DIR)/program.exe

# Absolute path to obj/ so sub-Makefiles can use it regardless of their cwd
ABS_OBJ := $(abspath $(OBJ_DIR))

.PHONY: all clean dirs

all: dirs
	$(MAKE) --no-print-directory -C src/asm OBJ_DIR=$(ABS_OBJ) NASM=$(NASM)
	$(MAKE) --no-print-directory -C src/c   OBJ_DIR=$(ABS_OBJ) CC=$(CC)
	"$(CC)" -m64 -o $(TARGET) $(OBJ_DIR)/*.obj

dirs:
	@if not exist $(OBJ_DIR) mkdir $(OBJ_DIR)
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)

clean:
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)
	@if exist *.exe del /q *.exe
	@if exist *.obj del /q *.obj
	@if exist examples\*.exe del /q examples\*.exe
	@if exist examples\*.obj del /q examples\*.obj