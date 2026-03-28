/**
 * @file build.c
 * @brief Compilation pipeline: parse → semantic analysis → codegen → assemble → link.
 *
 * `compile_f9s` is the single backend that drives every compilation.
 * It reads the source file into memory, runs the parser, semantic analyser,
 * and code generator, then shells out to NASM and Clang to produce a
 * native Windows x64 executable.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"
#include "codegen.h"
#include "error_handler.h"
#include "log.h"
#include "parser.h"
#include "symbol.h"

int compile_f9s(const char *filename,
                struct symbol_table *table,
                char *exe_path_out,
                size_t exe_path_size) {
    if (!filename || !table) return -1;

    reset_diagnostics();

    FILE *f = fopen(filename, "rb");
    if (!f) {
        slap("Cannot open file: %s", filename);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = (char *)malloc(size + 2);
    if (!buffer) {
        fclose(f);
        slap("Out of memory loading file");
        return -1;
    }

    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);
    /* Ensure the source ends with a newline so the lexer always terminates */
    buffer[read_size]     = '\n';
    buffer[read_size + 1] = '\0';

    struct ast_node *ast = parse_program(filename, &buffer);
    if (!ast || slap_occurred()) {
        free(buffer);
        return -1;
    }

    if (semantic_analysis(ast, table) < 0 || slap_occurred()) {
        ast_destroy(ast);
        free(buffer);
        return -1;
    }

    /* Derive output file names by stripping the .f9s extension */
    char asm_name[256], obj_name[256], exe_name[256];
    int base_len = (int)(strlen(filename) - 4);
    if (base_len < 0) base_len = (int)strlen(filename);

    snprintf(asm_name, sizeof(asm_name), "%.*s.asm", base_len, filename);
    snprintf(obj_name, sizeof(obj_name), "%.*s.obj", base_len, filename);
    snprintf(exe_name, sizeof(exe_name), "%.*s.exe", base_len, filename);

    if (generate_code(ast, asm_name) < 0) {
        slap("Code generation failed");
        ast_destroy(ast);
        free(buffer);
        return -1;
    }

    /* Assemble with NASM */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "nasm -f win64 %s -o %s 2>nul", asm_name, obj_name);
    if (system(cmd) != 0) {
        slap("Assembly failed");
        ast_destroy(ast);
        free(buffer);
        return -1;
    }

    /* Link with Clang */
    snprintf(cmd, sizeof(cmd), "clang -m64 %s -o %s 2>nul", obj_name, exe_name);
    if (system(cmd) != 0) {
        slap("Linking failed");
        ast_destroy(ast);
        free(buffer);
        return -1;
    }

    if (exe_path_out && exe_path_size > 0)
        snprintf(exe_path_out, exe_path_size, "%s", exe_name);

    char msg[256];
    snprintf(msg, sizeof(msg), "Compiled %s -> %s", filename, exe_name);
    log_success(msg);
    print_pat_summary();

    ast_destroy(ast);
    free(buffer);
    return 0;
}

int build_file(const char *filename, struct symbol_table *table) {
    return compile_f9s(filename, table, NULL, 0);
}
