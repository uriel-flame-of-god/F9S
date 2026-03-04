// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

static const char *names[] = {
	"int",
	"character",
	"string",
	"real",
	"complex",
	"logical",
	"unknown"
};

static int equals_ci(const char *a, const char *b) {
	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
			return 0;
		}
		a++;
		b++;
	}
	return *a == *b;
}

static void trim_span(const char **s, size_t *len) {
	const char *p = *s;
	size_t n = *len;
	while (n && isspace((unsigned char)*p)) {
		p++;
		n--;
	}
	while (n && isspace((unsigned char)p[n - 1])) {
		n--;
	}
	*s = p;
	*len = n;
}

static int split_kind(const char *s, char *numeric, size_t cap) {
	const char *u = strrchr(s, '_');
	size_t nlen;
	if (u) {
		nlen = (size_t)(u - s);
	} else {
		nlen = strlen(s);
	}
	if (nlen + 1 > cap) {
		return 0;
	}
	memcpy(numeric, s, nlen);
	numeric[nlen] = '\0';
	return 1;
}

static int is_logical(const char *s) {
	return equals_ci(s, ".true.") || equals_ci(s, ".false.");
}

static int is_character(const char *s) {
	size_t len = strlen(s);
	if (len < 2) {
		return 0;
	}
	char q = s[0];
	if (!((q == '"' && s[len - 1] == '"') || (q == '\'' && s[len - 1] == '\''))) {
		return 0;
	}
	return 1;
}

int parse_string(const char *s, struct string_type *out) {
	if (!is_character(s) || !out) {
		return 0;
	}
	size_t len = strlen(s);
	if (len < 2) {
		return 0;
	}
	out->data = s + 1;
	out->length = len - 2;
	out->kind = 0;
	return 1;
}

static int is_integer(const char *s) {
	char buf[256];
	if (!split_kind(s, buf, sizeof(buf))) {
		return 0;
	}
	if (buf[0] == '\0') {
		return 0;
	}
	char *end = NULL;
	strtol(buf, &end, 10);
	return end && *end == '\0';
}

static int is_real_literal(const char *s) {
	char buf[256];
	if (!split_kind(s, buf, sizeof(buf))) {
		return 0;
	}
	if (!strpbrk(buf, ".eE")) {
		return 0;
	}
	char *end = NULL;
	strtod(buf, &end);
	return end && *end == '\0';
}

static int is_complex_literal(const char *s) {
	size_t len = strlen(s);
	if (len < 5) {
		return 0;
	}
	if (s[0] != '(' || s[len - 1] != ')') {
		return 0;
	}
	const char *comma = strchr(s, ',');
	if (!comma || comma == s + 1 || comma >= s + len - 2) {
		return 0;
	}
	char left[256];
	char right[256];
	size_t left_len = (size_t)(comma - s - 1);
	size_t right_len = (size_t)(s + len - 1 - comma - 1);
	if (left_len >= sizeof(left) || right_len >= sizeof(right)) {
		return 0;
	}
	memcpy(left, s + 1, left_len);
	left[left_len] = '\0';
	memcpy(right, comma + 1, right_len);
	right[right_len] = '\0';
	const char *lp = left;
	size_t llen = strlen(left);
	const char *rp = right;
	size_t rlen = strlen(right);
	trim_span(&lp, &llen);
	trim_span(&rp, &rlen);
	char lbuf[256];
	char rbuf[256];
	if (llen >= sizeof(lbuf) || rlen >= sizeof(rbuf)) {
		return 0;
	}
	memcpy(lbuf, lp, llen);
	lbuf[llen] = '\0';
	memcpy(rbuf, rp, rlen);
	rbuf[rlen] = '\0';
	return (is_real_literal(lbuf) || is_integer(lbuf)) && (is_real_literal(rbuf) || is_integer(rbuf));
}

int detect_type(const char *s) {
	if (is_logical(s)) {
		return TYPE_LOGICAL;
	}
	if (is_character(s)) {
		size_t len = strlen(s);
		size_t payload = len >= 2 ? len - 2 : 0;
		if (payload > 1) {
			return TYPE_STRING;
		}
		return TYPE_CHARACTER;
	}
	if (is_complex_literal(s)) {
		return TYPE_COMPLEX;
	}
	if (is_real_literal(s)) {
		return TYPE_REAL;
	}
	if (is_integer(s)) {
		return TYPE_INT;
	}
	return TYPE_UNKNOWN;
}

const char *type_name(int kind) {
	if (kind < 0 || kind >= (int)(sizeof(names) / sizeof(names[0]))) {
		return names[TYPE_UNKNOWN];
	}
	return names[kind];
}
