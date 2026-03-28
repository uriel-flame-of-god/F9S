/**
 * @file heap.c
 * @brief AVL-tree symbol table implementation.
 *
 * Stores compiler variables as nodes in a self-balancing BST keyed by name.
 * All four AVL rotations (LL, LR, RL, RR) are handled by `avl_rebalance()`.
 * Lookup is O(log n); insert is O(log n) amortised.
 *
 * String values are heap-allocated on insert and freed on update/destroy
 * to prevent memory leaks.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdlib.h>
#include <string.h>

#include "symbol.h"

/** @brief Return the AVL height of a node (0 for NULL). */
static int avl_height(const struct symbol *n) {
    return n ? n->height : 0;
}

/** @brief Return the balance factor of a node (left_height - right_height). */
static int avl_balance_factor(const struct symbol *n) {
    return n ? avl_height(n->left) - avl_height(n->right) : 0;
}

/** @brief Recompute the stored height of a node from its children. */
static void avl_update_height(struct symbol *n) {
    if (!n) return;
    int lh = avl_height(n->left);
    int rh = avl_height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

/**
 * @brief Right rotation around `y`.
 *
 * @code
 *       y              x
 *      / \            / \
 *     x   T3   =>   T1   y
 *    / \                / \
 *   T1  T2            T2   T3
 * @endcode
 *
 * @param y Subtree root to rotate.
 * @return New subtree root (`x`).
 */
static struct symbol *avl_rotate_right(struct symbol *y) {
    struct symbol *x  = y->left;
    struct symbol *T2 = x->right;

    x->right = y;
    y->left  = T2;

    avl_update_height(y);
    avl_update_height(x);
    return x;
}

/**
 * @brief Left rotation around `x`.
 *
 * @code
 *     x                y
 *    / \              / \
 *   T1   y    =>    x    T3
 *       / \        / \
 *      T2  T3    T1   T2
 * @endcode
 *
 * @param x Subtree root to rotate.
 * @return New subtree root (`y`).
 */
static struct symbol *avl_rotate_left(struct symbol *x) {
    struct symbol *y  = x->right;
    struct symbol *T2 = y->left;

    y->left  = x;
    x->right = T2;

    avl_update_height(x);
    avl_update_height(y);
    return y;
}

/**
 * @brief Rebalance a subtree after an insertion.
 *
 * Handles all four imbalance cases (LL, LR, RL, RR).
 *
 * @param node Subtree root to rebalance.
 * @return Potentially new subtree root.
 */
static struct symbol *avl_rebalance(struct symbol *node) {
    avl_update_height(node);

    int bf = avl_balance_factor(node);

    /* Left-heavy */
    if (bf > 1) {
        if (avl_balance_factor(node->left) < 0)
            node->left = avl_rotate_left(node->left);  /* LR case */
        return avl_rotate_right(node);
    }

    /* Right-heavy */
    if (bf < -1) {
        if (avl_balance_factor(node->right) > 0)
            node->right = avl_rotate_right(node->right); /* RL case */
        return avl_rotate_left(node);
    }

    return node;  /* already balanced */
}

/**
 * @brief Copy a value into an existing symbol node.
 *
 * Frees any previously held string to prevent leaks before overwriting.
 *
 * @param sym   Target symbol node.
 * @param type  New type kind.
 * @param value Pointer to value data, or NULL for zero-initialisation.
 */
static void symbol_set_value(struct symbol *sym,
                             enum type_kind  type,
                             void           *value) {
    /* Release old string if needed */
    if ((sym->type == TYPE_STRING || sym->type == TYPE_CHARACTER)
            && sym->value.string_value) {
        free(sym->value.string_value);
        sym->value.string_value = NULL;
    }

    sym->type = type;

    switch (type) {
    case TYPE_INT:
        sym->value.int_value = value ? *(long long *)value : 0LL;
        break;
    case TYPE_REAL:
        sym->value.real_value = value ? *(double *)value : 0.0;
        break;
    case TYPE_STRING:
    case TYPE_CHARACTER:
        sym->value.string_value = strdup(value ? (const char *)value : "");
        break;
    case TYPE_LOGICAL:
        sym->value.int_value = value ? (*(long long *)value ? 1LL : 0LL) : 0LL;
        break;
    case TYPE_COMPLEX: {
        struct { double real; double imag; } *cv =
            (struct { double real; double imag; } *)value;
        if (cv) {
            sym->value.complex_value.real = cv->real;
            sym->value.complex_value.imag = cv->imag;
        } else {
            sym->value.complex_value.real = 0.0;
            sym->value.complex_value.imag = 0.0;
        }
        break;
    }
    default:
        sym->value.int_value = 0LL;
        break;
    }
}

/**
 * @brief Recursive AVL insert.
 *
 * On duplicate keys the existing node is updated in place (no rebalance).
 * `*inserted_sym` is set to the node carrying `name` on return.
 *
 * @param node         Current subtree root (may be NULL).
 * @param name         Key to insert.
 * @param type         Type kind for the new or updated value.
 * @param value        Pointer to value data, or NULL.
 * @param inserted_sym Receives pointer to the inserted/updated node.
 * @return Potentially new subtree root.
 */
static struct symbol *avl_insert(struct symbol   *node,
                                 const char       *name,
                                 enum type_kind    type,
                                 void             *value,
                                 struct symbol   **inserted_sym) {
    if (!node) {
        struct symbol *s = calloc(1, sizeof(*s));
        if (!s) {
            *inserted_sym = NULL;
            return NULL;
        }
        s->name   = strdup(name);
        s->height = 1;
        symbol_set_value(s, type, value);
        *inserted_sym = s;
        return s;
    }

    int cmp = strcmp(name, node->name);

    if (cmp < 0) {
        node->left  = avl_insert(node->left,  name, type, value, inserted_sym);
    } else if (cmp > 0) {
        node->right = avl_insert(node->right, name, type, value, inserted_sym);
    } else {
        /* Key already exists — update in place, no structural change */
        symbol_set_value(node, type, value);
        *inserted_sym = node;
        return node;
    }

    return avl_rebalance(node);
}

/**
 * @brief Recursively free an AVL subtree, including heap-allocated strings.
 *
 * @param node Subtree root to free (NULL is safe).
 */
static void avl_free(struct symbol *node) {
    if (!node) return;
    avl_free(node->left);
    avl_free(node->right);
    free(node->name);
    if ((node->type == TYPE_STRING || node->type == TYPE_CHARACTER)
            && node->value.string_value) {
        free(node->value.string_value);
    }
    free(node);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

struct symbol_table *symbol_table_create(void) {
    return calloc(1, sizeof(struct symbol_table));
}

void symbol_table_destroy(struct symbol_table *table) {
    if (!table) return;
    avl_free(table->root);
    free(table);
}

struct symbol *symbol_insert(struct symbol_table *table,
                             const char           *name,
                             enum type_kind         type,
                             void                 *initial_value) {
    if (!table || !name) return NULL;

    struct symbol *inserted = NULL;
    int was_new = (symbol_lookup(table, name) == NULL);

    table->root = avl_insert(table->root, name, type, initial_value, &inserted);

    if (inserted && was_new)
        table->count++;

    return inserted;
}

struct symbol *symbol_lookup(struct symbol_table *table,
                             const char           *name) {
    if (!table || !name) return NULL;

    struct symbol *cur = table->root;
    while (cur) {
        int cmp = strcmp(name, cur->name);
        if      (cmp < 0) cur = cur->left;
        else if (cmp > 0) cur = cur->right;
        else              return cur;
    }
    return NULL;
}
