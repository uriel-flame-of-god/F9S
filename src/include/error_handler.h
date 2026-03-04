#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

enum error_severity {
    SEVERITY_PAT,   /* friendly warning — keeps going      */
    SEVERITY_SLAP   /* fatal error — aborts compilation    */
};

/* Optional source location attached to diagnostics */
struct error_context {
    const char *filename;
    int         line_number;
    int         column;
    const char *line_content;  /* full text of the offending line */
};

extern int g_pat_count;
extern int g_slap_count;

void pat(const char *format, ...);

/* Issue a fatal error.  Increments g_slap_count.
   Sets the internal slap flag so callers can check slap_occurred(). */
void slap(const char *format, ...);

/* Returns 1 if slap() has been called since the last reset */
int slap_occurred(void);

/* Reset both counters and the slap flag (call before each file load) */
void reset_diagnostics(void);

/* Print end-of-compilation pat summary (only if g_pat_count > 0) */
void print_pat_summary(void);

/* Attach / detach a source-location context for richer messages */
void set_error_context(const struct error_context *ctx);
void clear_error_context(void);
