#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include "ast.h"
#include "symbol.h"

struct codegen_context {
    FILE              *output;        /* open handle to destination .asm file */
    struct symbol_table *table;       /* populated symbol table                */
    int                label_counter; /* monotonic counter for unique labels   */
};

int generate_code(struct ast_node *ast, const char *output_filename);
