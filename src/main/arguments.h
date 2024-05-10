#pragma once

#include <stdbool.h>

// Command-line arguments.
// Always a pointer into `argv`, not allocated memory.
typedef struct Arguments {
	const char *address;
	const char *port;

	const char *serve_path;

	bool test;
	const char *fuzz;
} Arguments;

void arguments_parse(Arguments *self, int argc, const char **argv);
