#include "test/harness.h"
#include "util.h"

#include <stdarg.h>
#include <stdio.h>

void test(
	TestContext *ctx,
	const char *fmt,
	...
) {
	test_end(ctx);

	set_undefined(&ctx->current_test, sizeof(ctx->current_test));
	ctx->current_test.expectations = 0;
	ctx->current_test.expectations_passed = 0;

	va_list args;
	va_start(args, fmt);

	vsnprintf(
		ctx->current_test.name,
		sizeof(ctx->current_test.name),
		fmt,
		args
	);

	va_end(args);

	ctx->is_in_test = true;
}

void test_end(TestContext *ctx) {
	if (!ctx->is_in_test) {
		return;
	}

	ctx->is_in_test = false;

	if (ctx->current_test.expectations == 0) {
		return;
	}

	ctx->total_tests++;
	if (ctx->current_test.expectations_passed < ctx->current_test.expectations) {
		return;
	}

	ctx->total_tests_passed++;
}

void expect(
	TestContext *ctx,
	bool condition,
	const char *condition_code,
	const char *file,
	int line
) {
	ctx->current_test.expectations++;
	if (condition) {
		ctx->current_test.expectations_passed++;
		return;
	}

	fprintf(
		stderr,
		"test %s\t(%s:%d)\tassertion failed:\t%s\n",
		ctx->current_test.name,

		file,
		line,

		condition_code
	);
}

