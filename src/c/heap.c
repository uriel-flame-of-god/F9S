// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdlib.h>
#include <string.h>

#include "symbol.h"

static int avl_height(const struct symbol *n) {
    return n ? n->height : 0;
}

static int avl_balance_factor(const struct symbol *n) {
    return n ? avl_height(n->left) - avl_height(n->right) : 0;
}

static void avl_update_height(struct symbol *n) {
    if (!n) return;
    int lh = avl_height(n->left);
    int rh = avl_height(n->right);
    n->height = 1 + (lh > rh ? lh : rh);
}

/* Right rotation around y:
 *
 *       y              x
 *      / \            / \
 *     x   T3   =>   T1   y
 *    / \                / \
 *   T1  T2            T2   T3
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

/* Left rotation around x:
 *
 *     x                y
 *    / \              / \
 *   T1   y    =>    x    T3
 *       / \        / \
 *      T2  T3    T1   T2
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

/* Rebalance after an insert; returns the (possibly new) subtree root */
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

/* ------------------------------------------------------------------ */
/*  Copy a value into an existing symbol node.                        */
/*  Frees any previously held string so there are no leaks.          */
/* ------------------------------------------------------------------ */
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
        if (value) {
            sym->value.string_value = strdup((const char *)value);
        } else {
            sym->value.string_value = strdup("");
        }
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

/* ------------------------------------------------------------------ */
/*  Recursive insert; returns potentially new subtree root.          */
/*  *inserted_sym is set to the node with the given name.            */
/* ------------------------------------------------------------------ */
static struct symbol *avl_insert(struct symbol   *node,
                                 const char       *name,
                                 enum type_kind    type,
                                 void             *value,
                                 struct symbol   **inserted_sym) {
    if (!node) {
        /* Allocate a new leaf */
        struct symbol *s = calloc(1, sizeof(*s));
        if (!s) {
            *inserted_sym = NULL;
            return NULL;
        }
        s->name   = strdup(name);
        s->height = 1;
        s->left   = NULL;
        s->right  = NULL;
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
        /* Key already exists — update in place */
        symbol_set_value(node, type, value);
        *inserted_sym = node;
        return node;  /* no structural change, no rebalance needed */
    }

    return avl_rebalance(node);
}

/* ------------------------------------------------------------------ */
/*  Recursive free                                                    */
/* ------------------------------------------------------------------ */
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
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

struct symbol_table *symbol_table_create(void) {
    struct symbol_table *t = calloc(1, sizeof(*t));
    return t;
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

    if (inserted && was_new) {
        table->count++;
    }
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
