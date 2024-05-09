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
	expect(strcmp(arguments.address, "localhost") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test1", "--address", "foobar" });
	expect(strcmp(arguments.address, "foobar") == 0);

	arguments_parse(&arguments, 2, (const char*[]) { "@test2", "--address=1234" });
	expect(strcmp(arguments.address, "1234") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test3", "-a", "--address" });
	expect(strcmp(arguments.address, "--address") == 0);

	arguments_parse(&arguments, 3, (const char*[]) { "@test4", "-p", "-t" });
	expect(strcmp(arguments.port, "-t") == 0);
	expect(!arguments.test);

	arguments_parse(&arguments, 4, (const char*[]) { "@test5", "-p", "", "-t" });
	expect(strcmp(arguments.port, "") == 0);
	expect(arguments.test);
}

#include "../buffer.h"
static void test_buffer(void) {
	Buffer buffer;

	buffer_init(&buffer);

	buffer_concat(&buffer, slice_from_cstr("12345"));
	buffer_concat(&buffer, slice_from_cstr("678"));
	expect(buffer.len == 8);
	expect(buffer_slice(&buffer).len == 8);
	expect(memcmp(buffer.bytes, "12345678", 8) == 0);

	buffer_deinit(&buffer);


	buffer_init(&buffer);

	buffer_concat_printf(&buffer, "hey %d", 123);
	expect(slice_equal(buffer_slice(&buffer), slice_from_cstr("hey 123")));

	buffer_deinit(&buffer);


	buffer_init(&buffer);

	buffer_reserve_total(&buffer, 10);
	expect(buffer_slice(&buffer).len == 0);
	expect(buffer_uninitialized(&buffer).len >= 10);

	buffer_reserve_total(&buffer, 1000);
	expect(buffer_slice(&buffer).len == 0);
	expect(buffer_uninitialized(&buffer).len >= 1000);

	buffer_reserve_additional(&buffer, 1020);
	expect(buffer_slice(&buffer).len == 0);
	expect(buffer_uninitialized(&buffer).len >= 1020);

	for (int i = 0; i < 102; i++) {
		buffer_concat(&buffer, slice_from_cstr("123456789A"));
	}
	expect(buffer_slice(&buffer).len == 1020);

	buffer_clear(&buffer);
	expect(buffer_slice(&buffer).len == 0);
	expect(buffer_uninitialized(&buffer).len >= 1020);

	buffer_clear_capacity(&buffer);
	expect(buffer_slice(&buffer).len == 0);
	expect(buffer_uninitialized(&buffer).len == 0);

	buffer_deinit(&buffer);
}

static void test_slice(void) {
	Slice a = slice_from_cstr("abc xyz def");

	expect(slice_equal(a, slice_from_cstr("abc xyz def")));
	expect(!slice_equal(a, slice_from_cstr("abc xyz")));
	expect(!slice_equal(a, slice_from_cstr("abc xyz defg")));
	expect(!slice_equal(a, slice_from_cstr("12345678")));

	expect(slice_equal(a, slice_from_len((uint8_t*) "abc xyz def zyx", 11)));

	expect(slice_equal(slice_remove_start(a, 0), a));
	expect(slice_equal(slice_remove_start(a, 4), slice_from_cstr("xyz def")));
	expect(slice_equal(slice_remove_start(a, 11), slice_new()));

	expect(slice_equal(slice_keep_bytes_from_end(a, 0), slice_from_cstr("")));
	expect(slice_equal(slice_keep_bytes_from_end(a, 2), slice_from_cstr("ef")));
	expect(slice_equal(slice_keep_bytes_from_end(a, 11), a));
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
			expect(err == ERR_SUCCESS);
			expect(result.done);

			Slice trailing = slice_new();
			if (cases[i].trailing != NULL) {
				trailing = slice_from_cstr(cases[i].trailing);
			}

			expect(slice_equal(result.remainder_slice, trailing));
			break;
		}

		case INCOMPLETE:
			expect(err == ERR_SUCCESS);
			expect(!result.done);
			expect(cases[i].trailing == NULL);
			break;

		case BAD:
			expect(err == ERR_PARSE_FAILED);
			expect(cases[i].trailing == NULL);
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
