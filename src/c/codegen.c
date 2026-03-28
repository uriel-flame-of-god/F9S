/**
 * @file codegen.c
 * @brief NASM x86-64 code generator for F9S.
 *
 * Walks the AST and emits a single `.asm` file targeting the Windows x64 ABI.
 * Integers live in `rax`; doubles in `xmm0`. All printf calls follow the
 * Windows x64 varargs convention (shadow space + xmm1 mirror for floats).
 *
 * Phase coverage: 1-4 (arithmetic, control flow) + 6 (arrays, hashmaps,
 * implied-DO I/O).
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "codegen.h"
#include "codegen_helpers.h"
#include "control.h"
#include "types.h"

/* ======================================================================
 * Type inference
 * ====================================================================== */

/**
 * @brief Infer the result type of an expression node.
 *
 * Comparison operators always return TYPE_LOGICAL (0 or 1).
 * Mixed integer/real arithmetic promotes to TYPE_REAL.
 * Array refs return the declared element type; hashmap lookups return TYPE_INT.
 *
 * @param expr Expression AST node (may be NULL).
 * @return Inferred `type_kind`; TYPE_INT if unknown.
 */
static enum type_kind infer_expr_type(struct ast_node *expr) {
    if (!expr) return TYPE_UNKNOWN;
    switch (expr->type) {
    case AST_LITERAL:
        return expr->data.literal.type;

    case AST_VARIABLE:
        return expr->data.variable.symbol
               ? expr->data.variable.symbol->type : TYPE_INT;

    case AST_BINARY_OP: {
        enum binary_op op = expr->data.binary.op;
        if (op == OP_EQ || op == OP_NE ||
            op == OP_LT || op == OP_GT ||
            op == OP_LE || op == OP_GE)
            return TYPE_LOGICAL;
        enum type_kind lt = infer_expr_type(expr->data.binary.left);
        enum type_kind rt = infer_expr_type(expr->data.binary.right);
        if (lt == TYPE_REAL || rt == TYPE_REAL) return TYPE_REAL;
        return TYPE_INT;
    }

    case AST_ARRAY_REF: {
        const char *n = expr->data.array_ref.name;
        if (cg_is_hashmap(n)) return TYPE_INT;
        return cg_is_array(n) ? cg_array_elem_type(n) : TYPE_INT;
    }

    case AST_IMPLIED_DO:
        return infer_expr_type(expr->data.implied_do.expr);

    default:
        return TYPE_INT;
    }
}

/* ======================================================================
 * Expression code generation
 * ====================================================================== */

/**
 * @brief Emit code to evaluate an expression.
 *
 * Result lands in:
 *   - `rax`  for integer / logical / string-pointer results
 *   - `xmm0` for REAL results
 *
 * The stack is balanced on exit (all pushes matched by pops).
 *
 * @param out  Output file handle.
 * @param expr Expression AST node to generate code for.
 */
static void gen_expr(FILE *out, struct ast_node *expr) {
    if (!expr) {
        fprintf(out, "    xor rax, rax\n");
        return;
    }

    enum type_kind expr_type = infer_expr_type(expr);

    switch (expr->type) {

    /* ---- Literals ---- */
    case AST_LITERAL:
        if (expr->data.literal.type == TYPE_INT ||
            expr->data.literal.type == TYPE_LOGICAL) {
            fprintf(out, "    mov rax, %lld\n",
                    expr->data.literal.value.int_val);
        } else if (expr->data.literal.type == TYPE_REAL) {
            int lid = cg_real_label_id(expr->data.literal.value.real_val);
            if (lid >= 0)
                fprintf(out, "    movsd xmm0, [rel _rc%d]\n", lid);
            else
                fprintf(out, "    xorpd xmm0, xmm0\n");
        } else if (expr->data.literal.type == TYPE_STRING ||
                   expr->data.literal.type == TYPE_CHARACTER) {
            int lid = cg_string_label_id(
                          expr->data.literal.value.string_val);
            if (lid >= 0)
                fprintf(out, "    lea rax, [rel _str%d]\n", lid);
            else
                fprintf(out, "    xor rax, rax\n");
        }
        break;

    /* ---- Variable reference ---- */
    case AST_VARIABLE:
        if (expr_type == TYPE_REAL)
            fprintf(out, "    movsd xmm0, [rel var_%s]\n",
                    expr->data.variable.name);
        else
            fprintf(out, "    mov rax, [rel var_%s]\n",
                    expr->data.variable.name);
        break;

    /* ---- Array element / hashmap lookup: name(index) ---- */
    case AST_ARRAY_REF: {
        const char *n = expr->data.array_ref.name;

        if (cg_is_hashmap(n)) {
            /* hashmap lookup: _f9s_hm_lookup(rcx=table, rdx=key) -> rax */
            gen_expr(out, expr->data.array_ref.index_expr);
            fprintf(out, "    mov rdx, rax\n");
            fprintf(out, "    lea rcx, [rel hm_%s]\n", n);
            fprintf(out, "    sub rsp, 32\n");
            fprintf(out, "    call _f9s_hm_lookup\n");
            fprintf(out, "    add rsp, 32\n");
        } else {
            /* array element: 1-based, slot = (index-1)*8 */
            enum type_kind etype = cg_array_elem_type(n);
            gen_expr(out, expr->data.array_ref.index_expr);
            fprintf(out, "    dec rax\n");
            fprintf(out, "    shl rax, 3\n");
            fprintf(out, "    lea rcx, [rel arr_%s]\n", n);
            if (etype == TYPE_REAL)
                fprintf(out, "    movsd xmm0, [rcx + rax]\n");
            else
                fprintf(out, "    mov rax, [rcx + rax]\n");
        }
        break;
    }

    /* ---- Binary operations ---- */
    case AST_BINARY_OP: {
        enum type_kind lt = infer_expr_type(expr->data.binary.left);
        enum type_kind rt = infer_expr_type(expr->data.binary.right);

        /* Comparison operators — always integer result 0/1 */
        switch (expr->data.binary.op) {
        case OP_EQ: case OP_NE:
        case OP_LT: case OP_GT:
        case OP_LE: case OP_GE: {
            gen_expr(out, expr->data.binary.left);
            if (lt == TYPE_REAL)
                fprintf(out, "    cvttsd2si rax, xmm0\n");
            fprintf(out, "    push rax\n");
            gen_expr(out, expr->data.binary.right);
            if (rt == TYPE_REAL)
                fprintf(out, "    cvttsd2si rax, xmm0\n");
            fprintf(out, "    mov  rbx, rax\n");
            fprintf(out, "    pop  rax\n");
            fprintf(out, "    cmp  rax, rbx\n");
            switch (expr->data.binary.op) {
            case OP_EQ: fprintf(out, "    sete  al\n"); break;
            case OP_NE: fprintf(out, "    setne al\n"); break;
            case OP_LT: fprintf(out, "    setl  al\n"); break;
            case OP_GT: fprintf(out, "    setg  al\n"); break;
            case OP_LE: fprintf(out, "    setle al\n"); break;
            case OP_GE: fprintf(out, "    setge al\n"); break;
            default: break;
            }
            fprintf(out, "    movzx rax, al\n");
            return;
        }
        default: break;
        }

        /* Arithmetic */
        if (expr_type == TYPE_REAL) {
            gen_expr(out, expr->data.binary.left);
            if (lt == TYPE_INT)
                fprintf(out, "    cvtsi2sd xmm0, rax\n");
            fprintf(out, "    sub rsp, 8\n");
            fprintf(out, "    movsd [rsp], xmm0\n");
            gen_expr(out, expr->data.binary.right);
            if (rt == TYPE_INT)
                fprintf(out, "    cvtsi2sd xmm0, rax\n");
            fprintf(out, "    movsd xmm1, xmm0\n");
            fprintf(out, "    movsd xmm0, [rsp]\n");
            fprintf(out, "    add rsp, 8\n");
            switch (expr->data.binary.op) {
            case OP_ADD: fprintf(out, "    addsd xmm0, xmm1\n"); break;
            case OP_SUB: fprintf(out, "    subsd xmm0, xmm1\n"); break;
            case OP_MUL: fprintf(out, "    mulsd xmm0, xmm1\n"); break;
            case OP_DIV: fprintf(out, "    divsd xmm0, xmm1\n"); break;
            default: break;
            }
        } else {
            gen_expr(out, expr->data.binary.left);
            fprintf(out, "    push rax\n");
            gen_expr(out, expr->data.binary.right);
            fprintf(out, "    mov rbx, rax\n");
            fprintf(out, "    pop rax\n");
            switch (expr->data.binary.op) {
            case OP_ADD: fprintf(out, "    add rax, rbx\n");  break;
            case OP_SUB: fprintf(out, "    sub rax, rbx\n");  break;
            case OP_MUL: fprintf(out, "    imul rax, rbx\n"); break;
            case OP_DIV:
                fprintf(out, "    cqo\n");
                fprintf(out, "    idiv rbx\n");
                break;
            default: break;
            }
        }
        break;
    }

    default:
        fprintf(out, "    xor rax, rax\n");
        break;
    }
}

/* ======================================================================
 * Printf helper — shared by PRINT and implied-DO
 * ====================================================================== */

/**
 * @brief Emit a printf call for the value already in rax or xmm0.
 *
 * Windows x64 varargs: shadow space = 32 bytes; float must appear in
 * both `xmm1` and `rdx`.
 *
 * @param out        Output file handle.
 * @param print_type Type of the value to be printed.
 */
static void emit_printf(FILE *out, enum type_kind print_type) {
    fprintf(out, "    sub rsp, 32\n");
    switch (print_type) {
    case TYPE_REAL:
        fprintf(out, "    lea rcx, [rel fmt_real]\n");
        fprintf(out, "    movq rdx, xmm0\n");
        fprintf(out, "    movsd xmm1, xmm0\n");
        break;
    case TYPE_STRING:
    case TYPE_CHARACTER:
        fprintf(out, "    lea rcx, [rel fmt_str]\n");
        fprintf(out, "    mov rdx, rax\n");
        break;
    case TYPE_LOGICAL:
        fprintf(out, "    test rax, rax\n");
        fprintf(out, "    lea rdx, [rel str_false]\n");
        fprintf(out, "    lea r8,  [rel str_true]\n");
        fprintf(out, "    cmovnz rdx, r8\n");
        fprintf(out, "    lea rcx, [rel fmt_str]\n");
        break;
    default: /* TYPE_INT */
        fprintf(out, "    lea rcx, [rel fmt_int]\n");
        fprintf(out, "    mov rdx, rax\n");
        break;
    }
    fprintf(out, "    call printf\n");
    fprintf(out, "    add rsp, 32\n");
}

/* ======================================================================
 * Statement code generation
 * ====================================================================== */

/**
 * @brief Emit code for a single statement node.
 *
 * Recursively handles nested statements in IF/DO bodies.
 *
 * @param out  Output file handle.
 * @param stmt Statement AST node.
 */
static void gen_stmt(FILE *out, struct ast_node *stmt) {
    if (!stmt) return;

    switch (stmt->type) {

    /* ---- Scalar assignment ---- */
    case AST_ASSIGN_EXPLICIT:
    case AST_ASSIGN_IMPLICIT: {
        enum type_kind vt = infer_expr_type(stmt->data.assign.value);
        gen_expr(out, stmt->data.assign.value);
        if (vt == TYPE_REAL)
            fprintf(out, "    movsd [rel var_%s], xmm0\n",
                    stmt->data.assign.var_name);
        else
            fprintf(out, "    mov [rel var_%s], rax\n",
                    stmt->data.assign.var_name);
        break;
    }

    /* ---- PRINT ---- */
    case AST_PRINT: {
        struct ast_node *pexpr = stmt->data.print.expr;

        /* Implied DO: PRINT *, (arr(i), i=1,n) */
        if (pexpr && pexpr->type == AST_IMPLIED_DO) {
            struct ast_node *ido = pexpr;
            int lbl = label_alloc();
            const char *vn = ido->data.implied_do.var_name;

            gen_expr(out, ido->data.implied_do.start_expr);
            fprintf(out, "    mov [rel var_%s], rax\n", vn);

            fprintf(out, ".ido_top_%d:\n", lbl);
            fprintf(out, "    mov  rax, [rel var_%s]\n", vn);
            fprintf(out, "    push rax\n");
            gen_expr(out, ido->data.implied_do.end_expr);
            fprintf(out, "    mov  rbx, rax\n");
            fprintf(out, "    pop  rax\n");
            fprintf(out, "    cmp  rax, rbx\n");
            fprintf(out, "    jg   .ido_end_%d\n", lbl);

            enum type_kind pt = infer_expr_type(ido->data.implied_do.expr);
            gen_expr(out, ido->data.implied_do.expr);
            emit_printf(out, pt);

            if (ido->data.implied_do.step_expr) {
                gen_expr(out, ido->data.implied_do.step_expr);
                fprintf(out, "    add [rel var_%s], rax\n", vn);
            } else {
                fprintf(out, "    add qword [rel var_%s], 1\n", vn);
            }
            fprintf(out, "    jmp  .ido_top_%d\n", lbl);
            fprintf(out, ".ido_end_%d:\n", lbl);
            break;
        }

        /* Normal PRINT */
        enum type_kind pt = infer_expr_type(pexpr);
        gen_expr(out, pexpr);
        emit_printf(out, pt);
        break;
    }

    /* ---- IF (cond) THEN ... [ELSE ...] END IF ---- */
    case AST_IF: {
        int lbl = label_alloc();
        gen_expr(out, stmt->data.if_stmt.condition);
        fprintf(out, "    test rax, rax\n");
        if (stmt->data.if_stmt.else_body)
            fprintf(out, "    jz .else_%d\n", lbl);
        else
            fprintf(out, "    jz .endif_%d\n", lbl);

        for (struct ast_node *s = stmt->data.if_stmt.then_body; s; s = s->next)
            gen_stmt(out, s);

        if (stmt->data.if_stmt.else_body) {
            fprintf(out, "    jmp .endif_%d\n", lbl);
            fprintf(out, ".else_%d:\n", lbl);
            for (struct ast_node *s = stmt->data.if_stmt.else_body; s; s = s->next)
                gen_stmt(out, s);
        }
        fprintf(out, ".endif_%d:\n", lbl);
        break;
    }

    /* ---- DO var = start, end [, step] ... END DO ---- */
    case AST_DO: {
        int lbl = label_alloc();
        const char *vn = stmt->data.do_loop.var_name;

        gen_expr(out, stmt->data.do_loop.start_expr);
        fprintf(out, "    mov [rel var_%s], rax\n", vn);

        fprintf(out, ".do_top_%d:\n", lbl);
        fprintf(out, "    mov  rax, [rel var_%s]\n", vn);
        fprintf(out, "    push rax\n");
        gen_expr(out, stmt->data.do_loop.end_expr);
        fprintf(out, "    mov  rbx, rax\n");
        fprintf(out, "    pop  rax\n");
        fprintf(out, "    cmp  rax, rbx\n");
        fprintf(out, "    jg   .do_end_%d\n", lbl);

        for (struct ast_node *s = stmt->data.do_loop.body; s; s = s->next)
            gen_stmt(out, s);

        if (stmt->data.do_loop.step_expr) {
            gen_expr(out, stmt->data.do_loop.step_expr);
            fprintf(out, "    add  [rel var_%s], rax\n", vn);
        } else {
            fprintf(out, "    add  qword [rel var_%s], 1\n", vn);
        }
        fprintf(out, "    jmp  .do_top_%d\n", lbl);
        fprintf(out, ".do_end_%d:\n", lbl);
        break;
    }

    /* ---- STOP ---- */
    case AST_STOP:
        fprintf(out, "    xor  eax, eax\n");
        fprintf(out, "    pop  rbp\n");
        fprintf(out, "    ret\n");
        break;

    /* ---- Array declaration: INTEGER :: arr(10) ----
     * No runtime code; BSS allocation is handled by cg_emit_bss_vars. */
    case AST_ARRAY_DECL:
        break;

    /* ---- Array element assignment: arr(i) = expr ----
     * 1-based FORTRAN indexing: slot = base + (index-1)*8. */
    case AST_ARRAY_ASSIGN: {
        const char *n  = stmt->data.array_assign.name;
        enum type_kind vt = infer_expr_type(stmt->data.array_assign.value_expr);

        if (vt == TYPE_REAL) {
            gen_expr(out, stmt->data.array_assign.value_expr);
            fprintf(out, "    sub rsp, 8\n");
            fprintf(out, "    movsd [rsp], xmm0\n");
            gen_expr(out, stmt->data.array_assign.index_expr);
            fprintf(out, "    dec rax\n");
            fprintf(out, "    shl rax, 3\n");
            fprintf(out, "    lea rcx, [rel arr_%s]\n", n);
            fprintf(out, "    movsd xmm0, [rsp]\n");
            fprintf(out, "    add rsp, 8\n");
            fprintf(out, "    movsd [rcx + rax], xmm0\n");
        } else {
            gen_expr(out, stmt->data.array_assign.value_expr);
            fprintf(out, "    push rax\n");
            gen_expr(out, stmt->data.array_assign.index_expr);
            fprintf(out, "    dec rax\n");
            fprintf(out, "    shl rax, 3\n");
            fprintf(out, "    lea rcx, [rel arr_%s]\n", n);
            fprintf(out, "    pop rbx\n");
            fprintf(out, "    mov [rcx + rax], rbx\n");
        }
        break;
    }

    /* ---- Hashmap declaration: HASHMAP :: map ----
     * No runtime code; BSS allocation handled by cg_emit_bss_vars. */
    case AST_HASHMAP_DECL:
        break;

    /* ---- Hashmap insert: map(key) := value ----
     * Calls _f9s_hm_insert(rcx=table, rdx=key, r8=value). */
    case AST_HASHMAP_INSERT: {
        const char *n = stmt->data.hashmap_insert.name;
        gen_expr(out, stmt->data.hashmap_insert.key_expr);
        fprintf(out, "    push rax\n");
        gen_expr(out, stmt->data.hashmap_insert.value_expr);
        fprintf(out, "    mov r8, rax\n");
        fprintf(out, "    pop rdx\n");
        fprintf(out, "    lea rcx, [rel hm_%s]\n", n);
        fprintf(out, "    sub rsp, 32\n");
        fprintf(out, "    call _f9s_hm_insert\n");
        fprintf(out, "    add rsp, 32\n");
        break;
    }

    default:
        break;
    }
}

/* ======================================================================
 * Top-level entry point
 * ====================================================================== */

/**
 * @brief Generate a complete NASM assembly file from the program AST.
 *
 * Emits sections in order:
 *   1. `section .data`  — format strings, string/real literals
 *   2. `section .bss`   — zero-initialised variables, arrays, hashmaps
 *   3. `section .text`  — hashmap runtime (if needed), then `main`
 *
 * @param ast             Root AST_PROGRAM node.
 * @param output_filename Path to the output `.asm` file.
 * @return 0 on success, -1 on I/O error.
 */
int generate_code(struct ast_node *ast, const char *output_filename) {
    if (!ast || !output_filename) return -1;

    FILE *out = fopen(output_filename, "w");
    if (!out) return -1;

    label_reset();
    cg_helpers_reset();

    fprintf(out, "default rel\n");
    fprintf(out, "extern printf\n\n");

    fprintf(out, "section .data\n");
    fprintf(out, "fmt_int   db \"%%lld\", 10, 0\n");
    fprintf(out, "fmt_real  db \"%%.17g\", 10, 0\n");
    fprintf(out, "fmt_str   db \"%%s\",   10, 0\n");
    fprintf(out, "str_true  db \".TRUE.\",  0\n");
    fprintf(out, "str_false db \".FALSE.\", 0\n");
    cg_emit_data_literals(ast, out);
    cg_emit_data_strings(ast, out);
    fprintf(out, "\n");

    fprintf(out, "section .bss\n");
    cg_emit_bss_vars(ast, out);
    fprintf(out, "\n");

    fprintf(out, "section .text\n");
    if (cg_hashmap_count() > 0)
        cg_emit_hashmap_runtime(out);

    fprintf(out, "global main\n");
    fprintf(out, "main:\n");
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");

    struct ast_node *stmts = (ast->type == AST_PROGRAM)
        ? ast->data.program.statements : ast;

    for (struct ast_node *s = stmts; s; s = s->next)
        gen_stmt(out, s);

    fprintf(out, "    xor eax, eax\n");
    fprintf(out, "    pop rbp\n");
    fprintf(out, "    ret\n");

    fclose(out);
    return 0;
}
