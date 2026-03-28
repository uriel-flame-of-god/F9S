/**
 * @file log.c
 * @brief ANSI-colour terminal logging implementation.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>

/** @brief ANSI Control Sequence Introducer. */
#define CSI "\x1b["

void log_info(const char *msg) {
	printf(CSI "37;40m[LOG] %s" CSI "0m\n", msg);
}

void log_warn(const char *msg) {
	printf(CSI "33;40m[WARN] %s" CSI "0m\n", msg);
}

void log_error(const char *msg) {
	printf(CSI "31;40m[ERROR] %s" CSI "0m\n", msg);
}

void log_success(const char *msg) {
	printf(CSI "32;40m[SUCCESS] %s" CSI "0m\n", msg);
}
