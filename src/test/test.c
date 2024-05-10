#include "test/test.h"
#include "test/harness.h"

#include "test/arguments.h"
#include "test/buffer.h"
#include "test/slice.h"
#include "test/hashmap.h"
#include "test/http_parser.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common/hash.h"

static void test_hash(TestContext *ctx) {
	test(ctx, "hash");

	EXPECT(ctx, hash(slice_from_cstr("a")) != hash(slice_from_cstr("b")));
}

Error test_all(void) {
	TestContext ctx = { 0 };

	printf("test hash\n");
	test_hash(&ctx);

	printf("test arguments\n");
	test_arguments(&ctx);

	printf("test buffer\n");
	test_buffer(&ctx);

	printf("test slice\n");
	test_slice(&ctx);

	printf("test hashmap\n");
	test_hashmap(&ctx);

	printf("test http parser\n");
	test_http_parser(&ctx);

	test_end(&ctx);

	printf("%d/%d tests passed\n", ctx.total_tests_passed, ctx.total_tests);

	if (ctx.total_tests_passed != ctx.total_tests) {
		return ERR_UNKNOWN;
	}

	return ERR_SUCCESS;
}
