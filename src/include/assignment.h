#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include "symbol.h"
#include "error_handler.h"

/* Handle a typed declaration with an explicit initial value:
 *   INTEGER x := 42
 * Returns 0 on success, -1 on slap (fatal). */
int handle_explicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               enum type_kind        declared_type,
                               void                *value,
                               enum type_kind        value_type);

/* Handle an untyped assignment that infers the type:
 *   ASSIGN x, 42
 * Returns 0 on success, -1 on slap (fatal). */
int handle_implicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               void                *value,
                               enum type_kind        value_type);

/* Convert a value from src_type to dest_type.
 * dest must point to storage large enough for dest_type's representation.
 * pat_count is incremented for every lossy ("pat") conversion.
 * Returns 0 on success, -1 on slap (fatal, dest unchanged). */
int convert_value(void          *dest,
                  enum type_kind  dest_type,
                  const void    *src,
                  enum type_kind  src_type,
                  int            *pat_count);

/* Determine the FORTRAN.ASM type of a literal token
 * (delegates to the existing detect_type() in types.c). */
enum type_kind get_literal_type(const char *literal);
