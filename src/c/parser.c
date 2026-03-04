// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"
#include "error_handler.h"
#include "parser.h"
#include "symbol.h"
#include "types.h"

typedef enum {
    TOK_EOF,
    TOK_NEWLINE,
    TOK_IDENT,
    TOK_INT_LIT,
    TOK_REAL_LIT,
    TOK_STRING_LIT,
    TOK_LOGICAL_LIT,    /* .TRUE. / .FALSE. */
    TOK_COMPLEX_LIT,    /* (r,i)            */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_ASSIGN_OP,      /* := */
    TOK_KW_PROGRAM,
    TOK_KW_END,
    TOK_KW_INTEGER,
    TOK_KW_REAL,
    TOK_KW_STRING,
    TOK_KW_LOGICAL,
    TOK_KW_COMPLEX,
    TOK_KW_PRINT,
    TOK_KW_ASSIGN,
    /* Phase 4 — control flow */
    TOK_KW_IF,
    TOK_KW_THEN,
    TOK_KW_ELSE,
    TOK_KW_DO,
    TOK_KW_STOP,
    TOK_EQUALS,         /* =   (DO variable assignment) */
    TOK_EQ,             /* ==  */
    TOK_NE,             /* /=  */
    TOK_LT,             /* <   */
    TOK_GT,             /* >   */
    TOK_LE,             /* <=  */
    TOK_GE              /* >=  */
} token_type;

typedef struct {
    token_type  type;
    char        text[512];   /* raw lexeme */
    int         line;
} token;

typedef struct {
    const char *src;
    const char *pos;
    int         line;
    const char *filename;
    token       lookahead;
    int         has_lookahead;
} lexer;

static void lexer_init(lexer *l, const char *src, const char *filename) {
    l->src          = src;
    l->pos          = src;
    l->line         = 1;
    l->filename     = filename;
    l->has_lookahead = 0;
}

/* Skip spaces/tabs (not newlines) and ! comments to end-of-line */
static void skip_inline_ws_and_comments(lexer *l) {
    for (;;) {
        while (*l->pos == ' ' || *l->pos == '\t' || *l->pos == '\r')
            l->pos++;
        if (*l->pos == '!') {
            while (*l->pos && *l->pos != '\n')
                l->pos++;
        } else {
            break;
        }
    }
}

static int ci_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

/* Classify an identifier as keyword or plain identifier */
static token_type classify_ident(const char *text) {
    if (ci_eq(text, "PROGRAM"))  return TOK_KW_PROGRAM;
    if (ci_eq(text, "END"))      return TOK_KW_END;
    if (ci_eq(text, "INTEGER"))  return TOK_KW_INTEGER;
    if (ci_eq(text, "REAL"))     return TOK_KW_REAL;
    if (ci_eq(text, "STRING"))   return TOK_KW_STRING;
    if (ci_eq(text, "LOGICAL"))  return TOK_KW_LOGICAL;
    if (ci_eq(text, "COMPLEX"))  return TOK_KW_COMPLEX;
    if (ci_eq(text, "PRINT"))    return TOK_KW_PRINT;
    if (ci_eq(text, "ASSIGN"))   return TOK_KW_ASSIGN;
    if (ci_eq(text, "IF"))       return TOK_KW_IF;
    if (ci_eq(text, "THEN"))     return TOK_KW_THEN;
    if (ci_eq(text, "ELSE"))     return TOK_KW_ELSE;
    if (ci_eq(text, "DO"))       return TOK_KW_DO;
    if (ci_eq(text, "STOP"))     return TOK_KW_STOP;
    return TOK_IDENT;
}

static token lexer_next_raw(lexer *l) {
    token t;
    t.text[0] = '\0';
    t.line = l->line;

    skip_inline_ws_and_comments(l);

    if (*l->pos == '\0') {
        t.type = TOK_EOF;
        return t;
    }

    /* Newline */
    if (*l->pos == '\n') {
        t.type = TOK_NEWLINE;
        strcpy(t.text, "\\n");
        l->pos++;
        l->line++;
        return t;
    }

    /* String literal: "..." or '...' */
    if (*l->pos == '"' || *l->pos == '\'') {
        char q = *l->pos;
        const char *start = l->pos;
        l->pos++;
        while (*l->pos && *l->pos != q && *l->pos != '\n')
            l->pos++;
        if (*l->pos == q) l->pos++;
        size_t len = (size_t)(l->pos - start);
        if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
        memcpy(t.text, start, len);
        t.text[len] = '\0';
        t.type = TOK_STRING_LIT;
        return t;
    }

    /* Logical literal: .TRUE. / .FALSE. */
    if (*l->pos == '.') {
        const char *save = l->pos;
        l->pos++;
        const char *word_start = l->pos;
        while (isalpha((unsigned char)*l->pos)) l->pos++;
        if (*l->pos == '.') {
            l->pos++;
            size_t len = (size_t)(l->pos - save);
            if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
            memcpy(t.text, save, len);
            t.text[len] = '\0';
            t.type = TOK_LOGICAL_LIT;
            return t;
        }
        /* Not a logical — restore and fall through */
        l->pos = save;
        (void)word_start;
    }

    /* Complex literal: (re, im) — quick lookahead */
    if (*l->pos == '(') {
        /* Try to scan a full complex literal */
        const char *save = l->pos;
        const char *end  = save + 1;
        int found_comma  = 0;
        int depth        = 1;
        while (*end && depth > 0) {
            if (*end == '(') depth++;
            else if (*end == ')') depth--;
            else if (*end == ',' && depth == 1) found_comma = 1;
            end++;
        }
        if (found_comma && depth == 0) {
            /* Looks like a complex literal — validate */
            size_t len = (size_t)(end - save);
            char tmp[512];
            if (len < sizeof(tmp)) {
                memcpy(tmp, save, len);
                tmp[len] = '\0';
                /* Use existing is_complex check via detect_type */
                if (detect_type(tmp) == TYPE_COMPLEX) {
                    memcpy(t.text, tmp, len + 1);
                    t.type = TOK_COMPLEX_LIT;
                    l->pos = end;
                    return t;
                }
            }
        }
        /* Not complex — fall through to handle '(' as LPAREN */
    }

    /* Number: integer or real */
    if (isdigit((unsigned char)*l->pos) ||
        (*l->pos == '-' && isdigit((unsigned char)*(l->pos + 1)))) {
        const char *start = l->pos;
        if (*l->pos == '-') l->pos++;
        while (isdigit((unsigned char)*l->pos)) l->pos++;
        int is_real = 0;
        if (*l->pos == '.') { is_real = 1; l->pos++; }
        while (isdigit((unsigned char)*l->pos)) l->pos++;
        if (*l->pos == 'e' || *l->pos == 'E') {
            is_real = 1; l->pos++;
            if (*l->pos == '+' || *l->pos == '-') l->pos++;
            while (isdigit((unsigned char)*l->pos)) l->pos++;
        }
        size_t len = (size_t)(l->pos - start);
        if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
        memcpy(t.text, start, len);
        t.text[len] = '\0';
        t.type = is_real ? TOK_REAL_LIT : TOK_INT_LIT;
        return t;
    }

    /* Identifier or keyword */
    if (isalpha((unsigned char)*l->pos) || *l->pos == '_') {
        const char *start = l->pos;
        while (isalnum((unsigned char)*l->pos) || *l->pos == '_')
            l->pos++;
        size_t len = (size_t)(l->pos - start);
        if (len >= sizeof(t.text)) len = sizeof(t.text) - 1;
        memcpy(t.text, start, len);
        t.text[len] = '\0';
        t.type = classify_ident(t.text);
        return t;
    }

    /* Two-char operators */
    if (*l->pos == ':' && *(l->pos + 1) == '=') {
        t.type = TOK_ASSIGN_OP;
        strcpy(t.text, ":=");
        l->pos += 2;
        return t;
    }

    /* Single-char and comparison tokens */
    switch (*l->pos) {
    case '+': t.type = TOK_PLUS;   strcpy(t.text, "+"); l->pos++; return t;
    case '-': t.type = TOK_MINUS;  strcpy(t.text, "-"); l->pos++; return t;
    case '*': t.type = TOK_STAR;   strcpy(t.text, "*"); l->pos++; return t;
    case '/':
        if (*(l->pos + 1) == '=') {
            t.type = TOK_NE; strcpy(t.text, "/="); l->pos += 2; return t;
        }
        t.type = TOK_SLASH; strcpy(t.text, "/"); l->pos++; return t;
    case '(': t.type = TOK_LPAREN; strcpy(t.text, "("); l->pos++; return t;
    case ')': t.type = TOK_RPAREN; strcpy(t.text, ")"); l->pos++; return t;
    case ',': t.type = TOK_COMMA;  strcpy(t.text, ","); l->pos++; return t;
    case '=':
        if (*(l->pos + 1) == '=') {
            t.type = TOK_EQ; strcpy(t.text, "=="); l->pos += 2; return t;
        }
        t.type = TOK_EQUALS; strcpy(t.text, "="); l->pos++; return t;
    case '<':
        if (*(l->pos + 1) == '=') {
            t.type = TOK_LE; strcpy(t.text, "<="); l->pos += 2; return t;
        }
        t.type = TOK_LT; strcpy(t.text, "<"); l->pos++; return t;
    case '>':
        if (*(l->pos + 1) == '=') {
            t.type = TOK_GE; strcpy(t.text, ">="); l->pos += 2; return t;
        }
        t.type = TOK_GT; strcpy(t.text, ">"); l->pos++; return t;
    default:
        t.type = TOK_EOF;  /* Unknown — treat as end */
        l->pos++;
        return t;
    }
}

/* ---- peek / consume helpers ---- */

static token lexer_peek(lexer *l) {
    if (!l->has_lookahead) {
        l->lookahead    = lexer_next_raw(l);
        l->has_lookahead = 1;
    }
    return l->lookahead;
}

static token lexer_consume(lexer *l) {
    if (l->has_lookahead) {
        l->has_lookahead = 0;
        return l->lookahead;
    }
    return lexer_next_raw(l);
}

/* Skip newlines; returns 0 if no newlines were consumed */
static int skip_newlines(lexer *l) {
    int count = 0;
    while (lexer_peek(l).type == TOK_NEWLINE) {
        lexer_consume(l);
        count++;
    }
    return count;
}

struct ast_node *ast_node_alloc(enum ast_node_type type) {
    struct ast_node *n = calloc(1, sizeof(*n));
    if (n) n->type = type;
    return n;
}

void ast_destroy(struct ast_node *node) {
    if (!node) return;
    ast_destroy(node->next);
    switch (node->type) {
    case AST_PROGRAM:
        free(node->data.program.name);
        ast_destroy(node->data.program.statements);
        break;
    case AST_ASSIGN_EXPLICIT:
    case AST_ASSIGN_IMPLICIT:
        free(node->data.assign.var_name);
        ast_destroy(node->data.assign.value);
        break;
    case AST_PRINT:
        ast_destroy(node->data.print.expr);
        break;
    case AST_BINARY_OP:
        ast_destroy(node->data.binary.left);
        ast_destroy(node->data.binary.right);
        break;
    case AST_LITERAL:
        if ((node->data.literal.type == TYPE_STRING  ||
             node->data.literal.type == TYPE_CHARACTER) &&
             node->data.literal.value.string_val) {
            free(node->data.literal.value.string_val);
        }
        break;
    case AST_VARIABLE:
        free(node->data.variable.name);
        break;
    case AST_IF:
        ast_destroy(node->data.if_stmt.condition);
        ast_destroy(node->data.if_stmt.then_body);
        ast_destroy(node->data.if_stmt.else_body);
        break;
    case AST_DO:
        free(node->data.do_loop.var_name);
        ast_destroy(node->data.do_loop.start_expr);
        ast_destroy(node->data.do_loop.end_expr);
        ast_destroy(node->data.do_loop.step_expr);
        ast_destroy(node->data.do_loop.body);
        break;
    case AST_STOP:
        break;
    }
    free(node);
}

/* ================================================================== */
/*  Parser state                                                      */
/* ================================================================== */

typedef struct {
    lexer       lex;
    const char *filename;
    int         error;    /* set on any parse error */
} parser_state;

static void parse_err(parser_state *p, const char *fmt, ...) {
    p->error = 1;
    char body[480];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    struct error_context ctx = {
        .filename    = p->filename,
        .line_number = lexer_peek(&p->lex).line,
        .column      = 0,
        .line_content = NULL
    };
    set_error_context(&ctx);
    slap("%s", body);
    clear_error_context();
}

static token expect(parser_state *p, token_type tt, const char *what) {
    token t = lexer_consume(&p->lex);
    if (t.type != tt) {
        parse_err(p, "Expected %s, got '%s'", what, t.text);
    }
    return t;
}

/* ================================================================== */
/*  Expression parsing (recursive descent)                           */
/* ================================================================== */

static struct ast_node *parse_expression(parser_state *p);

static struct ast_node *parse_literal_node(parser_state *p) {
    token t = lexer_peek(&p->lex);
    struct ast_node *n = NULL;

    switch (t.type) {
    case TOK_INT_LIT:
        lexer_consume(&p->lex);
        n = ast_node_alloc(AST_LITERAL);
        if (!n) return NULL;
        n->data.literal.type = TYPE_INT;
        n->data.literal.value.int_val = strtoll(t.text, NULL, 10);
        return n;

    case TOK_REAL_LIT:
        lexer_consume(&p->lex);
        n = ast_node_alloc(AST_LITERAL);
        if (!n) return NULL;
        n->data.literal.type = TYPE_REAL;
        n->data.literal.value.real_val = strtod(t.text, NULL);
        return n;

    case TOK_STRING_LIT: {
        lexer_consume(&p->lex);
        n = ast_node_alloc(AST_LITERAL);
        if (!n) return NULL;
        n->data.literal.type = TYPE_STRING;
        /* Strip surrounding quotes */
        size_t len = strlen(t.text);
        if (len >= 2 && (t.text[0] == '"' || t.text[0] == '\'')) {
            size_t slen = len - 2;
            char *buf = (char *)malloc(slen + 1);
            if (!buf) { free(n); return NULL; }
            memcpy(buf, t.text + 1, slen);
            buf[slen] = '\0';
            n->data.literal.value.string_val = buf;
        } else {
            n->data.literal.value.string_val = strdup(t.text);
        }
        return n;
    }

    case TOK_LOGICAL_LIT:
        lexer_consume(&p->lex);
        n = ast_node_alloc(AST_LITERAL);
        if (!n) return NULL;
        n->data.literal.type = TYPE_LOGICAL;
        /* .TRUE. / .FALSE. comparisons */
        n->data.literal.value.int_val =
            (tolower((unsigned char)t.text[1]) == 't') ? 1LL : 0LL;
        return n;

    case TOK_COMPLEX_LIT: {
        lexer_consume(&p->lex);
        n = ast_node_alloc(AST_LITERAL);
        if (!n) return NULL;
        /* Parse "(re, im)" */
        double re = 0.0, im = 0.0;
        /* Store as real — complex numbers need special handling;
           we store r+i in the string for codegen clarity */
        sscanf(t.text, "(%lf , %lf)", &re, &im);
        if (re == 0.0 && im == 0.0) {
            /* Try without spaces */
            sscanf(t.text, "(%lf,%lf)", &re, &im);
        }
        n->data.literal.type = TYPE_REAL;   /* approximation for now */
        n->data.literal.value.real_val = re;
        /* Store full text as string for future codegen */
        (void)im;
        return n;
    }

    default:
        return NULL;
    }
}

static struct ast_node *parse_primary(parser_state *p) {
    token t = lexer_peek(&p->lex);

    /* Parenthesised expression */
    if (t.type == TOK_LPAREN) {
        lexer_consume(&p->lex);
        struct ast_node *inner = parse_expression(p);
        expect(p, TOK_RPAREN, "')'");
        return inner;
    }

    /* Identifier / variable reference */
    if (t.type == TOK_IDENT) {
        lexer_consume(&p->lex);
        struct ast_node *n = ast_node_alloc(AST_VARIABLE);
        if (!n) return NULL;
        n->data.variable.name   = strdup(t.text);
        n->data.variable.symbol = NULL;  /* filled in semantic pass */
        return n;
    }

    /* Literal */
    struct ast_node *lit = parse_literal_node(p);
    if (lit) return lit;

    parse_err(p, "Unexpected token '%s' in expression", t.text);
    return NULL;
}

static struct ast_node *parse_unary(parser_state *p) {
    token t = lexer_peek(&p->lex);
    if (t.type == TOK_MINUS) {
        lexer_consume(&p->lex);
        struct ast_node *operand = parse_unary(p);
        if (!operand) return NULL;
        /* Fold unary minus into literal if possible */
        if (operand->type == AST_LITERAL) {
            if (operand->data.literal.type == TYPE_INT)
                operand->data.literal.value.int_val = -operand->data.literal.value.int_val;
            else if (operand->data.literal.type == TYPE_REAL)
                operand->data.literal.value.real_val = -operand->data.literal.value.real_val;
            return operand;
        }
        /* Otherwise wrap in 0 - x */
        struct ast_node *zero = ast_node_alloc(AST_LITERAL);
        if (!zero) { ast_destroy(operand); return NULL; }
        zero->data.literal.type = TYPE_INT;
        zero->data.literal.value.int_val = 0LL;
        struct ast_node *sub = ast_node_alloc(AST_BINARY_OP);
        if (!sub) { ast_destroy(zero); ast_destroy(operand); return NULL; }
        sub->data.binary.op    = OP_SUB;
        sub->data.binary.left  = zero;
        sub->data.binary.right = operand;
        return sub;
    }
    return parse_primary(p);
}

static struct ast_node *parse_term(parser_state *p) {
    struct ast_node *left = parse_unary(p);
    if (!left) return NULL;

    for (;;) {
        token t = lexer_peek(&p->lex);
        enum binary_op op;
        if      (t.type == TOK_STAR)  op = OP_MUL;
        else if (t.type == TOK_SLASH) op = OP_DIV;
        else break;

        lexer_consume(&p->lex);
        struct ast_node *right = parse_unary(p);
        if (!right) { ast_destroy(left); return NULL; }

        struct ast_node *bin = ast_node_alloc(AST_BINARY_OP);
        if (!bin) { ast_destroy(left); ast_destroy(right); return NULL; }
        bin->data.binary.op    = op;
        bin->data.binary.left  = left;
        bin->data.binary.right = right;
        left = bin;
    }
    return left;
}

static struct ast_node *parse_expression(parser_state *p) {
    struct ast_node *left = parse_term(p);
    if (!left) return NULL;

    for (;;) {
        token t = lexer_peek(&p->lex);
        enum binary_op op;
        if      (t.type == TOK_PLUS)  op = OP_ADD;
        else if (t.type == TOK_MINUS) op = OP_SUB;
        else break;

        lexer_consume(&p->lex);
        struct ast_node *right = parse_term(p);
        if (!right) { ast_destroy(left); return NULL; }

        struct ast_node *bin = ast_node_alloc(AST_BINARY_OP);
        if (!bin) { ast_destroy(left); ast_destroy(right); return NULL; }
        bin->data.binary.op    = op;
        bin->data.binary.left  = left;
        bin->data.binary.right = right;
        left = bin;
    }
    return left;
}

/* Comparison — one optional relational operator wrapping an arithmetic expr.
 * Result type is logical (0/1 integer).  Comparisons are not chainable. */
static struct ast_node *parse_comparison(parser_state *p) {
    struct ast_node *left = parse_expression(p);
    if (!left || p->error) return left;

    token t = lexer_peek(&p->lex);
    enum binary_op op;
    switch (t.type) {
    case TOK_EQ: op = OP_EQ; break;
    case TOK_NE: op = OP_NE; break;
    case TOK_LT: op = OP_LT; break;
    case TOK_GT: op = OP_GT; break;
    case TOK_LE: op = OP_LE; break;
    case TOK_GE: op = OP_GE; break;
    default:     return left;   /* plain arithmetic expression */
    }

    lexer_consume(&p->lex);
    struct ast_node *right = parse_expression(p);
    if (!right) { ast_destroy(left); return NULL; }

    struct ast_node *bin = ast_node_alloc(AST_BINARY_OP);
    if (!bin) { ast_destroy(left); ast_destroy(right); return NULL; }
    bin->data.binary.op    = op;
    bin->data.binary.left  = left;
    bin->data.binary.right = right;
    return bin;
}

static struct ast_node *parse_statement(parser_state *p);

/* Parse a list of statements until ELSE, END, or EOF (does NOT consume
 * the terminator token — the caller is responsible for that). */
static struct ast_node *parse_stmt_list(parser_state *p) {
    struct ast_node *head = NULL;
    struct ast_node *tail = NULL;

    for (;;) {
        skip_newlines(&p->lex);
        if (p->error) break;

        token t = lexer_peek(&p->lex);
        if (t.type == TOK_EOF    ||
            t.type == TOK_KW_END ||
            t.type == TOK_KW_ELSE)
            break;

        struct ast_node *stmt = parse_statement(p);
        if (p->error) { ast_destroy(head); return NULL; }
        if (!stmt) continue;

        /* Consume trailing newline */
        if (lexer_peek(&p->lex).type == TOK_NEWLINE)
            lexer_consume(&p->lex);

        if (!head) { head = stmt; tail = stmt; }
        else       { tail->next = stmt; tail = stmt; }
    }
    return head;
}

/* IF (condition) THEN
 *   [statements]
 * [ELSE
 *   [statements]]
 * END IF                                                              */
static struct ast_node *parse_if_stmt(parser_state *p) {
    lexer_consume(&p->lex);  /* consume IF */

    expect(p, TOK_LPAREN, "'('");
    if (p->error) return NULL;

    struct ast_node *cond = parse_comparison(p);
    if (!cond || p->error) return NULL;

    expect(p, TOK_RPAREN, "')'");
    if (p->error) { ast_destroy(cond); return NULL; }

    /* Consume THEN */
    token th = lexer_consume(&p->lex);
    if (th.type != TOK_KW_THEN) {
        parse_err(p, "Expected THEN after IF condition, got '%s'", th.text);
        ast_destroy(cond);
        return NULL;
    }

    /* Skip to body */
    while (lexer_peek(&p->lex).type == TOK_NEWLINE)
        lexer_consume(&p->lex);

    struct ast_node *then_body = parse_stmt_list(p);
    if (p->error) { ast_destroy(cond); return NULL; }

    struct ast_node *else_body = NULL;
    if (lexer_peek(&p->lex).type == TOK_KW_ELSE) {
        lexer_consume(&p->lex);  /* consume ELSE */
        while (lexer_peek(&p->lex).type == TOK_NEWLINE)
            lexer_consume(&p->lex);
        else_body = parse_stmt_list(p);
        if (p->error) {
            ast_destroy(cond); ast_destroy(then_body);
            return NULL;
        }
    }

    /* Consume END IF */
    token end_tok = lexer_consume(&p->lex);  /* END */
    if (end_tok.type != TOK_KW_END) {
        parse_err(p, "Expected END IF, got '%s'", end_tok.text);
        ast_destroy(cond); ast_destroy(then_body); ast_destroy(else_body);
        return NULL;
    }
    if (lexer_peek(&p->lex).type == TOK_KW_IF)
        lexer_consume(&p->lex);  /* optional IF after END */

    struct ast_node *n = ast_node_alloc(AST_IF);
    if (!n) {
        ast_destroy(cond); ast_destroy(then_body); ast_destroy(else_body);
        return NULL;
    }
    n->data.if_stmt.condition = cond;
    n->data.if_stmt.then_body = then_body;
    n->data.if_stmt.else_body = else_body;
    return n;
}

/* DO var = start, end [, step]
 *   [statements]
 * END DO                                                             */
static struct ast_node *parse_do_stmt(parser_state *p) {
    lexer_consume(&p->lex);  /* consume DO */

    token var_tok = expect(p, TOK_IDENT, "loop variable name");
    if (p->error) return NULL;

    /* DO uses plain '=' not ':=' */
    token eq_tok = lexer_consume(&p->lex);
    if (eq_tok.type != TOK_EQUALS) {
        parse_err(p, "Expected '=' in DO statement, got '%s'", eq_tok.text);
        return NULL;
    }

    struct ast_node *start = parse_expression(p);
    if (!start || p->error) return NULL;

    expect(p, TOK_COMMA, "','");
    if (p->error) { ast_destroy(start); return NULL; }

    struct ast_node *end = parse_expression(p);
    if (!end || p->error) { ast_destroy(start); return NULL; }

    /* Optional step */
    struct ast_node *step = NULL;
    if (lexer_peek(&p->lex).type == TOK_COMMA) {
        lexer_consume(&p->lex);
        step = parse_expression(p);
        if (!step || p->error) {
            ast_destroy(start); ast_destroy(end);
            return NULL;
        }
    }

    /* Skip to body */
    while (lexer_peek(&p->lex).type == TOK_NEWLINE)
        lexer_consume(&p->lex);

    struct ast_node *body = parse_stmt_list(p);
    if (p->error) {
        ast_destroy(start); ast_destroy(end); ast_destroy(step);
        return NULL;
    }

    /* Consume END DO */
    token end_tok = lexer_consume(&p->lex);  /* END */
    if (end_tok.type != TOK_KW_END) {
        parse_err(p, "Expected END DO, got '%s'", end_tok.text);
        ast_destroy(start); ast_destroy(end);
        ast_destroy(step); ast_destroy(body);
        return NULL;
    }
    if (lexer_peek(&p->lex).type == TOK_KW_DO)
        lexer_consume(&p->lex);  /* optional DO after END */

    struct ast_node *n = ast_node_alloc(AST_DO);
    if (!n) {
        ast_destroy(start); ast_destroy(end);
        ast_destroy(step); ast_destroy(body);
        return NULL;
    }
    n->data.do_loop.var_name   = strdup(var_tok.text);
    n->data.do_loop.start_expr = start;
    n->data.do_loop.end_expr   = end;
    n->data.do_loop.step_expr  = step;
    n->data.do_loop.body       = body;
    return n;
}

static enum type_kind token_to_type(token_type tt) {
    switch (tt) {
    case TOK_KW_INTEGER: return TYPE_INT;
    case TOK_KW_REAL:    return TYPE_REAL;
    case TOK_KW_STRING:  return TYPE_STRING;
    case TOK_KW_LOGICAL: return TYPE_LOGICAL;
    case TOK_KW_COMPLEX: return TYPE_COMPLEX;
    default:             return TYPE_UNKNOWN;
    }
}

static struct ast_node *parse_statement(parser_state *p) {
    skip_newlines(&p->lex);
    token t = lexer_peek(&p->lex);

    /* Explicit declaration: INTEGER x := expr */
    if (t.type == TOK_KW_INTEGER || t.type == TOK_KW_REAL   ||
        t.type == TOK_KW_STRING  || t.type == TOK_KW_LOGICAL ||
        t.type == TOK_KW_COMPLEX) {
        lexer_consume(&p->lex);
        enum type_kind decl_type = token_to_type(t.type);

        token name_tok = expect(p, TOK_IDENT, "variable name");
        if (p->error) return NULL;

        expect(p, TOK_ASSIGN_OP, "':='");
        if (p->error) return NULL;

        struct ast_node *val = parse_expression(p);
        if (!val || p->error) return NULL;

        struct ast_node *n = ast_node_alloc(AST_ASSIGN_EXPLICIT);
        if (!n) { ast_destroy(val); return NULL; }
        n->data.assign.var_name      = strdup(name_tok.text);
        n->data.assign.declared_type = decl_type;
        n->data.assign.value         = val;
        return n;
    }

    /* Implicit: ASSIGN x, expr */
    if (t.type == TOK_KW_ASSIGN) {
        lexer_consume(&p->lex);
        token name_tok = expect(p, TOK_IDENT, "variable name");
        if (p->error) return NULL;

        expect(p, TOK_COMMA, "','");
        if (p->error) return NULL;

        struct ast_node *val = parse_expression(p);
        if (!val || p->error) return NULL;

        struct ast_node *n = ast_node_alloc(AST_ASSIGN_IMPLICIT);
        if (!n) { ast_destroy(val); return NULL; }
        n->data.assign.var_name      = strdup(name_tok.text);
        n->data.assign.declared_type = TYPE_UNKNOWN;
        n->data.assign.value         = val;
        return n;
    }

    /* PRINT expr */
    if (t.type == TOK_KW_PRINT) {
        lexer_consume(&p->lex);
        struct ast_node *expr = parse_expression(p);
        if (!expr || p->error) return NULL;

        struct ast_node *n = ast_node_alloc(AST_PRINT);
        if (!n) { ast_destroy(expr); return NULL; }
        n->data.print.expr = expr;
        return n;
    }

    /* IF (condition) THEN ... [ELSE ...] END IF */
    if (t.type == TOK_KW_IF)
        return parse_if_stmt(p);

    /* DO i = start, end [, step] ... END DO */
    if (t.type == TOK_KW_DO)
        return parse_do_stmt(p);

    /* STOP */
    if (t.type == TOK_KW_STOP) {
        lexer_consume(&p->lex);
        return ast_node_alloc(AST_STOP);
    }

    /* END PROGRAM / END DO / END IF terminates the statement list */
    if (t.type == TOK_KW_END || t.type == TOK_EOF)
        return NULL;

    parse_err(p, "Unexpected token '%s' at start of statement", t.text);
    return NULL;
}

struct ast_node *parse_program(const char *filename, char **source_buffer) {
    if (!filename || !source_buffer || !*source_buffer) return NULL;

    parser_state p;
    lexer_init(&p.lex, *source_buffer, filename);
    p.filename = filename;
    p.error    = 0;

    skip_newlines(&p.lex);

    /* Expect: PROGRAM name */
    token kw = lexer_consume(&p.lex);
    if (kw.type != TOK_KW_PROGRAM) {
        struct error_context ctx = {filename, 1, 0, NULL};
        set_error_context(&ctx);
        slap("Expected PROGRAM keyword, got '%s'", kw.text);
        clear_error_context();
        return NULL;
    }

    token prog_name = expect(&p, TOK_IDENT, "program name");
    if (p.error) return NULL;

    /* Consume remainder of first line */
    while (lexer_peek(&p.lex).type != TOK_NEWLINE &&
           lexer_peek(&p.lex).type != TOK_EOF)
        lexer_consume(&p.lex);
    skip_newlines(&p.lex);

    /* Parse statement list until END PROGRAM or EOF */
    struct ast_node *head = parse_stmt_list(&p);
    if (p.error) return NULL;

    /* Consume END PROGRAM (if present) */
    skip_newlines(&p.lex);
    if (lexer_peek(&p.lex).type == TOK_KW_END) {
        lexer_consume(&p.lex);
        if (lexer_peek(&p.lex).type == TOK_KW_PROGRAM)
            lexer_consume(&p.lex);
    }

    struct ast_node *prog = ast_node_alloc(AST_PROGRAM);
    if (!prog) { ast_destroy(head); return NULL; }
    prog->data.program.name       = strdup(prog_name.text);
    prog->data.program.statements = head;
    return prog;
}

static int sem_expr(struct ast_node *node, struct symbol_table *table) {
    if (!node) return 0;
    switch (node->type) {
    case AST_VARIABLE: {
        struct symbol *sym = symbol_lookup(table, node->data.variable.name);
        if (!sym) {
            slap("Undefined variable '%s'", node->data.variable.name);
            return -1;
        }
        node->data.variable.symbol = sym;
        return 0;
    }
    case AST_BINARY_OP:
        if (sem_expr(node->data.binary.left,  table) < 0) return -1;
        if (sem_expr(node->data.binary.right, table) < 0) return -1;
        return 0;
    case AST_LITERAL:
        return 0;
    default:
        return 0;
    }
}

/* Pre-populate symbols from explicit declarations so forward refs work.
 * Recurses into IF/DO bodies to catch variables declared inside blocks. */
static void sem_collect_decls(struct ast_node *stmts, struct symbol_table *table) {
    for (struct ast_node *s = stmts; s; s = s->next) {
        if (s->type == AST_ASSIGN_EXPLICIT) {
            /* Insert with zero-value placeholder */
            symbol_insert(table, s->data.assign.var_name,
                          s->data.assign.declared_type, NULL);
        } else if (s->type == AST_ASSIGN_IMPLICIT) {
            /* Type will be inferred; use UNKNOWN for now */
            symbol_insert(table, s->data.assign.var_name,
                          TYPE_UNKNOWN, NULL);
        } else if (s->type == AST_DO) {
            /* Loop variable is implicitly INTEGER */
            symbol_insert(table, s->data.do_loop.var_name, TYPE_INT, NULL);
            sem_collect_decls(s->data.do_loop.body, table);
        } else if (s->type == AST_IF) {
            sem_collect_decls(s->data.if_stmt.then_body, table);
            sem_collect_decls(s->data.if_stmt.else_body, table);
        }
    }
}

int semantic_analysis(struct ast_node *ast, struct symbol_table *table) {
    if (!ast || ast->type != AST_PROGRAM) return -1;

    struct ast_node *stmts = ast->data.program.statements;

    /* First pass: collect all declared variable names */
    sem_collect_decls(stmts, table);

    /* Second pass: resolve expressions */
    int result = 0;
    for (struct ast_node *s = stmts; s; s = s->next) {
        switch (s->type) {
        case AST_ASSIGN_EXPLICIT:
        case AST_ASSIGN_IMPLICIT:
            if (sem_expr(s->data.assign.value, table) < 0)
                result = -1;
            /* Update symbol type from explicit decl */
            if (s->type == AST_ASSIGN_EXPLICIT) {
                struct symbol *sym = symbol_lookup(table, s->data.assign.var_name);
                if (sym) sym->type = s->data.assign.declared_type;
            }
            break;
        case AST_PRINT:
            if (sem_expr(s->data.print.expr, table) < 0)
                result = -1;
            break;
        case AST_IF:
            if (sem_expr(s->data.if_stmt.condition, table) < 0)
                result = -1;
            /* Recurse into bodies — collect decls first */
            sem_collect_decls(s->data.if_stmt.then_body, table);
            sem_collect_decls(s->data.if_stmt.else_body, table);
            {
                struct ast_node *sub;
                for (sub = s->data.if_stmt.then_body; sub; sub = sub->next) {
                    switch (sub->type) {
                    case AST_ASSIGN_EXPLICIT:
                    case AST_ASSIGN_IMPLICIT:
                        sem_expr(sub->data.assign.value, table); break;
                    case AST_PRINT:
                        sem_expr(sub->data.print.expr, table); break;
                    default: break;
                    }
                }
                for (sub = s->data.if_stmt.else_body; sub; sub = sub->next) {
                    switch (sub->type) {
                    case AST_ASSIGN_EXPLICIT:
                    case AST_ASSIGN_IMPLICIT:
                        sem_expr(sub->data.assign.value, table); break;
                    case AST_PRINT:
                        sem_expr(sub->data.print.expr, table); break;
                    default: break;
                    }
                }
            }
            break;
        case AST_DO: {
            /* Ensure loop var is in symbol table */
            symbol_insert(table, s->data.do_loop.var_name, TYPE_INT, NULL);
            sem_expr(s->data.do_loop.start_expr, table);
            sem_expr(s->data.do_loop.end_expr,   table);
            if (s->data.do_loop.step_expr)
                sem_expr(s->data.do_loop.step_expr, table);
            struct ast_node *sub;
            for (sub = s->data.do_loop.body; sub; sub = sub->next) {
                switch (sub->type) {
                case AST_ASSIGN_EXPLICIT:
                case AST_ASSIGN_IMPLICIT:
                    sem_expr(sub->data.assign.value, table); break;
                case AST_PRINT:
                    sem_expr(sub->data.print.expr, table); break;
                default: break;
                }
            }
            break;
        }
        case AST_STOP:
            break;
        default:
            break;
        }
    }
    return result;
}
