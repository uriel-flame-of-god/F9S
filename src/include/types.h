#pragma once
// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stddef.h>

enum type_kind {
	TYPE_INT,
	TYPE_CHARACTER,
	TYPE_STRING,
	TYPE_REAL,
	TYPE_COMPLEX,
	TYPE_LOGICAL,
	TYPE_UNKNOWN
};

struct string_type {
	const char *data;
	size_t length;
	int kind;
};

int detect_type(const char *s);
const char *type_name(int kind);
int parse_string(const char *s, struct string_type *out);
