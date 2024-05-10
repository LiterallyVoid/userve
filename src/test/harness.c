#include "test/harness.h"

#include <stdarg.h>
#include <stdio.h>

void test(
	TestContext *ctx,
	const char *fmt,
	...
) {
	test_end(ctx);

	ctx->is_in_test = true;
	ctx->current_test_has_failed = false;

	va_list args;
	va_start(args, fmt);

	vsnprintf(
		ctx->current_test,
		sizeof(ctx->current_test),
		fmt,
		args
	);

	va_end(args);
}

void test_end(TestContext *ctx) {
	if (!ctx->is_in_test) {
		return;
	}
	ctx->is_in_test = false;

	ctx->total_tests++;
	if (!ctx->current_test_has_failed) {
		ctx->total_tests_passed++;
	}
}

void expect(
	TestContext *ctx,
	bool condition,
	const char *condition_code,
	const char *file,
	int line
) {
	if (condition) return;

	fprintf(
		stderr,
		"test %s\t(%s:%d)\tassertion failed:\t%s\n",
		ctx->current_test,

		file,
		line,

		condition_code
	);

	ctx->current_test_has_failed = true;
}

