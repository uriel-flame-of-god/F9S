/**
 * @file int.h
 * @brief Integer arithmetic primitives (NASM-backed with C fallbacks).
 *
 * Basic arithmetic operations implemented in NASM (`src/asm/int.asm`) with
 * portable C fallbacks (`src/c/int_fallback.c`).  Used by the REPL
 * calculator and the interpreter layer.
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

/**
 * @brief Add two 64-bit signed integers.
 * @param a Left operand.
 * @param b Right operand.
 * @return `a + b`
 */
long long add_int(long long a, long long b);

/**
 * @brief Subtract two 64-bit signed integers.
 * @param a Left operand.
 * @param b Right operand.
 * @return `a - b`
 */
long long sub_int(long long a, long long b);

/**
 * @brief Multiply two 64-bit signed integers.
 * @param a Left operand.
 * @param b Right operand.
 * @return `a * b`
 */
long long mul_int(long long a, long long b);

/**
 * @brief Divide two 64-bit signed integers (truncating).
 * @param a Dividend.
 * @param b Divisor (must be non-zero).
 * @return `a / b`
 */
long long div_int(long long a, long long b);

/**
 * @brief Square a 64-bit signed integer.
 * @param a Value to square.
 * @return `a * a`
 */
long long square_int(long long a);

/**
 * @brief Compute the integer square root (NASM implementation).
 * @param a Non-negative value.
 * @return Floor of the square root of `a`.
 */
long long sqrt_int(long long a);

/**
 * @brief Portable C fallback for integer square root.
 *
 * Used when the NASM implementation is not available.
 *
 * @param a Non-negative value.
 * @return Floor of the square root of `a`.
 */
long long sqrt_int_fallback(long long a);
