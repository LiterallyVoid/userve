#include "test.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct TestContext {
	bool is_in_test;

	char current_test[128];
	bool current_test_has_failed;

	int total_tests;
	int total_tests_passed;
} TestContext;

static void test_end(TestContext *ctx) {
	if (!ctx->is_in_test) {
		return;
	}
	ctx->is_in_test = false;

	ctx->total_tests++;
	if (!ctx->current_test_has_failed) {
		ctx->total_tests_passed++;
	}
}

__attribute__((__format__(__printf__, 2, 3)))
static void test(
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

static void expect(
	TestContext *ctx,
	bool condition,
	const char *condition_code,
	const char *file,
	int line
) {
	if (condition) return;

	fprintf(
		stderr,
		"test %s (%s:%d) assertion failed:\t%s\n",
		ctx->current_test,

		file,
		line,

		condition_code
	);

	ctx->current_test_has_failed = true;
}

#define EXPECT(ctx, condition) expect(ctx, condition, #condition, __FILE__, __LINE__)

#include "../arguments.h"
static void test_arguments(TestContext *ctx) {
	test(ctx, "arguments");

	Arguments arguments;

	arguments_parse(&arguments, 2, (const char*[]) { "@test0", "--address", "foobar" });
	EXPECT(ctx, strcmp(arguments.address, "localhost") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test1", "--address", "foobar" });
	EXPECT(ctx, strcmp(arguments.address, "foobar") == 0);

	arguments_parse(&arguments, 2, (const char*[]) { "@test2", "--address=1234" });
	EXPECT(ctx, strcmp(arguments.address, "1234") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test3", "-a", "--address" });
	EXPECT(ctx, strcmp(arguments.address, "--address") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test4", "-p", "-t" });
	EXPECT(ctx, strcmp(arguments.port, "-t") == 0);
	EXPECT(ctx, !arguments.test);

	arguments_parse(&arguments, 4, (const char*[]) { "@test5", "-p", "", "-t" });
	EXPECT(ctx, strcmp(arguments.port, "") == 0);
	EXPECT(ctx, arguments.test);
}

#include "../buffer.h"
static void test_buffer(TestContext *ctx) {
	test(ctx, "buffer");

	Buffer buffer;

	test(ctx, "buffer concat");
	buffer_init(&buffer);

	buffer_concat(&buffer, slice_from_cstr("12345"));
	buffer_concat(&buffer, slice_from_cstr("678"));
	EXPECT(ctx, buffer.len == 8);
	EXPECT(ctx, buffer_slice(&buffer).len == 8);
	EXPECT(ctx, memcmp(buffer.bytes, "12345678", 8) == 0);

	buffer_deinit(&buffer);


	test(ctx, "buffer concat_printf");
	buffer_init(&buffer);

	buffer_concat_printf(&buffer, "hey %d", 123);
	EXPECT(ctx, slice_equal(buffer_slice(&buffer), slice_from_cstr("hey 123")));

	buffer_deinit(&buffer);


	test(ctx, "buffer reserve");
	buffer_init(&buffer);

	buffer_reserve_total(&buffer, 10);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 10);

	buffer_reserve_total(&buffer, 1000);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1000);

	buffer_reserve_additional(&buffer, 1020);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1020);

	for (int i = 0; i < 102; i++) {
		buffer_concat(&buffer, slice_from_cstr("123456789A"));
	}
	EXPECT(ctx, buffer_slice(&buffer).len == 1020);

	test(ctx, "buffer clear");

	buffer_clear(&buffer);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1020);

	buffer_clear_capacity(&buffer);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len == 0);

	buffer_deinit(&buffer);
}

static void test_slice(TestContext *ctx) {
	test(ctx, "slice");
	Slice a = slice_from_cstr("abc xyz def");

	test(ctx, "slice equal");
	EXPECT(ctx, slice_equal(a, slice_from_cstr("abc xyz def")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("abc xyz")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("abc xyz defg")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("12345678")));

	test(ctx, "slice from_len");
	EXPECT(ctx, slice_equal(a, slice_from_len((uint8_t*) "abc xyz def zyx", 11)));

	test(ctx, "slice remove_start");
	EXPECT(ctx, slice_equal(slice_remove_start(a, 0), a));
	EXPECT(ctx, slice_equal(slice_remove_start(a, 4), slice_from_cstr("xyz def")));
	EXPECT(ctx, slice_equal(slice_remove_start(a, 11), slice_new()));

	test(ctx, "slice keep_bytes_from_end");
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 0), slice_from_cstr("")));
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 2), slice_from_cstr("ef")));
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 11), a));
}

#include "../http/parser.h"
static void test_http_parser(TestContext *ctx) {
	test(ctx, "http_parser");
	typedef enum {
		GOOD,
		BAD,
		INCOMPLETE
	} Result;
	typedef struct {
		const char *key;
		const char *value;
	} Header;
	struct {
		const char *title;

		Result result;

		const char *method;
		const char *path;
		const char *version;

		Header *headers;

		const char *file;

		const char *trailing;
	} cases[] = {
		{
			"no headers",

			GOOD,
			"GET", "/", "HTTP/1.1",
			(Header[]) {{ 0 }},

			"GET / HTTP/1.1\r\n"
			"\r\n",

			NULL
		},
		{
			"single header",

			GOOD,
			"POST", "/", "HTTP/1.1",
			(Header[]) {{"Header", "Value"}, { 0 }},

			"POST / HTTP/1.1\r\n"
			"Header: Value\r\n"
			"\r\n",

			NULL
		},
		{
			"single header HTTP/1.0",

			GOOD,
			"GET", "/", "HTTP/1.0",
			(Header[]) {{"Header", "Value"}, { 0 }},

			"GET / HTTP/1.0\r\n"
			"Header: Value\r\n"
			"\r\n",

			NULL
		},
		// FIXME: decide if this should be allowed
		{
			"single header no version",

			GOOD,
			"GET", "/", "",
			(Header[]) {{"Header", "Value"}, { 0 }},
			"GET /\r\n"
			"Header: Value\r\n"
			"\r\n",

			NULL
		},

		// Extra spaces are currently allowed.
		{
			"no headers extra spaces",

			GOOD,
			"GET", "/path/path", "HTTP/1.1",
			(Header[]) {{ 0 }},
			"GET      /path/path        HTTP/1.1\r\n"
			"\r\n",

			NULL
		},

		// Leading spaces in headers aren't allowed.
		{
			"leading space in first header",

			BAD,
			NULL, NULL, NULL, (Header[]) {{ 0 }},

			"GET / HTTP/1.1\r\n"
			" Header: Value\r\n"
			"\r\n",

			NULL
		},

		// Ditto, but on the second header instead of the first
		{
			"leading space in second header",

			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\r\n"
			"Header: Value\r\n"
			" Header: Value\r\n"
			"\r\n",

			NULL
		},

		// Spaces before the `GET` are currently not allowed by the parser.
		{
			"leading space in request line",

			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			" GET / HTTP/1.1\r\n"
			"\r\n",

			NULL
		},

		// Bare newlines are not allowed.
		{
			"bare newline in request line",

			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\n"
			"test\r\n"
			"\r\n",

			NULL
		},
		{
			"bare newline in header",

			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\r\n"
			"test\n"
			"\r\n",

			NULL
		},

		// Tabs are not allowed.
		{
			"tab in request line",

			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / \tHTTP/1.1\r\n"
			"test\r\n"
			"\r\n",

			NULL
		},

		// The HTTP parser should not consume all input, only until the first \r\n\r\n
		{
			"trailing data",

			GOOD,
			"GET", "/", "HTTP/1.1",
			(Header[]) {{ "header-name", "value" }, { 0 }},

			"GET / HTTP/1.1\r\n"
			"header-name: value\r\n"
			"\r\n"
			"POST / HTTP/1.2\r\n"
			"header2: header2\r\n",

			.trailing = "POST / HTTP/1.2\r\nheader2: header2\r\n",
		},
	};

	Error err;
	HttpParser parser;

	// First run: test every case with a single call to `poll`.
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		test(ctx, "http_parser all up: %s", cases[i].title);

		http_parser_init(&parser);

		HttpParserPollResult result;

		err = http_parser_poll(
			&parser,
			slice_from_cstr(cases[i].file),
			&result
		);

		switch (cases[i].result) {
		case GOOD: {
			EXPECT(ctx, err == ERR_SUCCESS);
			EXPECT(ctx, result.done);

			Slice trailing = slice_new();
			if (cases[i].trailing != NULL) {
				trailing = slice_from_cstr(cases[i].trailing);
			}

			EXPECT(ctx, slice_equal(result.remainder_slice, trailing));
			break;
		}

		case INCOMPLETE:
			EXPECT(ctx, err == ERR_SUCCESS);
			EXPECT(ctx, !result.done);
			EXPECT(ctx, cases[i].trailing == NULL);
			break;

		case BAD:
			EXPECT(ctx, err == ERR_PARSE_FAILED);
			EXPECT(ctx, cases[i].trailing == NULL);
			break;
		}

		http_parser_deinit(&parser);
	}

	// Second run: byte-by-byte
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		test(ctx, "http_parser byte-by-byte: %s", cases[i].title);

		Slice remainder = slice_new();

		http_parser_init(&parser);

		HttpParserPollResult result;

		const char *stream = cases[i].file;
		for (size_t byte_index = 0; stream[byte_index] != '\0'; byte_index++) {
			err = http_parser_poll(
				&parser,
				slice_from_len((uint8_t*) &stream[byte_index], 1),
				&result
			);

			if (result.done || err != ERR_SUCCESS) {
				remainder = slice_from_cstr(stream);
				remainder = slice_remove_start(remainder, byte_index + 1);
				break;
			}
		}

		switch (cases[i].result) {
		case GOOD: {
			EXPECT(ctx, err == ERR_SUCCESS);
			EXPECT(ctx, result.done);

			Slice trailing = slice_new();
			if (cases[i].trailing != NULL) {
				trailing = slice_from_cstr(cases[i].trailing);
			}

			EXPECT(ctx, slice_equal(remainder, trailing));
			break;
		}

		case INCOMPLETE:
			EXPECT(ctx, err == ERR_SUCCESS);
			EXPECT(ctx, !result.done);
			EXPECT(ctx, cases[i].trailing == NULL);
			break;

		case BAD:
			EXPECT(ctx, err == ERR_PARSE_FAILED);
			EXPECT(ctx, cases[i].trailing == NULL);
			break;
		}

		http_parser_deinit(&parser);
	}
}

void test_all(void) {
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
}
