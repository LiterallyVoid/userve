#include "test.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define expect(condition) if (!(condition)) {	\
	fprintf(	\
		stderr,	\
		"%s (%s:%d) failed: %s\n",	\
		__FUNCTION__,	\
		__FILE__,	\
		__LINE__,	\
		#condition	\
	);	\
}

#include "../arguments.h"
static void test_arguments(void) {
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
static void test_buffer(void) {
	Buffer buffer;

	buffer_init(&buffer);

	buffer_concat(&buffer, slice_from_cstr("12345"));
	buffer_concat(&buffer, slice_from_cstr("678"));
	EXPECT(ctx, buffer.len == 8);
	EXPECT(ctx, buffer_slice(&buffer).len == 8);
	EXPECT(ctx, memcmp(buffer.bytes, "12345678", 8) == 0);

	buffer_deinit(&buffer);


	buffer_init(&buffer);

	buffer_concat_printf(&buffer, "hey %d", 123);
	EXPECT(ctx, slice_equal(buffer_slice(&buffer), slice_from_cstr("hey 123")));

	buffer_deinit(&buffer);


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

	buffer_clear(&buffer);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len >= 1020);

	buffer_clear_capacity(&buffer);
	EXPECT(ctx, buffer_slice(&buffer).len == 0);
	EXPECT(ctx, buffer_uninitialized(&buffer).len == 0);

	buffer_deinit(&buffer);
}

static void test_slice(void) {
	Slice a = slice_from_cstr("abc xyz def");

	EXPECT(ctx, slice_equal(a, slice_from_cstr("abc xyz def")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("abc xyz")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("abc xyz defg")));
	EXPECT(ctx, !slice_equal(a, slice_from_cstr("12345678")));

	EXPECT(ctx, slice_equal(a, slice_from_len((uint8_t*) "abc xyz def zyx", 11)));

	EXPECT(ctx, slice_equal(slice_remove_start(a, 0), a));
	EXPECT(ctx, slice_equal(slice_remove_start(a, 4), slice_from_cstr("xyz def")));
	EXPECT(ctx, slice_equal(slice_remove_start(a, 11), slice_new()));

	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 0), slice_from_cstr("")));
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 2), slice_from_cstr("ef")));
	EXPECT(ctx, slice_equal(slice_keep_bytes_from_end(a, 11), a));
}

#include "../http/parser.h"
static void test_http_parser(void) {
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
		Result result;

		const char *method;
		const char *path;
		const char *version;

		Header *headers;

		const char *file;

		const char *trailing;
	} cases[] = {
		{
			GOOD,
			"GET", "/", "HTTP/1.1",
			(Header[]) {{ 0 }},

			"GET / HTTP/1.1\r\n"
			"\r\n",

			.trailing = ""
		},
		{
			GOOD,
			"POST", "/", "HTTP/1.1",
			(Header[]) {{"Header", "Value"}, { 0 }},

			"POST / HTTP/1.1\r\n"
			"Header: Value\r\n"
			"\r\n",

			.trailing = ""
		},
		{
			GOOD,
			"GET", "/", "HTTP/1.0",
			(Header[]) {{"Header", "Value"}, { 0 }},

			"GET / HTTP/1.0\r\n"
			"Header: Value\r\n"
			"\r\n",

			.trailing = ""
		},
		// FIXME: decide if this should be allowed
		{
			GOOD,
			"GET", "/", "",
			(Header[]) {{"Header", "Value"}, { 0 }},
			"GET /\r\n"
			"Header: Value\r\n"
			"\r\n",

			.trailing = ""
		},

		// Extra spaces are currently allowed.
		{
			GOOD,
			"GET", "/path/path", "HTTP/1.1",
			(Header[]) {{ 0 }},
			"GET      /path/path        HTTP/1.1\r\n"
			"\r\n",

			.trailing = ""
		},

		// Leading spaces in headers aren't allowed.
		{
			BAD,
			NULL, NULL, NULL, (Header[]) {{ 0 }},

			"GET / HTTP/1.1\r\n"
			" Header: Value\r\n"
			"\r\n",

			.trailing = ""
		},

		// Ditto, but on the second header instead of the first
		{
			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\r\n"
			"Header: Value\r\n"
			" Header: Value\r\n"
			"\r\n",

			.trailing = ""
		},

		// Spaces before the `GET` are currently not allowed by the parser.
		{
			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			" GET / HTTP/1.1\r\n"
			"\r\n",

			.trailing = ""
		},

		// Bare newlines are not allowed, either to end a field or on the request line.
		{
			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\r\n"
			"test\n"
			"\r\n",

			.trailing = ""
		},
		{
			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\n"
			"test\r\n"
			"\r\n",

			.trailing = ""
		},

		// Tabs are not allowed.
		{
			BAD,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / \tHTTP/1.1\r\n"
			"test\r\n"
			"\r\n",

			.trailing = ""
		},

		// A lot of things are incomplete.
		{
			INCOMPLETE,
			NULL, NULL, NULL, (Header[]) { 0 },
			"",

			.trailing = ""
		},
		{
			INCOMPLETE,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GE",

			.trailing = ""
		},
		{
			INCOMPLETE,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET /",

			.trailing = ""
		},
		{
			INCOMPLETE,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / ",

			.trailing = ""
		},
		{
			INCOMPLETE,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\r\n"
			"hea",

			.trailing = ""
		},
		{
			INCOMPLETE,
			NULL, NULL, NULL, (Header[]) { 0 },
			"GET / HTTP/1.1\r\n"
			"header: value\r\n"
			"\r",

			.trailing = ""
		},

		// The HTTP parser should not consume all input, only until the first \r\n\r\n
		{
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
}

void test_all(void) {
	printf("test arguments\n");
	test_arguments();

	printf("test buffer\n");
	test_buffer();

	printf("test slice\n");
	test_slice();

	printf("test http parser\n");
	test_http_parser();

	printf("all tests passed\n");
}
