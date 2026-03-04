// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "load.h"
#include "build.h"
#include "symbol.h"
#include "log.h"

static struct symbol_table *g_symbol_table = NULL;

static const char *extract_filename(const char *line, const char *cmd, size_t cmd_len) {
    if (strncmp(line, cmd, cmd_len) != 0) return NULL;
    
    const char *p = line + cmd_len;
    while (*p && isspace((unsigned char)*p)) p++;
    
    if (*p == '\0') return NULL;  /* No filename */
    return p;
}

int handle_load_command(const char *line) {
    const char *filename = extract_filename(line, ".load", 5);
    if (!filename) {
        if (strncmp(line, ".load", 5) == 0) {
            log_error("Usage: .load <filename.f9s>");
            return -2;
        }
        return -1;  /* Not a .load command */
    }
    
    /* Create global symbol table if needed */
    if (!g_symbol_table) {
        g_symbol_table = symbol_table_create();
    }
    
    return load_file(filename, g_symbol_table);
}

/* ------------------------------------------------------------------ */
/*  handle_build_command - parse ".build <file>" and compile only    */
/*  Returns: 0 = handled OK, -1 = not a .build command, -2 = error   */
/* ------------------------------------------------------------------ */
int handle_build_command(const char *line) {
    const char *filename = extract_filename(line, ".build", 6);
    if (!filename) {
        if (strncmp(line, ".build", 6) == 0) {
            log_error("Usage: .build <filename.f9s>");
            return -2;
        }
        return -1;  /* Not a .build command */
    }
    
    /* Create global symbol table if needed */
    if (!g_symbol_table) {
        g_symbol_table = symbol_table_create();
    }
    
    return build_file(filename, g_symbol_table) < 0 ? -2 : 0;
}

/* ------------------------------------------------------------------ */
/*  load_file - compile and run                                      */
/* ------------------------------------------------------------------ */
int load_file(const char *filename, struct symbol_table *table) {
    char exe_path[256];
    
    if (compile_f9s(filename, table, exe_path, sizeof(exe_path)) < 0) {
        return -2;
    }

    /* Execute the compiled program */
    char msg[256];
    snprintf(msg, sizeof(msg), "Running %s...", exe_path);
    log_info(msg);
    
    int result = system(exe_path);
    
    if (result != 0) {
        snprintf(msg, sizeof(msg), "Program exited with code %d", result);
        log_warn(msg);
    }
    
    return 0;
}
