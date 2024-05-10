#pragma once

typedef struct TestInfo {
	char name[128];

	// How many times `EXPECT` was called while this test was active.
	int expectations;

	// How many times `EXPECT` was given a passing condition while this test was
	// active.
	int expectations_passed;
} TestInfo;

typedef struct TestContext {
	bool is_in_test;
	TestInfo current_test;

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

