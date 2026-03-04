#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include "symbol.h"

/* Compile .f9s source to .exe without running.
 * Parses, performs semantic analysis, generates NASM x64, assembles, links.
 * Returns 0 on success, -1 on any error. */
int build_file(const char *filename, struct symbol_table *table);

/* Internal helper used by both build and load - compile to exe.
 * If exe_path_out is non-NULL, stores the output path.
 * Returns 0 on success, -1 on error. */
int compile_f9s(const char *filename, 
                struct symbol_table *table,
                char *exe_path_out,
                size_t exe_path_size);
