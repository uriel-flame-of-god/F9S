// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "codegen_helpers.h"
#include "types.h"

#define MAX_REALS   256
#define MAX_STRINGS 256
#define MAX_VARS    512

typedef struct { double value; int label_id; } real_entry;
typedef struct { char *value;  int label_id; } str_entry;

static real_entry s_reals[MAX_REALS];
static str_entry  s_strings[MAX_STRINGS];
static char      *s_vars[MAX_VARS];
static int        s_real_count   = 0;
static int        s_string_count = 0;
static int        s_var_count    = 0;

void cg_helpers_reset(void) {
    for (int i = 0; i < s_string_count; i++) {
        free(s_strings[i].value);
        s_strings[i].value = NULL;
    }
    for (int i = 0; i < s_var_count; i++) {
        free(s_vars[i]);
        s_vars[i] = NULL;
    }
    s_real_count   = 0;
    s_string_count = 0;
    s_var_count    = 0;
}

static int real_register(double v) {
    for (int i = 0; i < s_real_count; i++) {
        /* Bit-exact comparison (handles -0.0 vs 0.0, NaN, etc.) */
        uint64_t a, b;
        memcpy(&a, &s_reals[i].value, 8);
        memcpy(&b, &v, 8);
        if (a == b) return s_reals[i].label_id;
    }
    if (s_real_count >= MAX_REALS) return -1;
    int id = s_real_count;
    s_reals[id].value    = v;
    s_reals[id].label_id = id;
    s_real_count++;
    return id;
}

/* Recursive scan for REAL literals in an expression subtree */
static void scan_expr_reals(struct ast_node *n, FILE *out) {
    if (!n) return;
    if (n->type == AST_LITERAL && n->data.literal.type == TYPE_REAL) {
        double v = n->data.literal.value.real_val;
        uint64_t bits;
        memcpy(&bits, &v, 8);
        int prev = s_real_count;
        int id = real_register(v);
        if (id >= 0 && s_real_count != prev) {
            fprintf(out, "_rc%d dq 0x%016llx  ; %.17g\n",
                    id, (unsigned long long)bits, v);
        }
    }
    if (n->type == AST_BINARY_OP) {
        scan_expr_reals(n->data.binary.left,  out);
        scan_expr_reals(n->data.binary.right, out);
    }
}

int cg_emit_data_literals(struct ast_node *root, FILE *out) {
    if (!root || !out) return 0;
    struct ast_node *stmts = NULL;
    if (root->type == AST_PROGRAM)
        stmts = root->data.program.statements;
    else
        stmts = root;

    /* Re-scan always; avoid double-emission by re-checking registry */
    int prev_count = s_real_count;

    for (struct ast_node *s = stmts; s; s = s->next) {
        switch (s->type) {
        case AST_ASSIGN_EXPLICIT:
        case AST_ASSIGN_IMPLICIT:
            scan_expr_reals(s->data.assign.value, out);
            break;
        case AST_PRINT:
            scan_expr_reals(s->data.print.expr, out);
            break;
        case AST_IF:
            scan_expr_reals(s->data.if_stmt.condition, out);
            /* Recurse into bodies via temporary root nodes is complex;
             * use a helper that takes a statement list pointer */
            {
                struct ast_node *sub;
                for (sub = s->data.if_stmt.then_body; sub; sub = sub->next) {
                    if (sub->type == AST_ASSIGN_EXPLICIT ||
                        sub->type == AST_ASSIGN_IMPLICIT)
                        scan_expr_reals(sub->data.assign.value, out);
                    else if (sub->type == AST_PRINT)
                        scan_expr_reals(sub->data.print.expr, out);
                }
                for (sub = s->data.if_stmt.else_body; sub; sub = sub->next) {
                    if (sub->type == AST_ASSIGN_EXPLICIT ||
                        sub->type == AST_ASSIGN_IMPLICIT)
                        scan_expr_reals(sub->data.assign.value, out);
                    else if (sub->type == AST_PRINT)
                        scan_expr_reals(sub->data.print.expr, out);
                }
            }
            break;
        case AST_DO:
            scan_expr_reals(s->data.do_loop.start_expr, out);
            scan_expr_reals(s->data.do_loop.end_expr,   out);
            if (s->data.do_loop.step_expr)
                scan_expr_reals(s->data.do_loop.step_expr, out);
            {
                struct ast_node *sub;
                for (sub = s->data.do_loop.body; sub; sub = sub->next) {
                    if (sub->type == AST_ASSIGN_EXPLICIT ||
                        sub->type == AST_ASSIGN_IMPLICIT)
                        scan_expr_reals(sub->data.assign.value, out);
                    else if (sub->type == AST_PRINT)
                        scan_expr_reals(sub->data.print.expr, out);
                }
            }
            break;
        default:
            break;
        }
    }
    return s_real_count - prev_count;
}

int cg_real_label_id(double value) {
    uint64_t b;
    memcpy(&b, &value, 8);
    for (int i = 0; i < s_real_count; i++) {
        uint64_t a;
        memcpy(&a, &s_reals[i].value, 8);
        if (a == b) return s_reals[i].label_id;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  String literals                                                   */
/* ------------------------------------------------------------------ */

static int string_register(const char *v) {
    for (int i = 0; i < s_string_count; i++) {
        if (strcmp(s_strings[i].value, v) == 0)
            return s_strings[i].label_id;
    }
    if (s_string_count >= MAX_STRINGS) return -1;
    int id = s_string_count;
    s_strings[id].value    = strdup(v);
    s_strings[id].label_id = id;
    s_string_count++;
    return id;
}

static void emit_str_escaped(FILE *out, const char *s) {
    /* Emit a NASM db string with escaped non-printables */
    fprintf(out, "\"");
    while (*s) {
        if (*s == '"')       fprintf(out, "\", 34, \"");
        else if (*s == '\n') fprintf(out, "\", 10, \"");
        else if (*s == '\r') fprintf(out, "\", 13, \"");
        else                 fputc(*s, out);
        s++;
    }
    fprintf(out, "\"");
}

static void scan_expr_strings(struct ast_node *n, FILE *out) {
    if (!n) return;
    if (n->type == AST_LITERAL &&
        (n->data.literal.type == TYPE_STRING ||
         n->data.literal.type == TYPE_CHARACTER)) {
        const char *v = n->data.literal.value.string_val;
        if (!v) v = "";
        int prev = s_string_count;
        int id = string_register(v);
        if (id >= 0 && s_string_count != prev) {
            fprintf(out, "_str%d db ", id);
            emit_str_escaped(out, v);
            fprintf(out, ", 0\n");
        }
    }
    if (n->type == AST_BINARY_OP) {
        scan_expr_strings(n->data.binary.left,  out);
        scan_expr_strings(n->data.binary.right, out);
    }
}

int cg_emit_data_strings(struct ast_node *root, FILE *out) {
    if (!root || !out) return 0;
    struct ast_node *stmts = (root->type == AST_PROGRAM)
        ? root->data.program.statements : root;
    int prev = s_string_count;
    for (struct ast_node *s = stmts; s; s = s->next) {
        switch (s->type) {
        case AST_ASSIGN_EXPLICIT:
        case AST_ASSIGN_IMPLICIT:
            scan_expr_strings(s->data.assign.value, out);
            break;
        case AST_PRINT:
            scan_expr_strings(s->data.print.expr, out);
            break;
        case AST_IF: {
            scan_expr_strings(s->data.if_stmt.condition, out);
            struct ast_node *sub;
            for (sub = s->data.if_stmt.then_body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT ||
                    sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_strings(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_strings(sub->data.print.expr, out);
            }
            for (sub = s->data.if_stmt.else_body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT ||
                    sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_strings(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_strings(sub->data.print.expr, out);
            }
            break;
        }
        case AST_DO: {
            struct ast_node *sub;
            for (sub = s->data.do_loop.body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT ||
                    sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_strings(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_strings(sub->data.print.expr, out);
            }
            break;
        }
        default:
            break;
        }
    }
    return s_string_count - prev;
}

int cg_string_label_id(const char *value) {
    if (!value) value = "";
    for (int i = 0; i < s_string_count; i++) {
        if (strcmp(s_strings[i].value, value) == 0)
            return s_strings[i].label_id;
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  BSS variables                                                     */
/* ------------------------------------------------------------------ */

static int var_seen(const char *name) {
    for (int i = 0; i < s_var_count; i++) {
        if (strcmp(s_vars[i], name) == 0) return 1;
    }
    return 0;
}

/* Recursive helper — traverses statement list including IF/DO nesting */
static void cg_scan_stmts_bss(struct ast_node *stmts, FILE *out) {
    for (struct ast_node *s = stmts; s; s = s->next) {
        const char *name = NULL;
        if (s->type == AST_ASSIGN_EXPLICIT ||
            s->type == AST_ASSIGN_IMPLICIT)
            name = s->data.assign.var_name;

        if (name && !var_seen(name)) {
            fprintf(out, "var_%s resq 1\n", name);
            if (s_var_count < MAX_VARS)
                s_vars[s_var_count++] = strdup(name);
        }

        /* DO loop variable also needs a BSS slot */
        if (s->type == AST_DO) {
            const char *vn = s->data.do_loop.var_name;
            if (vn && !var_seen(vn)) {
                fprintf(out, "var_%s resq 1\n", vn);
                if (s_var_count < MAX_VARS)
                    s_vars[s_var_count++] = strdup(vn);
            }
            cg_scan_stmts_bss(s->data.do_loop.body, out);
        } else if (s->type == AST_IF) {
            cg_scan_stmts_bss(s->data.if_stmt.then_body, out);
            cg_scan_stmts_bss(s->data.if_stmt.else_body, out);
        }
    }
}

void cg_emit_bss_vars(struct ast_node *root, FILE *out) {
    if (!root || !out) return;
    struct ast_node *stmts = (root->type == AST_PROGRAM)
        ? root->data.program.statements : root;
    cg_scan_stmts_bss(stmts, out);
}
