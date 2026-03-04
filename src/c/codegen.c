// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "codegen.h"
#include "codegen_helpers.h"
#include "control.h"
#include "types.h"

static enum type_kind infer_expr_type(struct ast_node *expr) {
    if (!expr) return TYPE_UNKNOWN;
    switch (expr->type) {
    case AST_LITERAL:
        return expr->data.literal.type;
    case AST_VARIABLE:
        return expr->data.variable.symbol ? expr->data.variable.symbol->type : TYPE_INT;
    case AST_BINARY_OP: {
        /* Comparison operators always yield an integer 0/1 */
        enum binary_op op = expr->data.binary.op;
        if (op == OP_EQ || op == OP_NE ||
            op == OP_LT || op == OP_GT ||
            op == OP_LE || op == OP_GE)
            return TYPE_LOGICAL;
        enum type_kind left_t  = infer_expr_type(expr->data.binary.left);
        enum type_kind right_t = infer_expr_type(expr->data.binary.right);
        /* Promote to REAL if either operand is REAL */
        if (left_t == TYPE_REAL || right_t == TYPE_REAL) return TYPE_REAL;
        return TYPE_INT;
    }
    default:
        return TYPE_INT;
    }
}

/* Generate code to evaluate expr and leave result in rax (int) or xmm0 (real) */
static void gen_expr(FILE *out, struct ast_node *expr) {
    if (!expr) {
        fprintf(out, "    xor rax, rax\n");
        return;
    }

    enum type_kind expr_type = infer_expr_type(expr);

    switch (expr->type) {
    case AST_LITERAL:
        if (expr->data.literal.type == TYPE_INT || expr->data.literal.type == TYPE_LOGICAL) {
            fprintf(out, "    mov rax, %lld\n", expr->data.literal.value.int_val);
        } else if (expr->data.literal.type == TYPE_REAL) {
            int label_id = cg_real_label_id(expr->data.literal.value.real_val);
            if (label_id >= 0) {
                fprintf(out, "    movsd xmm0, [rel _rc%d]\n", label_id);
            } else {
                fprintf(out, "    xorpd xmm0, xmm0\n");
            }
        } else if (expr->data.literal.type == TYPE_STRING || 
                   expr->data.literal.type == TYPE_CHARACTER) {
            int label_id = cg_string_label_id(expr->data.literal.value.string_val);
            if (label_id >= 0) {
                fprintf(out, "    lea rax, [rel _str%d]\n", label_id);
            } else {
                fprintf(out, "    xor rax, rax\n");
            }
        }
        break;

    case AST_VARIABLE:
        if (expr_type == TYPE_REAL) {
            fprintf(out, "    movsd xmm0, [rel var_%s]\n", expr->data.variable.name);
        } else {
            fprintf(out, "    mov rax, [rel var_%s]\n", expr->data.variable.name);
        }
        break;

    case AST_BINARY_OP: {
        enum type_kind left_t  = infer_expr_type(expr->data.binary.left);
        enum type_kind right_t = infer_expr_type(expr->data.binary.right);

        /* ---- Comparison operators — integer comparisons only ---- */
        switch (expr->data.binary.op) {
        case OP_EQ: case OP_NE:
        case OP_LT: case OP_GT:
        case OP_LE: case OP_GE: {
            /* Evaluate left into rax, push; evaluate right into rax.
             * Both sides coerced to integer (REAL via cvttsd2si). */
            gen_expr(out, expr->data.binary.left);
            if (left_t == TYPE_REAL)
                fprintf(out, "    cvttsd2si rax, xmm0\n");
            fprintf(out, "    push rax\n");
            gen_expr(out, expr->data.binary.right);
            if (right_t == TYPE_REAL)
                fprintf(out, "    cvttsd2si rax, xmm0\n");
            fprintf(out, "    mov  rbx, rax\n");  /* rbx = right */
            fprintf(out, "    pop  rax\n");        /* rax = left  */
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
            return;  /* result in rax (0 or 1) */
        }
        default: break;
        }

        if (expr_type == TYPE_REAL) {
            /* REAL arithmetic */
            gen_expr(out, expr->data.binary.left);
            if (left_t == TYPE_INT) {
                fprintf(out, "    cvtsi2sd xmm0, rax\n");
            }
            fprintf(out, "    sub rsp, 8\n");
            fprintf(out, "    movsd [rsp], xmm0\n");

            gen_expr(out, expr->data.binary.right);
            if (right_t == TYPE_INT) {
                fprintf(out, "    cvtsi2sd xmm0, rax\n");
            }
            fprintf(out, "    movsd xmm1, xmm0\n");
            fprintf(out, "    movsd xmm0, [rsp]\n");
            fprintf(out, "    add rsp, 8\n");

            switch (expr->data.binary.op) {
            case OP_ADD: fprintf(out, "    addsd xmm0, xmm1\n"); break;
            case OP_SUB: fprintf(out, "    subsd xmm0, xmm1\n"); break;
            case OP_MUL: fprintf(out, "    mulsd xmm0, xmm1\n"); break;
            case OP_DIV: fprintf(out, "    divsd xmm0, xmm1\n"); break;
            default: break;  /* comparison ops handled above */
            }
        } else {
            /* INTEGER arithmetic */
            gen_expr(out, expr->data.binary.left);
            fprintf(out, "    push rax\n");
            gen_expr(out, expr->data.binary.right);
            fprintf(out, "    mov rbx, rax\n");
            fprintf(out, "    pop rax\n");
            switch (expr->data.binary.op) {
            case OP_ADD: fprintf(out, "    add rax, rbx\n"); break;
            case OP_SUB: fprintf(out, "    sub rax, rbx\n"); break;
            case OP_MUL: fprintf(out, "    imul rax, rbx\n"); break;
            case OP_DIV:
                fprintf(out, "    cqo\n");
                fprintf(out, "    idiv rbx\n");
                break;
            default: break;  /* comparison ops handled above */
            }
        }
        break;
    }
    default:
        fprintf(out, "    xor rax, rax\n");
        break;
    }
}

static void gen_stmt(FILE *out, struct ast_node *stmt) {
    if (!stmt) return;
    switch (stmt->type) {
    case AST_ASSIGN_EXPLICIT:
    case AST_ASSIGN_IMPLICIT: {
        enum type_kind val_type = infer_expr_type(stmt->data.assign.value);
        gen_expr(out, stmt->data.assign.value);
        if (val_type == TYPE_REAL) {
            fprintf(out, "    movsd [rel var_%s], xmm0\n", stmt->data.assign.var_name);
        } else {
            fprintf(out, "    mov [rel var_%s], rax\n", stmt->data.assign.var_name);
        }
        break;
    }
    case AST_PRINT: {
        enum type_kind print_type = infer_expr_type(stmt->data.print.expr);
        gen_expr(out, stmt->data.print.expr);

        fprintf(out, "    sub rsp, 32\n");
        if (print_type == TYPE_REAL) {
            fprintf(out, "    lea rcx, [rel fmt_real]\n");
            /* Windows x64 varargs convention: pass float as both xmm1 and rdx */
            fprintf(out, "    movq rdx, xmm0\n");   /* Copy bit pattern to integer reg */
            fprintf(out, "    movsd xmm1, xmm0\n"); /* Keep in xmm1 too */
        } else if (print_type == TYPE_STRING || print_type == TYPE_CHARACTER) {
            fprintf(out, "    lea rcx, [rel fmt_str]\n");
            fprintf(out, "    mov rdx, rax\n");
        } else if (print_type == TYPE_LOGICAL) {
            /* Convert 0 → ".FALSE.", else → ".TRUE." */
            fprintf(out, "    test rax, rax\n");
            fprintf(out, "    lea rdx, [rel str_false]\n");
            fprintf(out, "    lea r8, [rel str_true]\n");
            fprintf(out, "    cmovnz rdx, r8\n");
            fprintf(out, "    lea rcx, [rel fmt_str]\n");
        } else {
            /* TYPE_INT */
            fprintf(out, "    lea rcx, [rel fmt_int]\n");
            fprintf(out, "    mov rdx, rax\n");
        }
        fprintf(out, "    call printf\n");
        fprintf(out, "    add rsp, 32\n");
        break;
    }

    /* ---------------------------------------------------------------- */
    /* Control flow                                                     */
    /* ---------------------------------------------------------------- */

    case AST_IF: {
        /*
         * Evaluates condition into rax.
         * test rax, rax  →  jz to else/endif
         * then-body
         * [jmp endif]
         * [.else_N:]
         * [else-body]
         * .endif_N:
         *
         * All branch logic is pure NASM — no C runtime calls.
         */
        int lbl = label_alloc();
        gen_expr(out, stmt->data.if_stmt.condition);
        fprintf(out, "    test rax, rax\n");
        if (stmt->data.if_stmt.else_body)
            fprintf(out, "    jz .else_%d\n", lbl);
        else
            fprintf(out, "    jz .endif_%d\n", lbl);

        /* Then body */
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

    case AST_DO: {
        /*
         * DO i = start, end [, step]
         *
         *   mov [var_i], <start>
         * .do_top_N:
         *   cmp [var_i], <end>
         *   jg  .do_end_N          ; exit when i > end
         *   <body>
         *   add [var_i], <step>    ; default 1
         *   jmp .do_top_N
         * .do_end_N:
         *
         * Integer arithmetic only; all branching in pure NASM.
         */
        int lbl = label_alloc();
        const char *vn = stmt->data.do_loop.var_name;

        /* Initialise loop variable */
        gen_expr(out, stmt->data.do_loop.start_expr);
        fprintf(out, "    mov [rel var_%s], rax\n", vn);

        fprintf(out, ".do_top_%d:\n", lbl);

        /* Loop condition: compare current value against end */
        fprintf(out, "    mov  rax, [rel var_%s]\n", vn);
        fprintf(out, "    push rax\n");                          /* save i */
        gen_expr(out, stmt->data.do_loop.end_expr);
        fprintf(out, "    mov  rbx, rax\n");                     /* rbx = end */
        fprintf(out, "    pop  rax\n");                          /* rax = i   */
        fprintf(out, "    cmp  rax, rbx\n");
        fprintf(out, "    jg   .do_end_%d\n", lbl);

        /* Loop body */
        for (struct ast_node *s = stmt->data.do_loop.body; s; s = s->next)
            gen_stmt(out, s);

        /* Step */
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

    case AST_STOP:
        /* Exit process cleanly with code 0 */
        fprintf(out, "    xor  eax, eax\n");
        fprintf(out, "    pop  rbp\n");
        fprintf(out, "    ret\n");
        break;
    default:
        break;
    }
}

int generate_code(struct ast_node *ast, const char *output_filename) {
    if (!ast || !output_filename) return -1;

    FILE *out = fopen(output_filename, "w");
    if (!out) return -1;

    /* Reset label counter so each compilation starts from label 0 */
    label_reset();

    /* Reset helper registries for this compilation unit */
    cg_helpers_reset();

    /* Preamble */
    fprintf(out, "default rel\n");
    fprintf(out, "extern printf\n\n");

    /* Data section: format strings and literals */
    fprintf(out, "section .data\n");
    fprintf(out, "fmt_int db \"%%lld\", 10, 0\n");
    fprintf(out, "fmt_real db \"%%.17g\", 10, 0\n");
    fprintf(out, "fmt_str db \"%%s\", 10, 0\n");
    fprintf(out, "str_true db \".TRUE.\", 0\n");
    fprintf(out, "str_false db \".FALSE.\", 0\n");
    cg_emit_data_literals(ast, out);
    cg_emit_data_strings(ast, out);
    fprintf(out, "\n");

    /* BSS section: variables */
    fprintf(out, "section .bss\n");
    cg_emit_bss_vars(ast, out);
    fprintf(out, "\n");

    /* Text section */
    fprintf(out, "section .text\n");
    fprintf(out, "global main\n");
    fprintf(out, "main:\n");
    fprintf(out, "    push rbp\n");
    fprintf(out, "    mov rbp, rsp\n");

    struct ast_node *stmts = (ast->type == AST_PROGRAM)
        ? ast->data.program.statements : ast;
    for (struct ast_node *s = stmts; s; s = s->next) {
        gen_stmt(out, s);
    }

    fprintf(out, "    xor eax, eax\n");
    fprintf(out, "    pop rbp\n");
    fprintf(out, "    ret\n");

    fclose(out);
    return 0;
}
