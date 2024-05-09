#include "arguments.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Returns true if `arg` is equal to `match`.
static bool match(const char *arg, const char *match) {
	if (strcmp(arg, match) != 0) {
		return false;
	}

	return true;
}

// Returns the suffix if `arg` starts with `prefix`.
static const char *remove_prefix(const char *arg, const char *prefix) {
	size_t index = 0;
	while (true) {
		// Every character up to now has matched.
		// `arg + index` may be a zero-length string.
		if (prefix[index] == '\0') {
			return arg + index;
		}

		// `arg` is shorter than `prefix`
		if (arg[index] == '\0') return NULL;

		// `arg` doesn't start with `prefix`.
		// Could be collapsed into the above check, but it's clearer this way.
		if (arg[index] != prefix[index]) return NULL;

		index++;
	}
}

static void print_usage(const char *argv0) {
	fprintf(stderr, "userve %s\n", USERVE_VERSION);
	fprintf(stderr, "usage: %s [--address <address>] [--port <port>]\n", argv0);

	fprintf(stderr, "\n");
	fprintf(stderr, "options:\n");

	fprintf(stderr, "\t-a [address], --address [address]\n");
	fprintf(stderr, "\t\tlisten for connections on address [address] (default: localhost)\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t-p [port], --port [port]\n");
	fprintf(stderr, "\t\tlisten on port [port] (default: 3000)\n");
	fprintf(stderr, "\t\tnote: if listening on [port] fails, userve will try up to five sequential ports above that port\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t-t, --test\n");
	fprintf(stderr, "\t\trun tests (very very wip)\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t-f, --fuzz\n");
	fprintf(stderr, "\t\trun fuzzer (very very wip)\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t-h, --help, -?\n");
	fprintf(stderr, "\t\tshow this help\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "\t-v, --version\n");
	fprintf(stderr, "\t\tshow version\n");
}

void arguments_parse(Arguments *self, int argc, char **argv) {
	// Defaults
	*self = (Arguments) {
		.address = "localhost",
		.port = "3000",

		.test = false,
		.fuzz = false,
	};

	for (int i = 1; i < argc; i++) {
		const char *arg = argv[i];

		const char *parsed = NULL;

		// --address [address], -a [address]
		if (match(arg, "--address") || match(arg, "-a")) {
			i++;
			if (i >= argc) break;

			self->address = arg;

		// --address=[port]
		} else if ((parsed = remove_prefix("--address=", arg)) != NULL) {
			self->address = arg;

		// --port [port], -p [port]
		} else if (match(arg, "--port") || match(arg, "-p")) {
			i++;
			if (i >= argc) break;

			self->address = arg;

		// --port=[port]
		} else if ((parsed = remove_prefix("--port=", arg)) != NULL) {
			self->port = arg;

		} else if (match(arg, "-t") || match(arg, "--test")) {
			self->test = true;

		} else if (match(arg, "-f") || match(arg, "--fuzz")) {
			self->fuzz = true;

		} else if (match(arg, "-h") || match(arg, "--help") || match(arg, "-?")) {
			fprintf(stderr, "error: unknown argument '%s'\n\n", arg);

			print_usage(argv[0]);

			exit(0);

		} else if (match(arg, "-v") || match(arg, "--version")) {
			// Note: printing to standard out instead of standard error.
			printf("userve %s\n", USERVE_VERSION);

			exit(0);

		} else {
			print_usage(argv[0]);

			exit(1);
		}
	}
}
