/**
 * @file codegen_helpers.h
 * @brief Code-generation support: data/BSS emission and registry queries.
 *
 * Provides two classes of functionality:
 *  - **Emission helpers** – scan the AST and write `.data` / `.bss`
 *    sections into an open NASM output file.
 *  - **Registry queries** – answer "is this name an array/hashmap?" and
 *    retrieve element types, used by the main code-generation pass.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <stdio.h>
#include "ast.h"

/**
 * @brief Scan the AST for REAL literals and emit `.data` entries.
 *
 * For each unique value, emits:
 * @code
 *   _rcN  dq  0x<bit-pattern>   ; <decimal value>
 * @endcode
 * and registers the label-id → value mapping for later lookup.
 *
 * @param root Root AST node to scan.
 * @param out  Open file to write NASM `.data` lines to.
 * @return Number of real-constant labels emitted.
 */
int cg_emit_data_literals(struct ast_node *root, FILE *out);

/**
 * @brief Look up the label index for a previously registered double.
 *
 * @param value Double value to look up.
 * @return Label index (≥ 0), or -1 if not registered.
 */
int cg_real_label_id(double value);

/**
 * @brief Walk the statement list and emit `var_<name> resq 1` BSS entries.
 *
 * De-duplicates via the internal registry, so calling this multiple times
 * for the same name is safe.
 *
 * @param root Root AST node to scan.
 * @param out  Open file to write NASM `.bss` lines to.
 */
void cg_emit_bss_vars(struct ast_node *root, FILE *out);

/**
 * @brief Scan the AST for STRING literals and emit `.data` entries.
 *
 * Emits `_str0`, `_str1`, … labels and registers them.
 *
 * @param root Root AST node to scan.
 * @param out  Open file to write NASM `.data` lines to.
 * @return Number of string labels emitted.
 */
int cg_emit_data_strings(struct ast_node *root, FILE *out);

/**
 * @brief Look up the label index for a previously registered string literal.
 *
 * @param value NUL-terminated string value to look up.
 * @return Label index (≥ 0), or -1 if not registered.
 */
int cg_string_label_id(const char *value);

/**
 * @brief Register an array declaration in the internal registry.
 *
 * Called during the BSS scan when an `AST_ARRAY_DECL` node is encountered.
 *
 * @param name      Array name (copied).
 * @param size      Number of elements.
 * @param elem_type Element type kind.
 */
void cg_register_array(const char *name, int size, enum type_kind elem_type);

/**
 * @brief Register a hashmap declaration in the internal registry.
 *
 * Called during the BSS scan when an `AST_HASHMAP_DECL` node is found.
 *
 * @param name Hashmap name (copied).
 */
void cg_register_hashmap(const char *name);

/**
 * @brief Query whether a name refers to a declared array.
 *
 * @param name Name to test.
 * @return 1 if the name is a registered array, 0 otherwise.
 */
int cg_is_array(const char *name);

/**
 * @brief Query whether a name refers to a declared hashmap.
 *
 * @param name Name to test.
 * @return 1 if the name is a registered hashmap, 0 otherwise.
 */
int cg_is_hashmap(const char *name);

/**
 * @brief Get the element type of a registered array.
 *
 * @param name Registered array name.
 * @return `type_kind` of the elements, or `TYPE_INT` if not found.
 */
enum type_kind cg_array_elem_type(const char *name);

/**
 * @brief Return the number of hashmaps declared in the current compilation unit.
 *
 * Used to decide whether the hashmap runtime helpers need to be emitted.
 *
 * @return Count of registered hashmaps.
 */
int cg_hashmap_count(void);

/**
 * @brief Emit the hashmap runtime helper routines into the output file.
 *
 * Writes `_f9s_hm_insert` and `_f9s_hm_lookup` as NASM procedures.
 * Must be called after `section .text` but before `_main` if any hashmap
 * was declared.
 *
 * @param out Open file to write NASM code to.
 */
void cg_emit_hashmap_runtime(FILE *out);

/**
 * @brief Reset all internal codegen-helper state.
 *
 * Must be called before each file compilation to avoid stale registry entries
 * from a previous run.
 */
void cg_helpers_reset(void);
