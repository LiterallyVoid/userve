#pragma once

typedef struct TestContext {
	bool is_in_test;

	char current_test[128];

	// `false` until `EXPECT` is called during the current test.
	// Used to not count empty tests.
	bool current_test_is_empty;

	// `false` until `EXPECT` is called with a `false` condition.
	// Used to tally passing tests.
	bool current_test_has_failed;

	int total_tests;
	int total_tests_passed;
} TestContext;


__attribute__((__format__(__printf__, 2, 3)))
void test(
	TestContext *ctx,
	const char *fmt,
	...
);
void test_end(TestContext *ctx);

// If `condition` is false, mark the current test as failed, print an error, and
// *keep running*.
void expect(
	TestContext *ctx,
	bool condition,
	const char *condition_code,
	const char *file,
	int line
);
#define EXPECT(ctx, condition) expect(ctx, condition, #condition, __FILE__, __LINE__)

