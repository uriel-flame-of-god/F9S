/**
 * @file build.h
 * @brief Compilation pipeline: parse → analyse → codegen → assemble → link.
 *
 * Exposes two entry points used by the REPL and the CLI:
 *  - `build_file`  — compile to `.exe` without executing.
 *  - `compile_f9s` — shared backend used by both build and load paths.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <stddef.h>
#include "symbol.h"

/**
 * @brief Compile a `.f9s` source file to a native `.exe` without running it.
 *
 * Runs the full pipeline: parse → semantic analysis → NASM x64 codegen →
 * `nasm -f win64` → `clang -m64`.
 *
 * @param filename Path to the `.f9s` source file.
 * @param table    Empty (or pre-seeded) symbol table.
 * @return 0 on success, -1 on any error.
 */
int build_file(const char *filename, struct symbol_table *table);

/**
 * @brief Internal compilation backend shared by `build_file` and `load_file`.
 *
 * Performs the same pipeline as `build_file`.  If `exe_path_out` is non-NULL,
 * the path to the produced `.exe` is written there on success.
 *
 * @param filename      Path to the `.f9s` source file.
 * @param table         Symbol table.
 * @param exe_path_out  Buffer to receive the output path, or NULL.
 * @param exe_path_size Size of `exe_path_out` in bytes.
 * @return 0 on success, -1 on error.
 */
int compile_f9s(const char *filename,
                struct symbol_table *table,
                char *exe_path_out,
                size_t exe_path_size);
