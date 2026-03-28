/**
 * @file types.c
 * @brief Type detection, naming, and string-literal parsing.
 *
 * `detect_type` classifies a raw source token by inspecting its syntax.
 * The precedence order is: LOGICAL → CHARACTER/STRING → COMPLEX → REAL → INT.
 * Anything that does not match any pattern returns `TYPE_UNKNOWN`.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

/** @brief Human-readable names indexed by `type_kind`. */
static const char *names[] = {
    "int",
    "character",
    "string",
    "real",
    "complex",
    "logical",
    "unknown"
};

/**
 * @brief Case-insensitive string equality.
 *
 * @param a First string.
 * @param b Second string.
 * @return 1 if equal (ignoring case), 0 otherwise.
 */
static int equals_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

/**
 * @brief Trim leading and trailing whitespace from a string span.
 *
 * @param s   Pointer to the string pointer (updated in place).
 * @param len Pointer to the length (updated in place).
 */
static void trim_span(const char **s, size_t *len) {
    const char *p = *s;
    size_t n = *len;
    while (n && isspace((unsigned char)*p)) { p++; n--; }
    while (n && isspace((unsigned char)p[n - 1])) n--;
    *s = p; *len = n;
}

/**
 * @brief Strip an optional FORTRAN kind suffix (e.g. `1.0_8` → `"1.0"`).
 *
 * Finds the last `_` and copies everything before it into `numeric`.
 *
 * @param s       Source token.
 * @param numeric Output buffer for the numeric portion.
 * @param cap     Capacity of `numeric`.
 * @return 1 on success, 0 if `cap` is too small.
 */
static int split_kind(const char *s, char *numeric, size_t cap) {
    const char *u = strrchr(s, '_');
    size_t nlen = u ? (size_t)(u - s) : strlen(s);
    if (nlen + 1 > cap) return 0;
    memcpy(numeric, s, nlen);
    numeric[nlen] = '\0';
    return 1;
}

/** @brief Return 1 if `s` is `.TRUE.` or `.FALSE.` (case-insensitive). */
static int is_logical(const char *s) {
    return equals_ci(s, ".true.") || equals_ci(s, ".false.");
}

/**
 * @brief Return 1 if `s` is a quoted character literal (`"…"` or `'…'`).
 *
 * The opening and closing quote characters must match.
 */
static int is_character(const char *s) {
    size_t len = strlen(s);
    if (len < 2) return 0;
    char q = s[0];
    return (q == '"'  && s[len - 1] == '"') ||
           (q == '\'' && s[len - 1] == '\'');
}

int parse_string(const char *s, struct string_type *out) {
    if (!is_character(s) || !out) return 0;
    size_t len = strlen(s);
    if (len < 2) return 0;
    out->data   = s + 1;
    out->length = len - 2;
    out->kind   = 0;
    return 1;
}

/** @brief Return 1 if `s` represents a decimal integer (with optional kind). */
static int is_integer(const char *s) {
    char buf[256];
    if (!split_kind(s, buf, sizeof(buf)) || buf[0] == '\0') return 0;
    char *end = NULL;
    strtol(buf, &end, 10);
    return end && *end == '\0';
}

/** @brief Return 1 if `s` is a floating-point literal (must contain `.`, `e`, or `E`). */
static int is_real_literal(const char *s) {
    char buf[256];
    if (!split_kind(s, buf, sizeof(buf))) return 0;
    if (!strpbrk(buf, ".eE")) return 0;
    char *end = NULL;
    strtod(buf, &end);
    return end && *end == '\0';
}

/**
 * @brief Return 1 if `s` is a FORTRAN complex literal `(real, real)`.
 *
 * Each component may be an integer or real literal.
 */
static int is_complex_literal(const char *s) {
    size_t len = strlen(s);
    if (len < 5 || s[0] != '(' || s[len - 1] != ')') return 0;
    const char *comma = strchr(s, ',');
    if (!comma || comma == s + 1 || comma >= s + len - 2) return 0;

    char left[256], right[256];
    size_t left_len  = (size_t)(comma - s - 1);
    size_t right_len = (size_t)(s + len - 1 - comma - 1);
    if (left_len >= sizeof(left) || right_len >= sizeof(right)) return 0;

    memcpy(left,  s + 1,       left_len);  left[left_len]   = '\0';
    memcpy(right, comma + 1,   right_len); right[right_len] = '\0';

    const char *lp = left,  *rp = right;
    size_t      ll = left_len, rl = right_len;
    trim_span(&lp, &ll);
    trim_span(&rp, &rl);

    char lbuf[256], rbuf[256];
    if (ll >= sizeof(lbuf) || rl >= sizeof(rbuf)) return 0;
    memcpy(lbuf, lp, ll); lbuf[ll] = '\0';
    memcpy(rbuf, rp, rl); rbuf[rl] = '\0';

    return (is_real_literal(lbuf) || is_integer(lbuf)) &&
           (is_real_literal(rbuf) || is_integer(rbuf));
}

int detect_type(const char *s) {
    if (is_logical(s))         return TYPE_LOGICAL;
    if (is_character(s)) {
        size_t len     = strlen(s);
        size_t payload = len >= 2 ? len - 2 : 0;
        return payload > 1 ? TYPE_STRING : TYPE_CHARACTER;
    }
    if (is_complex_literal(s)) return TYPE_COMPLEX;
    if (is_real_literal(s))    return TYPE_REAL;
    if (is_integer(s))         return TYPE_INT;
    return TYPE_UNKNOWN;
}

const char *type_name(int kind) {
    if (kind < 0 || kind >= (int)(sizeof(names) / sizeof(names[0])))
        return names[TYPE_UNKNOWN];
    return names[kind];
}
