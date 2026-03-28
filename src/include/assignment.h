/**
 * @file assignment.h
 * @brief Semantic-analysis assignment and type-conversion helpers.
 *
 * Handles the two assignment forms recognised by F9S:
 *  - **Explicit** (`INTEGER x := 42`) — declared type must be compatible.
 *  - **Implicit** (`ASSIGN x, 42` / `x = 42`) — type is inferred from value.
 *
 * Lossy conversions emit a `pat()` warning; incompatible conversions call
 * `slap()` and return -1.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "symbol.h"
#include "error_handler.h"

/**
 * @brief Process a typed declaration with an explicit initial value.
 *
 * Example: `INTEGER x := 42`
 *
 * The value is converted from `value_type` to `declared_type` if necessary.
 * A `pat()` warning is issued for lossy conversions (e.g. REAL → INTEGER).
 *
 * @param table         Target symbol table.
 * @param var_name      Variable name to insert or update.
 * @param declared_type Declared type kind.
 * @param value         Pointer to the initial value (type-erased).
 * @param value_type    Actual type of `*value`.
 * @return 0 on success, -1 if `slap()` was called.
 */
int handle_explicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               enum type_kind        declared_type,
                               void                *value,
                               enum type_kind        value_type);

/**
 * @brief Process an untyped assignment, inferring the type from the value.
 *
 * Example: `ASSIGN x, 42` or `x = 42`
 *
 * The variable is inserted (or updated) with the inferred type.
 *
 * @param table      Target symbol table.
 * @param var_name   Variable name to insert or update.
 * @param value      Pointer to the value (type-erased).
 * @param value_type Type kind of `*value`.
 * @return 0 on success, -1 if `slap()` was called.
 */
int handle_implicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               void                *value,
                               enum type_kind        value_type);

/**
 * @brief Convert a value from one type to another.
 *
 * `dest` must point to storage large enough for `dest_type`:
 *  - `TYPE_INT` / `TYPE_LOGICAL`  → `long long`
 *  - `TYPE_REAL`                  → `double`
 *  - `TYPE_STRING` / `TYPE_CHARACTER` → `char *` (NUL-terminated)
 *  - `TYPE_COMPLEX`               → `struct { double real; double imag; }`
 *
 * A `pat()` warning is issued for every lossy conversion;
 * `*pat_count` is incremented accordingly.
 *
 * @param dest       Output storage.
 * @param dest_type  Target type kind.
 * @param src        Source value (read-only).
 * @param src_type   Source type kind.
 * @param pat_count  Running warning counter (incremented on lossy convert).
 * @return 0 on success, -1 if conversion is impossible (`slap()` called).
 */
int convert_value(void          *dest,
                  enum type_kind  dest_type,
                  const void    *src,
                  enum type_kind  src_type,
                  int            *pat_count);

/**
 * @brief Infer the type of a raw literal token string.
 *
 * Delegates to `detect_type()` in `types.c`.
 *
 * @param literal Raw source token (e.g. `"3.14"`, `".TRUE."`, `"42"`).
 * @return Corresponding `type_kind` value.
 */
enum type_kind get_literal_type(const char *literal);
