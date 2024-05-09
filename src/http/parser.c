#include "parser.h"

#include "../print.h"
#include "../util.h"
#include "request.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void http_parser_init(HttpParser *self) {
	set_undefined(self, sizeof(*self));

	buffer_init(&self->buffer);
}

void http_parser_deinit(HttpParser *self) {
	buffer_deinit(&self->buffer);
}

// Return the index of the first occurence of `byte` in `slice`, starting from `start`.
// If there are no `byte` bytes in `slice`, instead return `-1`.
static ssize_t find_char(Slice slice, uint8_t byte, size_t start) {
	assert(start >= 0 && start <= slice.len);
	uint8_t *ptr_to_character = (uint8_t*) memchr(slice.bytes + start, byte, slice.len - start);
	if (ptr_to_character == NULL) return -1;

	size_t offset = ptr_to_character - slice.bytes;
	assert(offset >= 0 && offset < SSIZE_MAX);

	return offset;
}

static void slice_split_at_index(Slice slice, size_t index, Slice *out_before, Slice *out_after) {
	assert(index >= 0 && index < slice.len);

	out_before->bytes = slice.bytes;
	out_before->len = index;

	out_after->bytes = slice.bytes + index;
	out_after->len = slice.len - index;
}

static void slice_split_around_character(Slice slice, size_t index, Slice *out_before, Slice *out_after) {
	assert(slice.len > 0 && index >= 0 && index < slice.len - 1);

	out_before->bytes = slice.bytes;
	out_before->len = index;

	out_after->bytes = slice.bytes + index + 1;
	out_after->len = slice.len - index - 1;
}

// Remove the part of `rest` before the first occurence of `ch`, and return it.
// If `rest` doesn't contain `ch`, this returns the entire slice.
static Slice cut(Slice *rest, uint8_t ch) {
	ssize_t first_newline = find_char(*rest, '\n', 0);

	if (first_newline == -1) {
		Slice slice = *rest;
		*rest = slice_keep_bytes_from_end(*rest, 0);
		return slice;
	}

	Slice before;
	slice_split_around_character(*rest, first_newline, &before, rest);

	return before;
}

// Remove all occurences of `ch` from the start of `rest`.
static void trim_start(Slice *rest, uint8_t ch) {
	while (rest->len > 0 && rest->bytes[0] == ch) {
		rest->bytes += 1;
		rest->len -= 1;
	}
}

// Remove exactly one carriage return and newline from `rest`.
// If `rest` does not start with `\r\n`, return false.
static bool remove_newline(Slice *rest) {
	if (rest->len < 2) return false;
	if (rest->bytes[0] != '\r' || rest->bytes[1] != '\n') return false;

	*rest = slice_remove_start(*rest, 2);

	return true;
}

// Return a single field that starts from exactly `rest`, and trim spaces between the end of this field and the start of the next.
// If the field contains any bytes that aren't allowed by `byte_is_allowed`, field parsing fails.
//
// If a field couldn't be parsed, this returns an empty slice and leaves `rest` unchanged. Using an empty string as a sentinel is fine (if a little ugly), because there's no way to have an empty field if it's space-delimited.
static Slice cut_field(Slice *rest, bool (*byte_is_allowed)(uint8_t byte)) {
	if (rest->len > 0 && rest->bytes[0] == ' ') {
		return slice_new();
	}

	size_t index = 0;
	for (; index < rest->len; index++) {
		if (rest->bytes[index] == ' ') {
			break;
		}

		// Stop on encountering either `\n` and `\r\n` newlines here. Line endings
		// should be enforced by `remove_newline`.
		if (rest->bytes[index] == '\r' || rest->bytes[index] == '\n') {
			break;
		}

		if (byte_is_allowed && !byte_is_allowed(rest->bytes[index])) {
			return slice_new();
		}
	}

	Slice field;
	slice_split_at_index(*rest, index, &field, rest);

	trim_start(rest, ' ');

	return field;
}

static bool is_uppercase_byte(uint8_t byte) {
	return byte >= 'A' && byte <= 'Z';
}

static bool is_any_byte(uint8_t byte) {
	return true;
}

// Returns `0` if the parse was successful or `-1` otherwise.
// Only takes ownership of `buffer` if the parse was successful.
static Error parse_headers(
	Buffer buffer,
	HttpRequest *request
) {
	// Make sure it's very loud if we forget to set any field.
	set_undefined(request, sizeof(*request));

	request->buffer = buffer;
	Slice bytes = buffer_slice(&buffer);

	trim_start(&bytes, ' ');

	// The method must be uppercase.
	Slice method = cut_field(&bytes, is_uppercase_byte);
	if (method.len == 0) {
		printf("request malformed: no method\n");
		return ERR_PARSE_FAILED;
	}

	Slice path = cut_field(&bytes, NULL);
	if (path.len == 0) {
		printf("request malformed: no path\n");
		return ERR_PARSE_FAILED;
	}

	Slice version = cut_field(&bytes, NULL);
	// An empty version is allowed, and understood as an HTTP/1.0 Simple-Request for now.

	// `cut_field` removes trailing whitespace; if `bytes` doesn't start with a newline, that means
	// there are more fields than method, path, and version; and the request is malformed.
	if (!remove_newline(&bytes)) {
		printf("request malformed: no newline\n");
		return ERR_PARSE_FAILED;
	}

	request->buffer = buffer;

	request->method = method;
	request->path = path;
	request->version = version;

	return ERR_SUCCESS;
}

Error http_parser_poll(
	HttpParser *self,
	Slice bytes,
	HttpParserPollResult *out_result
) {
	// We do nothing until we find two newlines in a row, and parse the header then.
	// We're okay by default.
	out_result->status = HTTP_PARSER_INCOMPLETE;

	// We successfully parsed zero bytes.
	if (bytes.len == 0) return ERR_SUCCESS;

	size_t index = self->buffer.len;
	// self->buffer may already end in `\r\n\r`, so we have to look two characters back for the newline.
	if (index > 2) {
		index -= 2;
	} else {
		index = 0;
	}

	buffer_concat(&self->buffer, bytes);

	ssize_t prev_newline = -1;

	Slice all_bytes = buffer_slice(&self->buffer);

	while (true) {
		ssize_t next_newline = find_char(all_bytes, '\n', index);
		if (next_newline < 0) break;

		// Start looking for the next newline after this '\n' character.
		index = next_newline + 1;

		if (prev_newline < 0) {
			prev_newline = next_newline;
			continue;
		}

		// There are two cases where we'd want to stop:
		// 1) `next_newline` immediately follows `prev_newline`
		// 2) `next_newline` is two characters away from `prev_newline` and there's
		//    a carriage return between them.
		//
		// Line endings will be enforced later. Let's be lenient for now, so there's only
		// one place to change that later.

		// `prev_newline + 1` must be in-bounds, because we advance one character every time,
		// and we found an index for `next_newline`.
		if ((prev_newline + 1 == next_newline) || (
			prev_newline + 2 == next_newline &&
			self->buffer.bytes[prev_newline + 1] == '\r'
	    )) {
			// Truncate the buffer to include the character at `next_newline`.
			size_t new_buffer_end = next_newline + 1;

			size_t removed_bytes = self->buffer.len - new_buffer_end;

			self->buffer.len = new_buffer_end;

			out_result->done.remainder_slice = slice_keep_bytes_from_end(all_bytes, removed_bytes);

			// We'll fill in `out_result->request` later.
			out_result->status = HTTP_PARSER_DONE;

			break;
		}

		prev_newline = next_newline;
	}

	if (out_result->status != HTTP_PARSER_DONE) return ERR_SUCCESS;

	printf("Found end of HTTP request\n");
	print_slice(stdout, buffer_slice(&self->buffer));;

	// Move `buffer` out of `self`.
	Buffer buffer = self->buffer;

	// Create a new empty buffer that can be freed without consequence later, in `http_parser_deinit`.
	buffer_init(&self->buffer);

	// ALl the actual parsing goes here.
	Error err = parse_headers(buffer, &out_result->done.request);
	if (err != 0) {
		buffer_deinit(&buffer);

		return ERR_PARSE_FAILED;
	}

	return ERR_SUCCESS;
}
