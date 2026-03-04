#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include "symbol.h"

/* Handle ".load <file>" command from REPL.
 * Parses command, compiles .f9s to .exe, and runs it.
 * Returns: 0 = handled OK, -1 = not a .load command, -2 = error */
int handle_load_command(const char *line);

/* Handle ".build <file>" command from REPL.
 * Parses command, compiles .f9s to .exe without running.
 * Returns: 0 = handled OK, -1 = not a .build command, -2 = error */
int handle_build_command(const char *line);

/* Load and run: compile .f9s to .exe and execute it immediately.
 * Returns 0 on success, -2 on error. */
int load_file(const char *filename, struct symbol_table *table);
