#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include "types.h"

struct symbol {
    char            *name;          /* heap-allocated variable name          */
    enum type_kind   type;
    union {
        long long    int_value;
        double       real_value;
        char        *string_value;   /* heap-allocated                        */
        struct {
            double real;
            double imag;
        } complex_value;
    } value;
    struct symbol   *left;          /* AVL left child                         */
    struct symbol   *right;         /* AVL right child                        */
    int              height;        /* AVL node height (leaf = 1)              */
};

struct symbol_table {
    struct symbol *root;
    int            count;
};

struct symbol_table *symbol_table_create(void);

/* Recursively free every node, then free the table itself */
void symbol_table_destroy(struct symbol_table *table);

/* Insert a new variable or update an existing one.
   initial_value is interpreted according to type:
     TYPE_INT     → long long *
     TYPE_REAL    → double *
     TYPE_STRING / TYPE_CHARACTER → char * (nul-terminated)
     TYPE_LOGICAL → long long * (0 = FALSE, else TRUE)
     TYPE_COMPLEX → pointer to { double real; double imag; }
   Returns pointer to the symbol (stable pointer for ASM direct access).
   Returns NULL on allocation failure. */
struct symbol *symbol_insert(struct symbol_table *table,
                             const char          *name,
                             enum type_kind        type,
                             void                *initial_value);

/* Find a variable by name.
   Returns pointer to symbol or NULL if not found. */
struct symbol *symbol_lookup(struct symbol_table *table,
                             const char          *name);
