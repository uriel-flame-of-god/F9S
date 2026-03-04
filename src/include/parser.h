#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include "ast.h"
#include "symbol.h"

/* Parse a complete .f9s file already loaded into *source_buffer.
 * filename is used for error messages only.
 * Returns the root AST_PROGRAM node, or NULL on fatal parse error. */
struct ast_node *parse_program(const char *filename,
                               char      **source_buffer);

/* Resolve symbols and type-check the parsed AST.
 * Fills ast_node.variable.symbol pointers.
 * Returns 0 on success, -1 if any slap was issued. */
int semantic_analysis(struct ast_node    *ast,
                      struct symbol_table *table);
