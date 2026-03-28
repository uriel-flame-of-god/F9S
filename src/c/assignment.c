/**
 * @file assignment.c
 * @brief Type conversion and assignment helpers.
 *
 * Handles all cross-type conversions used by the semantic-analysis pass.
 * Lossy conversions (e.g. REAL → INTEGER) emit a `pat()` warning.
 * Incompatible conversions (e.g. STRING → LOGICAL) call `slap()` and
 * return -1 without modifying the destination.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "assignment.h"
#include "error_handler.h"
#include "symbol.h"
#include "types.h"

/** @brief Shorthand for the complex value struct used throughout this file. */
typedef struct { double real; double imag; } complex_val;

enum type_kind get_literal_type(const char *literal) {
    return (enum type_kind)detect_type(literal);
}

int convert_value(void          *dest,
                  enum type_kind  dest_type,
                  const void    *src,
                  enum type_kind  src_type,
                  int            *pat_count) {
    /* Identity — no conversion needed */
    if (src_type == dest_type) {
        switch (dest_type) {
        case TYPE_INT:
        case TYPE_LOGICAL:
            *(long long *)dest = *(const long long *)src;
            break;
        case TYPE_REAL:
            *(double *)dest = *(const double *)src;
            break;
        case TYPE_STRING:
        case TYPE_CHARACTER:
            strcpy((char *)dest, (const char *)src);
            break;
        case TYPE_COMPLEX:
            *(complex_val *)dest = *(const complex_val *)src;
            break;
        default:
            break;
        }
        return 0;
    }

    /* Extract source helpers for cross-type conversions */
    long long   src_int   = 0;
    double      src_real  = 0.0;
    long long   src_logic = 0;
    complex_val src_cx    = {0.0, 0.0};

    switch (src_type) {
    case TYPE_INT:
        src_int   = *(const long long *)src;
        src_real  = (double)src_int;
        src_logic = src_int ? 1LL : 0LL;
        src_cx.real = (double)src_int;
        break;
    case TYPE_REAL:
        src_real  = *(const double *)src;
        src_int   = (long long)src_real;
        src_logic = (src_real != 0.0) ? 1LL : 0LL;
        src_cx.real = src_real;
        break;
    case TYPE_LOGICAL:
        src_logic = *(const long long *)src ? 1LL : 0LL;
        src_int   = src_logic;
        src_real  = (double)src_logic;
        src_cx.real = src_real;
        break;
    case TYPE_COMPLEX:
        src_cx    = *(const complex_val *)src;
        src_real  = src_cx.real;
        src_int   = (long long)src_real;
        src_logic = (src_cx.real != 0.0 || src_cx.imag != 0.0) ? 1LL : 0LL;
        break;
    case TYPE_STRING:
    case TYPE_CHARACTER:
        /* Handled case-by-case below */
        break;
    default:
        break;
    }

    switch (dest_type) {

    /* =================== → INTEGER =================== */
    case TYPE_INT:
        switch (src_type) {
        case TYPE_REAL:
            pat("Lost fractional part converting REAL to INTEGER (%.6g → %lld)",
                src_real, src_int);
            if (pat_count) (*pat_count)++;
            *(long long *)dest = src_int;
            return 0;
        case TYPE_LOGICAL:
            pat("Converted LOGICAL to INTEGER (.%s. → %lld)",
                src_logic ? "TRUE" : "FALSE", src_logic);
            if (pat_count) (*pat_count)++;
            *(long long *)dest = src_logic;
            return 0;
        case TYPE_COMPLEX:
            pat("Discarded imaginary part converting COMPLEX to INTEGER "
                "(%.6g+%.6gi → %lld)",
                src_cx.real, src_cx.imag, src_int);
            if (pat_count) (*pat_count)++;
            *(long long *)dest = src_int;
            return 0;
        case TYPE_STRING:
        case TYPE_CHARACTER: {
            const char *s = (const char *)src;
            char *end     = NULL;
            errno         = 0;
            long long v   = strtoll(s, &end, 10);
            if (end && *end == '\0' && errno == 0) {
                pat("Parsed STRING \"%s\" as INTEGER (%lld)", s, v);
                if (pat_count) (*pat_count)++;
                *(long long *)dest = v;
                return 0;
            }
            slap("Cannot convert STRING \"%s\" to INTEGER", s);
            return -1;
        }
        default: break;
        }
        break;

    /* =================== → REAL =================== */
    case TYPE_REAL:
        switch (src_type) {
        case TYPE_INT:
            pat("Widened INTEGER %lld to REAL (%.6g)", src_int, (double)src_int);
            if (pat_count) (*pat_count)++;
            *(double *)dest = (double)src_int;
            return 0;
        case TYPE_LOGICAL:
            pat("Converted LOGICAL to REAL (.%s. → %.1f)",
                src_logic ? "TRUE" : "FALSE", (double)src_logic);
            if (pat_count) (*pat_count)++;
            *(double *)dest = (double)src_logic;
            return 0;
        case TYPE_COMPLEX:
            pat("Discarded imaginary part converting COMPLEX to REAL "
                "(%.6g+%.6gi → %.6g)",
                src_cx.real, src_cx.imag, src_cx.real);
            if (pat_count) (*pat_count)++;
            *(double *)dest = src_cx.real;
            return 0;
        case TYPE_STRING:
        case TYPE_CHARACTER: {
            const char *s = (const char *)src;
            char *end     = NULL;
            errno         = 0;
            double v      = strtod(s, &end);
            if (end && *end == '\0' && errno == 0) {
                pat("Parsed STRING \"%s\" as REAL (%.6g)", s, v);
                if (pat_count) (*pat_count)++;
                *(double *)dest = v;
                return 0;
            }
            slap("Cannot convert STRING \"%s\" to REAL", s);
            return -1;
        }
        default: break;
        }
        break;

    /* =================== → LOGICAL =================== */
    case TYPE_LOGICAL:
        switch (src_type) {
        case TYPE_INT:
            pat("Converted INTEGER %lld to LOGICAL (.%s.)",
                src_int, src_int ? "TRUE" : "FALSE");
            if (pat_count) (*pat_count)++;
            *(long long *)dest = src_int ? 1LL : 0LL;
            return 0;
        case TYPE_REAL:
            pat("Converted REAL %.6g to LOGICAL (.%s.)",
                src_real, src_real != 0.0 ? "TRUE" : "FALSE");
            if (pat_count) (*pat_count)++;
            *(long long *)dest = (src_real != 0.0) ? 1LL : 0LL;
            return 0;
        case TYPE_STRING:
        case TYPE_CHARACTER:
            slap("Cannot convert STRING \"%s\" to LOGICAL", (const char *)src);
            return -1;
        case TYPE_COMPLEX:
            pat("Converted COMPLEX (%.6g+%.6gi) to LOGICAL (.%s.)",
                src_cx.real, src_cx.imag,
                (src_cx.real != 0.0 || src_cx.imag != 0.0) ? "TRUE" : "FALSE");
            if (pat_count) (*pat_count)++;
            *(long long *)dest = src_logic;
            return 0;
        default: break;
        }
        break;

    /* =================== → COMPLEX =================== */
    case TYPE_COMPLEX:
        switch (src_type) {
        case TYPE_INT:
            pat("Widened INTEGER %lld to COMPLEX (%.6g+0.0i)",
                src_int, (double)src_int);
            if (pat_count) (*pat_count)++;
            ((complex_val *)dest)->real = (double)src_int;
            ((complex_val *)dest)->imag = 0.0;
            return 0;
        case TYPE_REAL:
            pat("Widened REAL %.6g to COMPLEX (%.6g+0.0i)", src_real, src_real);
            if (pat_count) (*pat_count)++;
            ((complex_val *)dest)->real = src_real;
            ((complex_val *)dest)->imag = 0.0;
            return 0;
        case TYPE_LOGICAL:
            pat("Converted LOGICAL .%s. to COMPLEX (%.1f+0.0i)",
                src_logic ? "TRUE" : "FALSE", (double)src_logic);
            if (pat_count) (*pat_count)++;
            ((complex_val *)dest)->real = (double)src_logic;
            ((complex_val *)dest)->imag = 0.0;
            return 0;
        case TYPE_STRING:
        case TYPE_CHARACTER:
            slap("Cannot convert STRING \"%s\" to COMPLEX", (const char *)src);
            return -1;
        default: break;
        }
        break;

    /* =================== → STRING / CHARACTER =================== */
    case TYPE_STRING:
    case TYPE_CHARACTER:
        slap("Cannot implicitly convert to STRING");
        return -1;

    default:
        break;
    }

    slap("Unsupported type conversion (%d → %d)", (int)src_type, (int)dest_type);
    return -1;
}

int handle_explicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               enum type_kind        declared_type,
                               void                *value,
                               enum type_kind        value_type) {
    if (!table || !var_name) return -1;

    union {
        long long  i;
        double     r;
        char       s[512];
        complex_val cx;
    } converted;
    memset(&converted, 0, sizeof(converted));

    void *final_value = value;
    int   pats        = 0;

    if (value_type != declared_type) {
        if (convert_value(&converted, declared_type, value, value_type, &pats) < 0)
            return -1;
        final_value = &converted;
    }

    if (!symbol_insert(table, var_name, declared_type, final_value)) {
        slap("Out of memory inserting variable '%s'", var_name);
        return -1;
    }
    return 0;
}

int handle_implicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               void                *value,
                               enum type_kind        value_type) {
    if (!table || !var_name) return -1;

    struct symbol *existing = symbol_lookup(table, var_name);

    if (existing && existing->type != value_type) {
        union {
            long long  i;
            double     r;
            char       s[512];
            complex_val cx;
        } converted;
        memset(&converted, 0, sizeof(converted));
        int pats = 0;

        if (convert_value(&converted, existing->type, value, value_type, &pats) < 0)
            return -1;

        if (!symbol_insert(table, var_name, existing->type, &converted)) {
            slap("Out of memory updating variable '%s'", var_name);
            return -1;
        }
        return 0;
    }

    if (!symbol_insert(table, var_name, value_type, value)) {
        slap("Out of memory inserting variable '%s'", var_name);
        return -1;
    }
    return 0;
}
