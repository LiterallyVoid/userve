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

// Remove all occurences of `ch` from the start of `rest`.
static void trim_start(Slice *rest, uint8_t ch) {
	while (rest->len > 0 && rest->bytes[0] == ch) {
		rest->bytes += 1;
		rest->len -= 1;
	}
}

// Remove exactly one space character from `rest`.
// If `rest` does not start with a space, return false.
static bool remove_space(Slice *rest) {
	if (rest->len < 1) return false;
	if (rest->bytes[0] != ' ') return false;

	*rest = slice_remove_start(*rest, 1);

	return true;
}

// Remove at least one space character from `rest`.
// If `rest` does not start with a space, return false.
static bool remove_many_spaces(Slice *rest) {
	if (rest->len < 1) return false;
	if (rest->bytes[0] != ' ') return false;

	trim_start(rest, ' ');

	return true;
}

// Remove exactly one carriage return and newline from `rest`.
// If `rest` does not start with `\r\n`, return false.
static bool remove_newline(Slice *rest) {
	if (rest->len < 2) return false;
	if (rest->bytes[0] != '\r' || rest->bytes[1] != '\n') return false;

	*rest = slice_remove_start(*rest, 2);

	return true;
}

// Return a single field that starts from exactly `rest`, and ends *before* the
// first byte that `allow_byte` returns `false` for.
static Slice cut_field(
	Slice *rest,
	bool (*allow_byte)(uint8_t byte)
) {
	size_t index = 0;
	for (; index < rest->len; index++) {
		if (allow_byte(rest->bytes[index])) {
			continue;
		}

		break;
	}

	Slice field;
	slice_split_at_index(
		// slice
		*rest,

		// index
		index,

		// before
		&field,

		// after
		rest
	);

	return field;
}

static bool is_token_byte(uint8_t byte) {
	// `token`, from https://datatracker.ietf.org/doc/html/rfc9110#appendix-A-2
	switch (byte) {
	case '!':
	case '#':
	case '$':
	case '%':
	case '&':
	case '\'':
	case '*':
	case '+':
	case '-':
	case '.':
	case '^':
	case '_':
	case '`':
	case '|':
	case '~':
		return true;
	}

	if (byte >= '0' && byte <= '9') {
		return true;
	}

	if (
		(byte >= 'a' && byte <= 'z') ||
		(byte >= 'A' && byte <= 'Z')
	) {
		return true;
	}

	return false;
}

static bool is_target_byte(uint8_t byte) {
	if (byte == '\r' || byte == '\n') return false;
	if (byte == ' ' || byte == '\t') return false;

	// Exclude all control characters.
	if (byte < ' ') return false;

	return true;
}

static bool is_version_byte(uint8_t byte) {
	return is_token_byte(byte) || byte == '/' || byte == '.';
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

	// The method must be uppercase.
	Slice method = cut_field(&bytes, is_token_byte);
	if (method.len == 0) return ERR_PARSE_FAILED;
	if (!remove_many_spaces(&bytes)) return ERR_PARSE_FAILED;

	Slice target = cut_field(&bytes, is_target_byte);
	if (target.len == 0) return ERR_PARSE_FAILED;

	// Allowing any token as a version is more lenient than spec.
	// An empty version is allowed, and understood as an HTTP/1.0 Simple-Request.
	Slice version = slice_new();
	if (remove_space(&bytes)) {
		remove_many_spaces(&bytes);

		version = cut_field(&bytes, is_version_byte);
	}

	// `cut_field` removes trailing whitespace; if `bytes` doesn't start with a
	// newline, that means there are more fields than method, path, and version;
	// and the request is malformed.
	if (!remove_newline(&bytes)) {
		print_slice(stdout, bytes);
		return ERR_PARSE_FAILED;
	}

	request->buffer = buffer;

	request->method = method;
	request->target = target;
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
