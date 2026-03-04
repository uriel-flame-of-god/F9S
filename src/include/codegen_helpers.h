#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include "ast.h"

/* Scan the entire AST for REAL literals.
 * For each unique value, emit a .data entry:
 *   _rcN dq 0x<bit-pattern>   ; <decimal value>
 * and register the mapping label-id → value.
 * Returns the number of real constants emitted. */
int cg_emit_data_literals(struct ast_node *root, FILE *out);

/* Look up the label id for a double value (set by cg_emit_data_literals).
 * Returns -1 if not found (should not happen after a proper first pass). */
int cg_real_label_id(double value);

/* ------------------------------------------------------------------ */
/*  BSS section                                                       */
/* ------------------------------------------------------------------ */

/* Walk the statement list and emit one "var_<name> resq 1" line per
 * unique variable name.  Safe to call multiple times (uses the symbol
 * table which de-duplicates). */
void cg_emit_bss_vars(struct ast_node *root, FILE *out);

/* ------------------------------------------------------------------ */
/*  String-literal registry                                           */
/*                                                                    */
/*  String literals get their own .data labels (_str0, _str1, …).    */
/* ------------------------------------------------------------------ */

/* Scan the AST for STRING literals, emit .data entries, and register
 * them.  Returns count emitted. */
int cg_emit_data_strings(struct ast_node *root, FILE *out);

/* Look up the label id for a string literal value.
 * Returns -1 if not found. */
int cg_string_label_id(const char *value);

/* ------------------------------------------------------------------ */
/*  Reset helper state (call before each file compilation)           */
/* ------------------------------------------------------------------ */
void cg_helpers_reset(void);
