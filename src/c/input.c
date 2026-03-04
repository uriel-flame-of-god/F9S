// F9S - FORTRAN 95 Subset Compiler
// Copyright (C) 2026 Debaditya Malakar
// SPDX-License-Identifier: AGPL-3.0-only

#include <stdio.h>
#include <string.h>

int read_line(char *buf, int size);
void write_str(const char *s);

int read_line(char *buf, int size) {
	if (!fgets(buf, size, stdin)) {
		return 0;
	}
	size_t len = strlen(buf);
	if (len && buf[len - 1] == '\n') {
		buf[len - 1] = '\0';
	}
	return 1;
}

void write_str(const char *s) {
	fputs(s, stdout);
	fflush(stdout);
}
