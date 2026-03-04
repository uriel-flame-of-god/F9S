#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include "symbol.h"
enum ast_node_type {
    AST_PROGRAM,
    AST_ASSIGN_EXPLICIT,   /* INTEGER x := 42                         */
    AST_ASSIGN_IMPLICIT,   /* ASSIGN x, 42                            */
    AST_PRINT,             /* PRINT x                                 */
    AST_BINARY_OP,
    AST_LITERAL,
    AST_VARIABLE,
    AST_IF,                /* IF (cond) THEN ... [ELSE ...] END IF    */
    AST_DO,                /* DO i = start, end [, step] ... END DO   */
    AST_STOP               /* STOP                                    */
};

enum binary_op {
    /* Arithmetic */
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    /* Comparison — result is 0 (false) or 1 (true) in rax */
    OP_EQ,   /* ==  */
    OP_NE,   /* /=  */
    OP_LT,   /* <   */
    OP_GT,   /* >   */
    OP_LE,   /* <=  */
    OP_GE    /* >=  */
};


struct ast_node {
    enum ast_node_type  type;
    struct ast_node    *next;   /* singly-linked list of statements */

    union {
        /* AST_PROGRAM */
        struct {
            char           *name;
            struct ast_node *statements;
        } program;

        /* AST_ASSIGN_EXPLICIT and AST_ASSIGN_IMPLICIT */
        struct {
            char           *var_name;
            enum type_kind  declared_type;  /* explicit only; UNKNOWN for implicit */
            struct ast_node *value;         /* expression subtree */
        } assign;

        /* AST_PRINT */
        struct {
            struct ast_node *expr;
        } print;

        /* AST_BINARY_OP */
        struct {
            enum binary_op   op;
            struct ast_node *left;
            struct ast_node *right;
        } binary;

        /* AST_LITERAL */
        struct {
            enum type_kind type;
            union {
                long long  int_val;
                double     real_val;
                char      *string_val;  /* heap-allocated */
            } value;
        } literal;

        /* AST_VARIABLE */
        struct {
            char          *name;
            struct symbol *symbol;   /* filled during semantic analysis */
        } variable;

        /* AST_IF */
        struct {
            struct ast_node *condition;
            struct ast_node *then_body;  /* linked list of statements     */
            struct ast_node *else_body;  /* NULL if no ELSE clause        */
        } if_stmt;

        /* AST_DO */
        struct {
            char            *var_name;
            struct ast_node *start_expr;
            struct ast_node *end_expr;
            struct ast_node *step_expr;  /* NULL → implicit step of 1    */
            struct ast_node *body;       /* linked list of statements     */
        } do_loop;

        /* AST_STOP — no extra fields */
    } data;
};

/* ------------------------------------------------------------------ */
/*  AST helpers (implemented in parser.c)                             */
/* ------------------------------------------------------------------ */
struct ast_node *ast_node_alloc(enum ast_node_type type);
void             ast_destroy(struct ast_node *node);
