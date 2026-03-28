/**
 * @file calc.h
 * @brief REPL expression evaluator (integer calculator).
 *
 * Provides a simple arithmetic expression evaluator used by the interactive
 * REPL for quick calculations without compiling a full `.f9s` program.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

/**
 * @brief Check whether a REPL input string is an exit command.
 *
 * Recognised exit strings: `"exit"`, `"quit"`, `".exit"`, `".quit"`.
 *
 * @param s Input line (NUL-terminated).
 * @return 1 if the string is an exit command, 0 otherwise.
 */
int is_exit(const char *s);

/**
 * @brief Evaluate a single arithmetic expression line.
 *
 * Parses and evaluates `line` as an integer arithmetic expression.
 * The `features` bitmask controls which extensions are enabled (reserved
 * for future use; pass 0 for default behaviour).
 *
 * @param line     NUL-terminated expression string.
 * @param features Feature flags (currently unused; pass 0).
 * @param out      Receives the integer result on success.
 * @return 0 on success, -1 on parse/evaluation error.
 */
int compute(const char *line, int features, long long *out);
