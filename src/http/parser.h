#pragma once

#include "../buffer.h"
#include "request.h"

typedef struct HttpParser {
	Buffer buffer;
} HttpParser;

typedef enum HttpParserStatus {
	// The request is not yet complete, and the caller should continue to poll this parser with new data.
	HTTP_PARSER_INCOMPLETE,

	// The request was malformed.
	HTTP_PARSER_MALFORMED,

	HTTP_PARSER_DONE,
} HttpParserStatus;

typedef struct HttpParserPollResult {
	HttpParserStatus status;

	struct {
		Slice remainder_slice;

		// Must be deinitialized by the caller.
		HttpRequest request;
	} done;
} HttpParserPollResult;

void http_parser_init(HttpParser *self);
void http_parser_deinit(HttpParser *self);

void http_parser_poll(
	HttpParser *self,
	Slice bytes,
	HttpParserPollResult *out_result
);

