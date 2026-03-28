/**
 * @file input.c
 * @brief Low-level terminal I/O helpers.
 *
 * Thin wrappers around `fgets` / `fputs` used by the REPL to read a line
 * from `stdin` and write a string to `stdout`.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include <string.h>

/**
 * @brief Read one line from `stdin` into `buf`, stripping the trailing newline.
 *
 * @param buf  Output buffer.
 * @param size Buffer capacity in bytes.
 * @return 1 on success, 0 on EOF or read error.
 */
int read_line(char *buf, int size);

/**
 * @brief Write a NUL-terminated string to `stdout` and flush.
 *
 * @param s String to write.
 */
void write_str(const char *s);

int read_line(char *buf, int size) {
    if (!fgets(buf, size, stdin)) return 0;
    size_t len = strlen(buf);
    if (len && buf[len - 1] == '\n') buf[len - 1] = '\0';
    return 1;
}

void write_str(const char *s) {
    fputs(s, stdout);
    fflush(stdout);
}
