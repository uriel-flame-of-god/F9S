/**
 * @file load.c
 * @brief REPL `.load` and `.build` command implementations.
 *
 * Parses dot-commands entered in the REPL, drives the compilation pipeline,
 * and optionally executes the resulting binary.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"
#include "load.h"
#include "log.h"
#include "symbol.h"

/** @brief Lazily-initialised symbol table shared across REPL compilations. */
static struct symbol_table *g_symbol_table = NULL;

static int is_absolute_path(const char *path) {
    if (!path || !*path) return 0;
    if (path[0] == '\\' || path[0] == '/') return 1;
    return isalpha((unsigned char)path[0]) && path[1] == ':';
}

static void normalize_path_separators(const char *in, char *out, size_t out_size) {
    if (!out || out_size == 0) return;
    size_t i = 0;
    if (!in) {
        out[0] = '\0';
        return;
    }
    for (; in[i] && i + 1 < out_size; i++)
        out[i] = (in[i] == '/') ? '\\' : in[i];
    out[i] = '\0';
}

/**
 * @brief Extract the filename argument from a dot-command line.
 *
 * Skips the command prefix and any leading whitespace.
 *
 * @param line    Full input line.
 * @param cmd     Command prefix (e.g. `".load"`).
 * @param cmd_len Length of `cmd`.
 * @return Pointer into `line` at the start of the filename, or NULL if
 *         the line does not start with `cmd` or has no filename argument.
 */
static const char *extract_filename(const char *line,
                                    const char *cmd,
                                    size_t      cmd_len) {
    if (strncmp(line, cmd, cmd_len) != 0) return NULL;
    const char *p = line + cmd_len;
    while (*p && isspace((unsigned char)*p)) p++;
    return (*p == '\0') ? NULL : p;
}

int handle_load_command(const char *line) {
    const char *filename = extract_filename(line, ".load", 5);
    if (!filename) {
        if (strncmp(line, ".load", 5) == 0) {
            log_error("Usage: .load <filename.f9s>");
            return -2;
        }
        return -1;
    }
    if (!g_symbol_table)
        g_symbol_table = symbol_table_create();
    return load_file(filename, g_symbol_table);
}

int handle_build_command(const char *line) {
    const char *filename = extract_filename(line, ".build", 6);
    if (!filename) {
        if (strncmp(line, ".build", 6) == 0) {
            log_error("Usage: .build <filename.f9s>");
            return -2;
        }
        return -1;
    }
    if (!g_symbol_table)
        g_symbol_table = symbol_table_create();
    return build_file(filename, g_symbol_table) < 0 ? -2 : 0;
}

int load_file(const char *filename, struct symbol_table *table) {
    char exe_path[256];

    if (compile_f9s(filename, table, exe_path, sizeof(exe_path)) < 0)
        return -2;

    char msg[256];
    snprintf(msg, sizeof(msg), "Running %s...", exe_path);
    log_info(msg);

    char exe_norm[256];
    normalize_path_separators(exe_path, exe_norm, sizeof(exe_norm));

    char run_path[300];
    if (!is_absolute_path(exe_norm) && exe_norm[0] != '.')
        snprintf(run_path, sizeof(run_path), ".\\%s", exe_norm);
    else
        snprintf(run_path, sizeof(run_path), "%s", exe_norm);

    char run_cmd[340];
    snprintf(run_cmd, sizeof(run_cmd), "\"%s\"", run_path);

    int result = system(run_cmd);
    if (result != 0) {
        snprintf(msg, sizeof(msg), "Program exited with code %d", result);
        log_warn(msg);
    }
    return 0;
}
