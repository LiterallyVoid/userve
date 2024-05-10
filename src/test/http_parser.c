#include "test/http_parser.h"
#include "http/parser.h"

#include <stdlib.h>

void test_http_parser(TestContext *ctx) {
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

		if (err == ERR_SUCCESS && result.done) {
			http_request_deinit(&result.request);
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

		if (err == ERR_SUCCESS && result.done) {
			http_request_deinit(&result.request);
		}

		http_parser_deinit(&parser);
	}
}


