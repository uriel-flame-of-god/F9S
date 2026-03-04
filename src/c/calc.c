// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "int.h"

static char *next_token(char **ctx) {
	char *s = *ctx;
	while (*s == ' ' || *s == '\t') s++;
	if (*s == '\0') {
		*ctx = s;
		return NULL;
	}
	char *start = s;
	while (*s && *s != ' ' && *s != '\t') s++;
	if (*s) {
		*s++ = '\0';
	}
	*ctx = s;
	return start;
}

int is_exit(const char *s) {
	if (!s) return 0;
	return strcmp(s, "exit") == 0 || strcmp(s, "quit") == 0;
}

static int parse_ll(const char *t, long long *v) {
	if (!t || !*t) return 0;
	char *end = NULL;
	long long val = strtoll(t, &end, 10);
	if (end && *end == '\0') {
		*v = val;
		return 1;
	}
	return 0;
}

int compute(const char *line, int features, long long *out) {
	if (!line || !*line) return 1;

	/*
	 * Allocate a working copy sized to the actual input length.
	 * Avoids any fixed-size stack buffer that could be overflowed if
	 * compute() is called from a context outside the 128-byte REPL buffer.
	 */
	size_t len = strlen(line);
	char *buf = (char *)malloc(len + 1);
	if (!buf) return 1;
	memcpy(buf, line, len + 1);

	for (size_t i = 0; i < len; ++i) {
		if (buf[i] == ',') buf[i] = ' ';
	}

	int rc = 1; /* default: unrecognized */
	long long a = 0;
	long long b = 0;

	/* try infix: A op B */
	char *p = buf;
	while (isspace((unsigned char)*p)) p++;
	char *end1 = NULL;
	a = strtoll(p, &end1, 10);
	if (end1 && end1 != p) {
		char *optr = end1;
		while (isspace((unsigned char)*optr)) optr++;
		char opch = *optr;
		if (opch == '+' || opch == '-' || opch == '*' || opch == '/') {
			optr++;
			while (isspace((unsigned char)*optr)) optr++;
			char *end2 = NULL;
			b = strtoll(optr, &end2, 10);
			if (end2 && end2 != optr) {
				while (isspace((unsigned char)*end2)) end2++;
				if (*end2 == '\0') {
					switch (opch) {
					case '+': *out = add_int(a, b);  rc = 0;  goto done;
					case '-': *out = sub_int(a, b);  rc = 0;  goto done;
					case '*': *out = mul_int(a, b);  rc = 0;  goto done;
					case '/':
						if (b == 0) { rc = -3; goto done; }
						*out = div_int(a, b); rc = 0; goto done;
					}
				}
			}
		}
	}

	/* fallback: verb first */
	{
		char *ctx = buf;
		char *op = next_token(&ctx);
		if (!op) goto done; /* rc = 1 */

		if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0 ||
		    strcmp(op, "mul") == 0 || strcmp(op, "mult") == 0 ||
		    strcmp(op, "div") == 0) {
			char *t1 = next_token(&ctx);
			char *t2 = next_token(&ctx);
			if (!parse_ll(t1, &a) || !parse_ll(t2, &b)) { rc = -4; goto done; }
			if (strcmp(op, "add") == 0)       *out = add_int(a, b);
			else if (strcmp(op, "sub") == 0)  *out = sub_int(a, b);
			else if (strcmp(op, "mul") == 0 || strcmp(op, "mult") == 0)
			                                  *out = mul_int(a, b);
			else {
				if (b == 0) { rc = -3; goto done; }
				*out = div_int(a, b);
			}
			rc = 0; goto done;
		}

		if (strcmp(op, "square") == 0) {
			char *t1 = next_token(&ctx);
			if (!parse_ll(t1, &a)) { rc = -4; goto done; }
			*out = square_int(a);
			rc = 0; goto done;
		}

		if (strcmp(op, "sqrt") == 0) {
			char *t1 = next_token(&ctx);
			if (!parse_ll(t1, &a)) { rc = -4; goto done; }
			if (a < 0) { rc = -5; goto done; }
			if (features & 1) {
				*out = sqrt_int(a);
			} else {
				*out = sqrt_int_fallback(a);
			}
			rc = 0; goto done;
		}
	}
	/* rc = 1: unrecognized verb */

done:
	free(buf);
	return rc;
}
