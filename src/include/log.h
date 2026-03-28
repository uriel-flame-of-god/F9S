/**
 * @file log.h
 * @brief ANSI-colour terminal logging helpers.
 *
 * Four severity levels map to distinct foreground colours so that compiler
 * output is easy to scan at a glance.  All functions write to `stderr`.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

/**
 * @brief Print an informational message (white / default colour).
 * @param msg NUL-terminated message string.
 */
void log_info(const char *msg);

/**
 * @brief Print a warning message (yellow).
 * @param msg NUL-terminated message string.
 */
void log_warn(const char *msg);

/**
 * @brief Print an error message (red).
 * @param msg NUL-terminated message string.
 */
void log_error(const char *msg);

/**
 * @brief Print a success message (green).
 * @param msg NUL-terminated message string.
 */
void log_success(const char *msg);
