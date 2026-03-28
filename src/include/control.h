/**
 * @file control.h
 * @brief Unique label allocator for code-generation control flow.
 *
 * Provides a simple monotonic counter used to generate unique branch labels
 * (e.g. `_L0`, `_L1`, …) during the code-generation pass.  Labels must be
 * unique within an assembled translation unit.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

/**
 * @brief Allocate the next unique label index.
 *
 * The caller is responsible for formatting the label string, e.g.:
 * @code
 *   int id = label_alloc();
 *   fprintf(out, "_L%d:\n", id);
 * @endcode
 *
 * @return Next integer label index (starts at 0, increments by 1).
 */
int label_alloc(void);

/**
 * @brief Reset the label counter to zero.
 *
 * Must be called before each file compilation so that label indices
 * start fresh and do not collide with a previous translation unit.
 */
void label_reset(void);
