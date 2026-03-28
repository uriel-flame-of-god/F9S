/**
 * @file codegen_helpers.c
 * @brief Code-generation helper implementations.
 *
 * Three registries are maintained as flat static arrays:
 *  - **Real constants** — keyed by bit-exact double value, emitted as `_rcN`.
 *  - **String literals** — keyed by content, emitted as `_strN`.
 *  - **BSS variables**  — scalars (`var_<name> resq 1`), arrays
 *    (`arr_<name> resq <size>`), and hashmaps (`hm_<name> resb 2048`).
 *
 * The hashmap runtime (`_f9s_hm_insert` / `_f9s_hm_lookup`) uses a
 * 64-slot open-addressing scheme with 32-byte slots:
 * `key(8), value(8), used(8), padding(8)`.
 */

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

#define MAX_REALS    256
#define MAX_STRINGS  256
#define MAX_VARS     512
#define MAX_ARRAYS    64
#define MAX_HASHMAPS  32

/** @brief Entry in the real-constant registry. */
typedef struct { double value; int label_id; } real_entry;

/** @brief Entry in the string-literal registry. */
typedef struct { char *value; int label_id; } str_entry;

/** @brief Entry in the array registry. */
typedef struct { char *name; int size; enum type_kind elem_type; } array_entry_t;

/** @brief Entry in the hashmap registry. */
typedef struct { char *name; } hmap_entry_t;

static real_entry    s_reals[MAX_REALS];
static str_entry     s_strings[MAX_STRINGS];
static char         *s_vars[MAX_VARS];
static array_entry_t s_arrays[MAX_ARRAYS];
static hmap_entry_t  s_hashmaps[MAX_HASHMAPS];
static int           s_real_count   = 0;
static int           s_string_count = 0;
static int           s_var_count    = 0;
static int           s_array_count  = 0;
static int           s_hmap_count   = 0;

void cg_helpers_reset(void) {
    for (int i = 0; i < s_string_count; i++) { free(s_strings[i].value);  s_strings[i].value = NULL; }
    for (int i = 0; i < s_var_count;    i++) { free(s_vars[i]);            s_vars[i]           = NULL; }
    for (int i = 0; i < s_array_count;  i++) { free(s_arrays[i].name);    s_arrays[i].name    = NULL; }
    for (int i = 0; i < s_hmap_count;   i++) { free(s_hashmaps[i].name);  s_hashmaps[i].name  = NULL; }
    s_real_count = s_string_count = s_var_count = s_array_count = s_hmap_count = 0;
}

/* ------------------------------------------------------------------ */
/*  Array and hashmap registries                                       */
/* ------------------------------------------------------------------ */

void cg_register_array(const char *name, int size, enum type_kind elem_type) {
    for (int i = 0; i < s_array_count; i++)
        if (strcmp(s_arrays[i].name, name) == 0) return;
    if (s_array_count >= MAX_ARRAYS) return;
    s_arrays[s_array_count].name      = strdup(name);
    s_arrays[s_array_count].size      = size;
    s_arrays[s_array_count].elem_type = elem_type;
    s_array_count++;
}

void cg_register_hashmap(const char *name) {
    for (int i = 0; i < s_hmap_count; i++)
        if (strcmp(s_hashmaps[i].name, name) == 0) return;
    if (s_hmap_count >= MAX_HASHMAPS) return;
    s_hashmaps[s_hmap_count].name = strdup(name);
    s_hmap_count++;
}

int cg_is_array(const char *name) {
    for (int i = 0; i < s_array_count; i++)
        if (strcmp(s_arrays[i].name, name) == 0) return 1;
    return 0;
}

int cg_is_hashmap(const char *name) {
    for (int i = 0; i < s_hmap_count; i++)
        if (strcmp(s_hashmaps[i].name, name) == 0) return 1;
    return 0;
}

enum type_kind cg_array_elem_type(const char *name) {
    for (int i = 0; i < s_array_count; i++)
        if (strcmp(s_arrays[i].name, name) == 0) return s_arrays[i].elem_type;
    return TYPE_INT;
}

int cg_hashmap_count(void) { return s_hmap_count; }

/* ------------------------------------------------------------------ */
/*  Hashmap runtime helpers (emitted once per translation unit)        */
/* ------------------------------------------------------------------ */

void cg_emit_hashmap_runtime(FILE *out) {
    fprintf(out,
        "; _f9s_hm_insert: rcx=table, rdx=key (int64), r8=value (int64)\n"
        "; 64-slot open-addressing hash, slot=32 bytes: key(8),value(8),used(8),pad(8)\n"
        "_f9s_hm_insert:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    sub rsp, 32\n"
        "    mov r10, rdx\n"
        "    and r10, 63\n"
        "    xor r11, r11\n"
        ".hmi_probe:\n"
        "    mov rax, r10\n"
        "    shl rax, 5\n"
        "    mov r9, qword [rcx + rax + 16]\n"
        "    test r9, r9\n"
        "    jz .hmi_use\n"
        "    cmp qword [rcx + rax], rdx\n"
        "    je .hmi_use\n"
        "    inc r10\n"
        "    and r10, 63\n"
        "    inc r11\n"
        "    cmp r11, 64\n"
        "    jge .hmi_done\n"
        "    jmp .hmi_probe\n"
        ".hmi_use:\n"
        "    mov rax, r10\n"
        "    shl rax, 5\n"
        "    mov qword [rcx + rax], rdx\n"
        "    mov qword [rcx + rax + 8], r8\n"
        "    mov qword [rcx + rax + 16], 1\n"
        ".hmi_done:\n"
        "    add rsp, 32\n"
        "    pop rbp\n"
        "    ret\n"
        "\n"
        "; _f9s_hm_lookup: rcx=table, rdx=key (int64) -> rax=value (0 if not found)\n"
        "_f9s_hm_lookup:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    sub rsp, 32\n"
        "    mov r10, rdx\n"
        "    and r10, 63\n"
        "    xor r11, r11\n"
        ".hml_probe:\n"
        "    mov rax, r10\n"
        "    shl rax, 5\n"
        "    mov r9, qword [rcx + rax + 16]\n"
        "    test r9, r9\n"
        "    jz .hml_notfound\n"
        "    cmp qword [rcx + rax], rdx\n"
        "    je .hml_found\n"
        "    inc r10\n"
        "    and r10, 63\n"
        "    inc r11\n"
        "    cmp r11, 64\n"
        "    jge .hml_notfound\n"
        "    jmp .hml_probe\n"
        ".hml_found:\n"
        "    mov rax, r10\n"
        "    shl rax, 5\n"
        "    mov rax, qword [rcx + rax + 8]\n"
        "    jmp .hml_done\n"
        ".hml_notfound:\n"
        "    xor rax, rax\n"
        ".hml_done:\n"
        "    add rsp, 32\n"
        "    pop rbp\n"
        "    ret\n"
        "\n"
    );
}

/* ------------------------------------------------------------------ */
/*  Real literals                                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Register a double value, returning its label index.
 *
 * Uses bit-exact comparison so that -0.0, NaN, and Inf are distinct.
 *
 * @param v Value to register.
 * @return Label index, or -1 if the registry is full.
 */
static int real_register(double v) {
    uint64_t b;
    memcpy(&b, &v, 8);
    for (int i = 0; i < s_real_count; i++) {
        uint64_t a;
        memcpy(&a, &s_reals[i].value, 8);
        if (a == b) return s_reals[i].label_id;
    }
    if (s_real_count >= MAX_REALS) return -1;
    int id = s_real_count;
    s_reals[id].value    = v;
    s_reals[id].label_id = id;
    s_real_count++;
    return id;
}

/**
 * @brief Recursively scan an expression subtree for REAL literals.
 *
 * @param n   Root expression node.
 * @param out Output file for `.data` entries.
 */
static void scan_expr_reals(struct ast_node *n, FILE *out) {
    if (!n) return;
    if (n->type == AST_LITERAL && n->data.literal.type == TYPE_REAL) {
        double v = n->data.literal.value.real_val;
        uint64_t bits;
        memcpy(&bits, &v, 8);
        int prev = s_real_count;
        int id = real_register(v);
        if (id >= 0 && s_real_count != prev)
            fprintf(out, "_rc%d dq 0x%016llx  ; %.17g\n", id, (unsigned long long)bits, v);
    }
    if (n->type == AST_BINARY_OP) {
        scan_expr_reals(n->data.binary.left,  out);
        scan_expr_reals(n->data.binary.right, out);
    }
    if (n->type == AST_ARRAY_ASSIGN) {
        scan_expr_reals(n->data.array_assign.index_expr, out);
        scan_expr_reals(n->data.array_assign.value_expr, out);
    }
    if (n->type == AST_ARRAY_REF)
        scan_expr_reals(n->data.array_ref.index_expr, out);
    if (n->type == AST_HASHMAP_INSERT) {
        scan_expr_reals(n->data.hashmap_insert.key_expr,   out);
        scan_expr_reals(n->data.hashmap_insert.value_expr, out);
    }
    if (n->type == AST_IMPLIED_DO) {
        scan_expr_reals(n->data.implied_do.expr,       out);
        scan_expr_reals(n->data.implied_do.start_expr, out);
        scan_expr_reals(n->data.implied_do.end_expr,   out);
        if (n->data.implied_do.step_expr)
            scan_expr_reals(n->data.implied_do.step_expr, out);
    }
}

int cg_emit_data_literals(struct ast_node *root, FILE *out) {
    if (!root || !out) return 0;
    struct ast_node *stmts = (root->type == AST_PROGRAM)
        ? root->data.program.statements : root;
    int prev = s_real_count;
    for (struct ast_node *s = stmts; s; s = s->next) {
        switch (s->type) {
        case AST_ASSIGN_EXPLICIT:
        case AST_ASSIGN_IMPLICIT:
            scan_expr_reals(s->data.assign.value, out);
            break;
        case AST_PRINT:
            scan_expr_reals(s->data.print.expr, out);
            break;
        case AST_ARRAY_ASSIGN:
            scan_expr_reals(s->data.array_assign.index_expr, out);
            scan_expr_reals(s->data.array_assign.value_expr, out);
            break;
        case AST_HASHMAP_INSERT:
            scan_expr_reals(s->data.hashmap_insert.key_expr,   out);
            scan_expr_reals(s->data.hashmap_insert.value_expr, out);
            break;
        case AST_IF: {
            scan_expr_reals(s->data.if_stmt.condition, out);
            struct ast_node *sub;
            for (sub = s->data.if_stmt.then_body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT || sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_reals(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_reals(sub->data.print.expr, out);
                else if (sub->type == AST_ARRAY_ASSIGN) {
                    scan_expr_reals(sub->data.array_assign.index_expr, out);
                    scan_expr_reals(sub->data.array_assign.value_expr, out);
                }
            }
            for (sub = s->data.if_stmt.else_body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT || sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_reals(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_reals(sub->data.print.expr, out);
                else if (sub->type == AST_ARRAY_ASSIGN) {
                    scan_expr_reals(sub->data.array_assign.index_expr, out);
                    scan_expr_reals(sub->data.array_assign.value_expr, out);
                }
            }
            break;
        }
        case AST_DO:
            scan_expr_reals(s->data.do_loop.start_expr, out);
            scan_expr_reals(s->data.do_loop.end_expr,   out);
            if (s->data.do_loop.step_expr)
                scan_expr_reals(s->data.do_loop.step_expr, out);
            {
                struct ast_node *sub;
                for (sub = s->data.do_loop.body; sub; sub = sub->next) {
                    if (sub->type == AST_ASSIGN_EXPLICIT || sub->type == AST_ASSIGN_IMPLICIT)
                        scan_expr_reals(sub->data.assign.value, out);
                    else if (sub->type == AST_PRINT)
                        scan_expr_reals(sub->data.print.expr, out);
                    else if (sub->type == AST_ARRAY_ASSIGN) {
                        scan_expr_reals(sub->data.array_assign.index_expr, out);
                        scan_expr_reals(sub->data.array_assign.value_expr, out);
                    }
                }
            }
            break;
        default:
            break;
        }
    }
    return s_real_count - prev;
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
/*  String literals                                                    */
/* ------------------------------------------------------------------ */

/**
 * @brief Register a string value, returning its label index.
 *
 * @param v NUL-terminated string to register (copied on first registration).
 * @return Label index, or -1 if the registry is full.
 */
static int string_register(const char *v) {
    for (int i = 0; i < s_string_count; i++)
        if (strcmp(s_strings[i].value, v) == 0) return s_strings[i].label_id;
    if (s_string_count >= MAX_STRINGS) return -1;
    int id = s_string_count;
    s_strings[id].value    = strdup(v);
    s_strings[id].label_id = id;
    s_string_count++;
    return id;
}

/**
 * @brief Emit a NASM `db` string with non-printables escaped as byte values.
 *
 * @param out Output file.
 * @param s   String to emit.
 */
static void emit_str_escaped(FILE *out, const char *s) {
    fprintf(out, "\"");
    while (*s) {
        if      (*s == '"')  fprintf(out, "\", 34, \"");
        else if (*s == '\n') fprintf(out, "\", 10, \"");
        else if (*s == '\r') fprintf(out, "\", 13, \"");
        else                 fputc(*s, out);
        s++;
    }
    fprintf(out, "\"");
}

/**
 * @brief Recursively scan an expression subtree for STRING/CHARACTER literals.
 *
 * @param n   Root expression node.
 * @param out Output file for `.data` entries.
 */
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
    if (n->type == AST_ARRAY_ASSIGN) {
        scan_expr_strings(n->data.array_assign.index_expr, out);
        scan_expr_strings(n->data.array_assign.value_expr, out);
    }
    if (n->type == AST_ARRAY_REF)
        scan_expr_strings(n->data.array_ref.index_expr, out);
    if (n->type == AST_HASHMAP_INSERT) {
        scan_expr_strings(n->data.hashmap_insert.key_expr,   out);
        scan_expr_strings(n->data.hashmap_insert.value_expr, out);
    }
    if (n->type == AST_IMPLIED_DO) {
        scan_expr_strings(n->data.implied_do.expr,       out);
        scan_expr_strings(n->data.implied_do.start_expr, out);
        scan_expr_strings(n->data.implied_do.end_expr,   out);
        if (n->data.implied_do.step_expr)
            scan_expr_strings(n->data.implied_do.step_expr, out);
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
        case AST_ARRAY_ASSIGN:
            scan_expr_strings(s->data.array_assign.index_expr, out);
            scan_expr_strings(s->data.array_assign.value_expr, out);
            break;
        case AST_HASHMAP_INSERT:
            scan_expr_strings(s->data.hashmap_insert.key_expr,   out);
            scan_expr_strings(s->data.hashmap_insert.value_expr, out);
            break;
        case AST_IF: {
            scan_expr_strings(s->data.if_stmt.condition, out);
            struct ast_node *sub;
            for (sub = s->data.if_stmt.then_body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT || sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_strings(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_strings(sub->data.print.expr, out);
            }
            for (sub = s->data.if_stmt.else_body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT || sub->type == AST_ASSIGN_IMPLICIT)
                    scan_expr_strings(sub->data.assign.value, out);
                else if (sub->type == AST_PRINT)
                    scan_expr_strings(sub->data.print.expr, out);
            }
            break;
        }
        case AST_DO: {
            struct ast_node *sub;
            for (sub = s->data.do_loop.body; sub; sub = sub->next) {
                if (sub->type == AST_ASSIGN_EXPLICIT || sub->type == AST_ASSIGN_IMPLICIT)
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
    for (int i = 0; i < s_string_count; i++)
        if (strcmp(s_strings[i].value, value) == 0) return s_strings[i].label_id;
    return -1;
}

/* ------------------------------------------------------------------ */
/*  BSS variables                                                      */
/* ------------------------------------------------------------------ */

/** @brief Return 1 if `name` has already been emitted into BSS, 0 otherwise. */
static int var_seen(const char *name) {
    for (int i = 0; i < s_var_count; i++)
        if (strcmp(s_vars[i], name) == 0) return 1;
    return 0;
}

/**
 * @brief Recursively walk a statement list and emit BSS entries.
 *
 * Handles IF/DO nesting, implied-DO loop variables, array declarations,
 * and hashmap declarations.
 *
 * @param stmts Head of a statement list.
 * @param out   Output file for `.bss` lines.
 */
static void cg_scan_stmts_bss(struct ast_node *stmts, FILE *out) {
    for (struct ast_node *s = stmts; s; s = s->next) {
        const char *name = NULL;
        if (s->type == AST_ASSIGN_EXPLICIT || s->type == AST_ASSIGN_IMPLICIT)
            name = s->data.assign.var_name;

        if (name && !var_seen(name)) {
            fprintf(out, "var_%s resq 1\n", name);
            if (s_var_count < MAX_VARS)
                s_vars[s_var_count++] = strdup(name);
        }

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
        } else if (s->type == AST_ARRAY_DECL) {
            const char     *aname = s->data.array_decl.name;
            int             asize = s->data.array_decl.size;
            enum type_kind  atype = s->data.array_decl.elem_type;
            fprintf(out, "arr_%s resq %d\n", aname, asize);
            cg_register_array(aname, asize, atype);
        } else if (s->type == AST_HASHMAP_DECL) {
            const char *hname = s->data.hashmap_decl.name;
            fprintf(out, "hm_%s resb 2048\n", hname);
            cg_register_hashmap(hname);
        } else if (s->type == AST_PRINT) {
            if (s->data.print.expr &&
                s->data.print.expr->type == AST_IMPLIED_DO) {
                const char *vn = s->data.print.expr->data.implied_do.var_name;
                if (vn && !var_seen(vn)) {
                    fprintf(out, "var_%s resq 1\n", vn);
                    if (s_var_count < MAX_VARS)
                        s_vars[s_var_count++] = strdup(vn);
                }
            }
        }
    }
}

void cg_emit_bss_vars(struct ast_node *root, FILE *out) {
    if (!root || !out) return;
    struct ast_node *stmts = (root->type == AST_PROGRAM)
        ? root->data.program.statements : root;
    cg_scan_stmts_bss(stmts, out);
}
