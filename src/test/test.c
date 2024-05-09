#include "test.h"

#include "harness.h"

#include "arguments.h"
#include "buffer.h"
#include "http_parser.h"
#include "slice.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

Error test_all(void) {
	TestContext ctx = { 0 };

	printf("test arguments\n");
	test_arguments(&ctx);

	printf("test buffer\n");
	test_buffer(&ctx);

	printf("test slice\n");
	test_slice(&ctx);

	printf("test http parser\n");
	test_http_parser(&ctx);

	test_end(&ctx);

	printf("%d/%d tests passed\n", ctx.total_tests_passed, ctx.total_tests);

	if (ctx.total_tests_passed != ctx.total_tests) {
		return ERR_UNKNOWN;
	}

	return ERR_SUCCESS;
}
