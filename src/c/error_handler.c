/**
 * @file error_handler.c
 * @brief Pat/Slap diagnostic system implementation.
 *
 * `pat()` issues a warning; `slap()` records a fatal error.  Both accept
 * `printf`-style format strings and prepend an optional source-location
 * prefix when a context has been attached with `set_error_context()`.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "error_handler.h"
#include "log.h"

int g_pat_count  = 0;
int g_slap_count = 0;

static int                  s_slap_flag = 0;
static struct error_context s_ctx       = {0};
static int                  s_has_ctx   = 0;

/**
 * @brief Build a location prefix string (e.g. `"foo.f9s:12: "`).
 *
 * Writes an empty string into `buf` when no context is set.
 *
 * @param buf Output buffer.
 * @param cap Capacity of `buf` in bytes.
 */
static void build_location(char *buf, size_t cap) {
    if (!s_has_ctx || !s_ctx.filename) {
        buf[0] = '\0';
        return;
    }
    if (s_ctx.line_number > 0) {
        snprintf(buf, cap, "%s:%d: ", s_ctx.filename, s_ctx.line_number);
    } else {
        snprintf(buf, cap, "%s: ", s_ctx.filename);
    }
}

void pat(const char *format, ...) {
    g_pat_count++;

    char body[480];
    va_list ap;
    va_start(ap, format);
    vsnprintf(body, sizeof(body), format, ap);
    va_end(ap);

    char loc[128];
    build_location(loc, sizeof(loc));

    char msg[640];
    snprintf(msg, sizeof(msg), "[PAT] %s%s", loc, body);
    log_warn(msg);
}

void slap(const char *format, ...) {
    g_slap_count++;
    s_slap_flag = 1;

    char body[480];
    va_list ap;
    va_start(ap, format);
    vsnprintf(body, sizeof(body), format, ap);
    va_end(ap);

    char loc[128];
    build_location(loc, sizeof(loc));

    /* Show the offending source line when available */
    if (s_has_ctx && s_ctx.line_content && s_ctx.line_content[0]) {
        char src[256];
        snprintf(src, sizeof(src), "  --> %s", s_ctx.line_content);
        log_error(src);
    }

    char msg[640];
    snprintf(msg, sizeof(msg), "[SLAP] %s%s", loc, body);
    log_error(msg);
}

int slap_occurred(void) {
    return s_slap_flag;
}

void reset_diagnostics(void) {
    g_pat_count  = 0;
    g_slap_count = 0;
    s_slap_flag  = 0;
}

void print_pat_summary(void) {
    if (g_pat_count > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Gave %d pat%s. You're welcome.",
                 g_pat_count, g_pat_count == 1 ? "" : "s");
        log_info(msg);
    }
}

void set_error_context(const struct error_context *ctx) {
    if (ctx) {
        s_ctx     = *ctx;
        s_has_ctx = 1;
    }
}

void clear_error_context(void) {
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_has_ctx = 0;
}
