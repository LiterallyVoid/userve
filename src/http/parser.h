#pragma once

#include "../buffer.h"
#include "request.h"

typedef struct HttpParser {
	Buffer buffer;

	// Set when this parser has completed parsing, and returned a result with `done`
	// set to true. Only use for lifecycle validation!
	bool done;
} HttpParser;

typedef struct HttpParserPollResult {
	bool done;

	// Only defined when `done` is `true`.
	Slice remainder_slice;
	HttpRequest request;
} HttpParserPollResult;

void http_parser_init(HttpParser *self);
void http_parser_deinit(HttpParser *self);

// If parsing fails, `poll` will return `ERR_PARSE_FAILED`.
Error http_parser_poll(
	HttpParser *self,
	Slice bytes,
	HttpParserPollResult *out_result
);

