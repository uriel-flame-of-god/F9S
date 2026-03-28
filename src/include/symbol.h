/**
 * @file symbol.h
 * @brief AVL-tree symbol table for F9S.
 *
 * Each variable in a compiled program gets one `symbol` node stored in a
 * balanced BST keyed by name.  Lookup is O(log n).
 *
 * Only the compiler's semantic/codegen passes use this table.  The generated
 * `.asm` file uses its own flat BSS labels and does not call back into this
 * data structure at runtime.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "types.h"

/** @brief A single symbol-table entry (AVL tree node). */
struct symbol {
    char            *name;    /**< Heap-allocated variable name.         */
    enum type_kind   type;    /**< Declared or inferred type.            */

    /** @brief Stored value (used by the interpreter, not codegen). */
    union {
        long long    int_value;      /**< TYPE_INT / TYPE_LOGICAL.       */
        double       real_value;     /**< TYPE_REAL.                     */
        char        *string_value;   /**< TYPE_STRING / TYPE_CHARACTER (heap-allocated). */
        struct {
            double real;
            double imag;
        } complex_value;             /**< TYPE_COMPLEX.                  */
    } value;

    struct symbol   *left;    /**< AVL left child.                       */
    struct symbol   *right;   /**< AVL right child.                      */
    int              height;  /**< AVL node height (leaf = 1).           */
};

/** @brief Root container for the AVL symbol table. */
struct symbol_table {
    struct symbol *root;   /**< Tree root, or NULL if empty.  */
    int            count;  /**< Number of symbols stored.     */
};

/**
 * @brief Allocate an empty symbol table.
 * @return Heap-allocated table, or NULL on failure.
 */
struct symbol_table *symbol_table_create(void);

/**
 * @brief Recursively free every node, then free the table itself.
 * @param table Table to destroy (NULL is safe).
 */
void symbol_table_destroy(struct symbol_table *table);

/**
 * @brief Insert or update a variable.
 *
 * `initial_value` is interpreted according to `type`:
 *   - TYPE_INT / TYPE_LOGICAL → `long long *`
 *   - TYPE_REAL               → `double *`
 *   - TYPE_STRING / TYPE_CHARACTER → `char *` (NUL-terminated)
 *   - TYPE_COMPLEX            → `struct { double real; double imag; } *`
 *   - NULL                    → zero-initialised
 *
 * @param table         Target table.
 * @param name          Variable name (copied).
 * @param type          Type kind.
 * @param initial_value Pointer to initial value, or NULL.
 * @return Stable pointer to the symbol node, or NULL on allocation failure.
 */
struct symbol *symbol_insert(struct symbol_table *table,
                             const char          *name,
                             enum type_kind        type,
                             void                *initial_value);

/**
 * @brief Look up a variable by name.
 * @param table Source table.
 * @param name  Variable name to find.
 * @return Pointer to symbol, or NULL if not found.
 */
struct symbol *symbol_lookup(struct symbol_table *table,
                             const char          *name);
