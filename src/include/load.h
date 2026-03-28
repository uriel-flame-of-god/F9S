/**
 * @file load.h
 * @brief REPL `.load` and `.build` command handlers.
 *
 * These functions parse REPL dot-commands, drive the full compilation
 * pipeline, and (for `.load`) execute the resulting binary.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "symbol.h"

/**
 * @brief Handle a `.load <file>` command entered in the REPL.
 *
 * Parses the command, compiles the named `.f9s` file to `.exe`, and
 * executes it immediately.
 *
 * @param line Full REPL input line.
 * @return  0 if the command was handled successfully,
 *         -1 if `line` is not a `.load` command,
 *         -2 on compilation or execution error.
 */
int handle_load_command(const char *line);

/**
 * @brief Handle a `.build <file>` command entered in the REPL.
 *
 * Parses the command and compiles the named `.f9s` file to `.exe` without
 * executing it.
 *
 * @param line Full REPL input line.
 * @return  0 if the command was handled successfully,
 *         -1 if `line` is not a `.build` command,
 *         -2 on compilation error.
 */
int handle_build_command(const char *line);

/**
 * @brief Compile a `.f9s` source file to `.exe` and execute it.
 *
 * This is the backend invoked by `handle_load_command`.
 *
 * @param filename Path to the `.f9s` source file.
 * @param table    Symbol table (used during compilation).
 * @return 0 on success, -2 on error.
 */
int load_file(const char *filename, struct symbol_table *table);
