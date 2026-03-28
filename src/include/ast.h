/**
 * @file ast.h
 * @brief Abstract Syntax Tree node definitions for F9S.
 *
 * Every parse construct maps to an `ast_node` tagged by `ast_node_type`.
 * Nodes are heap-allocated and linked as a singly-linked statement list
 * through the `next` pointer.  Expression trees use child pointers inside
 * the `data` union.
 *
 * Ownership: the AST is owned by whoever calls `parse_program`.
 * Release everything with `ast_destroy`.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "symbol.h"

/** @brief All possible statement and expression node kinds. */
enum ast_node_type {
    AST_PROGRAM,          /**< Root node: PROGRAM name ... END PROGRAM         */
    AST_ASSIGN_EXPLICIT,  /**< INTEGER x := 42  (type declared)                */
    AST_ASSIGN_IMPLICIT,  /**< ASSIGN x, 42  or  x = 42  (type inferred)       */
    AST_PRINT,            /**< PRINT *, expr  or implied-DO print              */
    AST_BINARY_OP,        /**< Arithmetic / comparison sub-expression          */
    AST_LITERAL,          /**< Integer, real, string, logical, complex literal */
    AST_VARIABLE,         /**< Variable reference (resolved in semantic pass)  */
    AST_IF,               /**< IF (cond) THEN ... [ELSE ...] END IF            */
    AST_DO,               /**< DO i = start, end [, step] ... END DO           */
    AST_STOP,             /**< STOP                                            */
    AST_ARRAY_DECL,       /**< INTEGER :: arr(10)                              */
    AST_ARRAY_ASSIGN,     /**< arr(i) = expr                                   */
    AST_ARRAY_REF,        /**< arr(i) as rvalue; also hashmap(key) lookup      */
    AST_HASHMAP_DECL,     /**< HASHMAP :: map                                  */
    AST_HASHMAP_INSERT,   /**< map(key) := value                               */
    AST_IMPLIED_DO        /**< (expr, var=start,end[,step]) inside PRINT       */
};

/** @brief Arithmetic and relational operators. */
enum binary_op {
    /* Arithmetic */
    OP_ADD,  /**< +  */
    OP_SUB,  /**< -  */
    OP_MUL,  /**< *  */
    OP_DIV,  /**< /  */
    /* Comparison — yield 0 (false) or 1 (true) in rax */
    OP_EQ,   /**< == */
    OP_NE,   /**< /= */
    OP_LT,   /**< <  */
    OP_GT,   /**< >  */
    OP_LE,   /**< <= */
    OP_GE    /**< >= */
};

/** @brief A single AST node. */
struct ast_node {
    enum ast_node_type  type;  /**< Discriminant for the `data` union.  */
    struct ast_node    *next;  /**< Next statement in a linked list.     */

    union {

        /** @brief AST_PROGRAM */
        struct {
            char            *name;        /**< Program name.              */
            struct ast_node *statements;  /**< Head of statement list.    */
        } program;

        /** @brief AST_ASSIGN_EXPLICIT and AST_ASSIGN_IMPLICIT */
        struct {
            char            *var_name;       /**< Target variable name.   */
            enum type_kind   declared_type;  /**< Declared type (explicit only; UNKNOWN for implicit). */
            struct ast_node *value;          /**< Value expression.        */
        } assign;

        /** @brief AST_PRINT */
        struct {
            struct ast_node *expr;  /**< Expression or AST_IMPLIED_DO.  */
        } print;

        /** @brief AST_BINARY_OP */
        struct {
            enum binary_op   op;
            struct ast_node *left;
            struct ast_node *right;
        } binary;

        /** @brief AST_LITERAL */
        struct {
            enum type_kind type;
            union {
                long long  int_val;     /**< TYPE_INT or TYPE_LOGICAL.   */
                double     real_val;    /**< TYPE_REAL.                  */
                char      *string_val;  /**< TYPE_STRING / TYPE_CHARACTER (heap-allocated). */
            } value;
        } literal;

        /** @brief AST_VARIABLE */
        struct {
            char          *name;    /**< Variable name.                  */
            struct symbol *symbol;  /**< Resolved in semantic analysis.  */
        } variable;

        /** @brief AST_IF */
        struct {
            struct ast_node *condition;  /**< Boolean expression.         */
            struct ast_node *then_body;  /**< Statement list (THEN).      */
            struct ast_node *else_body;  /**< Statement list (ELSE); NULL if absent. */
        } if_stmt;

        /** @brief AST_DO */
        struct {
            char            *var_name;    /**< Loop control variable.     */
            struct ast_node *start_expr;  /**< Initial value.             */
            struct ast_node *end_expr;    /**< Terminal value.            */
            struct ast_node *step_expr;   /**< Step; NULL means 1.        */
            struct ast_node *body;        /**< Loop body statement list.  */
        } do_loop;

        /* AST_STOP — no payload */

        /** @brief AST_ARRAY_DECL — e.g. `INTEGER :: arr(10)` */
        struct {
            char           *name;       /**< Array name.                 */
            enum type_kind  elem_type;  /**< Element type.               */
            int             size;       /**< Number of elements.         */
        } array_decl;

        /** @brief AST_ARRAY_ASSIGN — e.g. `arr(i) = expr` */
        struct {
            char            *name;        /**< Array name.               */
            struct ast_node *index_expr;  /**< 1-based index expression. */
            struct ast_node *value_expr;  /**< Value to store.           */
        } array_assign;

        /** @brief AST_ARRAY_REF — `arr(i)` or `map(key)` as rvalue */
        struct {
            char            *name;        /**< Array or hashmap name.    */
            struct ast_node *index_expr;  /**< Index or key expression.  */
        } array_ref;

        /** @brief AST_HASHMAP_DECL — `HASHMAP :: map` */
        struct {
            char *name;  /**< Hashmap name. */
        } hashmap_decl;

        /** @brief AST_HASHMAP_INSERT — `map(key) := value` */
        struct {
            char            *name;        /**< Hashmap name.             */
            struct ast_node *key_expr;    /**< Key expression.           */
            struct ast_node *value_expr;  /**< Value expression.         */
        } hashmap_insert;

        /** @brief AST_IMPLIED_DO — `(expr, var=start,end[,step])` */
        struct {
            struct ast_node *expr;        /**< Expression to evaluate/print per iteration. */
            char            *var_name;    /**< Loop control variable.    */
            struct ast_node *start_expr;  /**< Initial value.            */
            struct ast_node *end_expr;    /**< Terminal value.           */
            struct ast_node *step_expr;   /**< Step; NULL means 1.       */
        } implied_do;

    } data;
};

/**
 * @brief Allocate and zero-initialise a new AST node.
 * @param type Node kind.
 * @return Heap-allocated node, or NULL on allocation failure.
 */
struct ast_node *ast_node_alloc(enum ast_node_type type);

/**
 * @brief Recursively free an AST node and all its descendants.
 *
 * Also follows `next` pointers, so calling this on a statement-list head
 * frees the entire list.
 *
 * @param node Root node to free (NULL is safe).
 */
void ast_destroy(struct ast_node *node);
