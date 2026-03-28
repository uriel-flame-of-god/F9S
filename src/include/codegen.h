/**
 * @file codegen.h
 * @brief NASM x64 code-generation interface for F9S.
 *
 * `generate_code` is the single entry point for the code-generation phase.
 * It consumes a fully analysed AST and writes a Windows x64 NASM source file
 * that can be assembled with `nasm -f win64` and linked with `clang -m64`.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <stdio.h>
#include "ast.h"
#include "symbol.h"

/**
 * @brief Internal state carried through the code-generation pass.
 *
 * Callers do not need to allocate or inspect this struct directly;
 * it is created and destroyed inside `generate_code`.
 */
struct codegen_context {
    FILE               *output;        /**< Open handle to the destination `.asm` file. */
    struct symbol_table *table;        /**< Populated symbol table from semantic analysis. */
    int                 label_counter; /**< Monotonically increasing counter for unique labels. */
};

/**
 * @brief Generate a NASM x64 assembly file from a parsed, analysed AST.
 *
 * Performs three sub-passes in order:
 *  1. **Data pass** – emit `.data` section (real constants, string literals).
 *  2. **BSS pass**  – emit `.bss` section (scalar vars, arrays, hashmaps).
 *  3. **Text pass** – emit `.text` section (`_main` + all statement handlers).
 *
 * @param ast             Root `AST_PROGRAM` node from `parse_program`.
 * @param output_filename Path for the `.asm` file to create.
 * @return 0 on success, -1 on I/O or generation error.
 */
int generate_code(struct ast_node *ast, const char *output_filename);
