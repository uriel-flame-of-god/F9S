/**
 * @file parser.h
 * @brief Recursive-descent parser and semantic analyser for F9S.
 *
 * Exposes two public functions used by the compilation pipeline
 * (`build.c` / `load.c`):
 *
 *  1. `parse_program`      — tokenise + build AST
 *  2. `semantic_analysis`  — resolve symbols, type-check
 *
 * Both phases operate on in-memory source text; no re-reading is done.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "ast.h"
#include "symbol.h"

/**
 * @brief Parse a complete `.f9s` source file into an AST.
 *
 * The source must already be loaded into `*source_buffer` as a
 * NUL-terminated string.  `filename` is used only for error messages.
 *
 * @param filename      Path shown in diagnostics.
 * @param source_buffer Pointer to the source string pointer.
 * @return Root `AST_PROGRAM` node, or NULL on fatal parse error.
 */
struct ast_node *parse_program(const char *filename,
                               char      **source_buffer);

/**
 * @brief Resolve symbols and type-check a parsed AST.
 *
 * Fills `ast_node.variable.symbol` pointers by looking up each
 * identifier in `table`.  Must be called after `parse_program` and
 * before `generate_code`.
 *
 * @param ast   Root AST_PROGRAM node produced by `parse_program`.
 * @param table Empty (or pre-seeded) symbol table.
 * @return 0 on success, -1 if any fatal diagnostic was issued.
 */
int semantic_analysis(struct ast_node    *ast,
                      struct symbol_table *table);
