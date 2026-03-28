/**
 * @file error_handler.h
 * @brief Pat/Slap diagnostic system for F9S.
 *
 * F9S uses two diagnostic levels inspired by FORTRAN tradition:
 *  - **pat** — a friendly warning that allows compilation to continue.
 *  - **slap** — a fatal error that marks compilation as failed.
 *
 * Both functions accept `printf`-style format strings.  After compilation,
 * call `print_pat_summary()` to report the total warning count.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

/** @brief Diagnostic severity levels. */
enum error_severity {
    SEVERITY_PAT,   /**< Friendly warning — compilation continues. */
    SEVERITY_SLAP   /**< Fatal error — compilation is aborted.     */
};

/**
 * @brief Optional source-location context for richer diagnostic messages.
 *
 * Attach with `set_error_context()` before issuing diagnostics, then
 * detach with `clear_error_context()` when the context is no longer valid.
 */
struct error_context {
    const char *filename;      /**< Source file path.                  */
    int         line_number;   /**< 1-based line number.               */
    int         column;        /**< 1-based column number.             */
    const char *line_content;  /**< Full text of the offending line.   */
};

/** @brief Running count of `pat()` calls since the last `reset_diagnostics()`. */
extern int g_pat_count;

/** @brief Running count of `slap()` calls since the last `reset_diagnostics()`. */
extern int g_slap_count;

/**
 * @brief Issue a warning (`printf`-style).
 *
 * Increments `g_pat_count`.  Compilation continues after a `pat()`.
 *
 * @param format `printf`-compatible format string, followed by arguments.
 */
void pat(const char *format, ...);

/**
 * @brief Issue a fatal error (`printf`-style).
 *
 * Increments `g_slap_count` and sets the internal slap flag so that
 * `slap_occurred()` returns 1.
 *
 * @param format `printf`-compatible format string, followed by arguments.
 */
void slap(const char *format, ...);

/**
 * @brief Check whether `slap()` has been called since the last reset.
 *
 * @return 1 if at least one fatal error was issued, 0 otherwise.
 */
int slap_occurred(void);

/**
 * @brief Reset both diagnostic counters and the slap flag.
 *
 * Should be called before each file compilation.
 */
void reset_diagnostics(void);

/**
 * @brief Print the end-of-compilation warning summary.
 *
 * Outputs a human-readable count of warnings.  No-ops if `g_pat_count` is 0.
 */
void print_pat_summary(void);

/**
 * @brief Attach a source-location context for subsequent diagnostics.
 *
 * @param ctx Pointer to a context struct (not copied; must remain valid until
 *            `clear_error_context()` is called).
 */
void set_error_context(const struct error_context *ctx);

/**
 * @brief Clear the current source-location context.
 *
 * After this call, diagnostics are emitted without a location prefix.
 */
void clear_error_context(void);
