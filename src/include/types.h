/**
 * @file types.h
 * @brief F9S type system: type kinds, string representation, and detection.
 *
 * The `type_kind` enum is the canonical type tag used throughout the compiler.
 * `detect_type` identifies the type of a raw source string (used by the REPL
 * calculator and the type-conversion layer).
 */

// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <stddef.h>

/** @brief All value types supported by F9S. */
enum type_kind {
    TYPE_INT,       /**< 64-bit signed integer.                   */
    TYPE_CHARACTER, /**< Single character.                        */
    TYPE_STRING,    /**< NUL-terminated character string.         */
    TYPE_REAL,      /**< 64-bit IEEE 754 double-precision float.  */
    TYPE_COMPLEX,   /**< Pair of doubles (real, imaginary).       */
    TYPE_LOGICAL,   /**< Boolean: 0 = .FALSE., non-zero = .TRUE.  */
    TYPE_UNKNOWN    /**< Type not yet inferred.                   */
};

/**
 * @brief Parsed representation of a string literal.
 * @see parse_string
 */
struct string_type {
    const char *data;    /**< Pointer into the source buffer.  */
    size_t      length;  /**< Number of characters.            */
    int         kind;    /**< Character kind (1 = default).    */
};

/**
 * @brief Detect the type of a raw source token string.
 *
 * Recognises integer, real, complex, string, logical literals.
 *
 * @param s Source string to classify.
 * @return `type_kind` value; TYPE_STRING if no other type matches.
 */
int detect_type(const char *s);

/**
 * @brief Return a human-readable name for a type kind.
 * @param kind `type_kind` cast to int.
 * @return Constant string such as `"INTEGER"`, `"REAL"`, etc.
 */
const char *type_name(int kind);

/**
 * @brief Parse a string literal token into a `string_type`.
 * @param s   Source string (must include surrounding quotes).
 * @param out Output struct to fill.
 * @return 0 on success, -1 on malformed input.
 */
int parse_string(const char *s, struct string_type *out);
