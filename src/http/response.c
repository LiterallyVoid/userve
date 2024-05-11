#include "http/response.h"

#include "util.h"
#include "warble/util.h"

#include <assert.h>

const char *http_status_to_string(HttpStatus status) {
	switch (status) {
	case HTTP_OK:	return "OK";
	case HTTP_BAD_REQUEST:	return "Bad Request";
	case HTTP_NOT_FOUND:	return "Not Found";
	case HTTP_INTERNAL_SERVER_ERROR:	return "Internal Server Error";
	}

	return "";
}

void http_response_init(HttpResponse *self, HttpRequest *req, int write_fd) {
	set_undefined(self, sizeof(*self));

	self->write_fd = write_fd;

	self->state = HTTP_RESPONSE_STATE_HEADERS;

	self->status = HTTP_INTERNAL_SERVER_ERROR;
	buffer_init(&self->headers);

	self->was_head_request = slice_equal(req->method, slice_from_cstr("HEAD"));
}

void http_response_deinit(HttpResponse *self) {
	Error err;

	switch (self->state) {
	case HTTP_RESPONSE_STATE_HEADERS:
		err = http_response_internal_server_error(self);
		// Not much we can do about errors here.
		(void) err;

		break;
	case HTTP_RESPONSE_STATE_BODY_CHUNKS:
		break;
	case HTTP_RESPONSE_STATE_DONE:
		break;
	}
	buffer_deinit(&self->headers);

	set_undefined(self, sizeof(*self));
}

Error http_response_not_found(HttpResponse *self) {
	if (self->state != HTTP_RESPONSE_STATE_HEADERS) return ERR_SUCCESS;

	Error err;

	buffer_clear(&self->headers);
	http_response_set_status(self, HTTP_NOT_FOUND);

	err = http_response_end_with_body(self, slice_from_cstr("not found"));
	if (err != ERR_SUCCESS) return err;

	return ERR_SUCCESS;
}

Error http_response_internal_server_error(HttpResponse *self) {
	if (self->state != HTTP_RESPONSE_STATE_HEADERS) return ERR_SUCCESS;

	Error err;

	buffer_clear(&self->headers);
	http_response_set_status(self, HTTP_INTERNAL_SERVER_ERROR);

	err = http_response_end_with_body(self, slice_from_cstr("internal server error"));
	if (err != ERR_SUCCESS) return err;

	return ERR_SUCCESS;
}

void http_response_set_status(HttpResponse *self, HttpStatus status) {
	// The status can only be changed if headers haven't been sent yet.
	assert(self->state == HTTP_RESPONSE_STATE_HEADERS);

	self->status = status;
}

Error http_response_add_header(HttpResponse *self, Slice name, Slice value) {
	// More headers can only be added if headers haven't been sent yet.
	assert(self->state == HTTP_RESPONSE_STATE_HEADERS);

	Error err = buffer_reserve_additional(
		&self->headers,
		name.len +
		// ": "
		2 +
		value.len +
		// "\r\n"
		2
	);
	if (err != ERR_SUCCESS) return err;

	// @TODO: Any validation at all. Urgent!
	buffer_concat(&self->headers, name);
	buffer_concat(&self->headers, slice_from_cstr(": "));
	buffer_concat(&self->headers, value);
	buffer_concat(&self->headers, slice_from_cstr("\n"));

	return ERR_SUCCESS;
}

// This function does not transition out of the `HTTP_RESPONSE_STATE_HEADERS`
// state, because it doesn't know whether it should go into the BODY_CHUNKS
// or DONE state.
static Error http_response_send_headers(HttpResponse *self) {
	// Headers can only be sent once.
	assert(self->state == HTTP_RESPONSE_STATE_HEADERS);

	Error err;

	Buffer status_line;
	buffer_init(&status_line);

	err = buffer_concat_printf(
		&status_line,
		"HTTP/1.1 %03d %s\r\n",
		self->status,
		http_status_to_string(self->status)
	);
	if (err != ERR_SUCCESS) {
		buffer_deinit(&status_line);
		return err;
	}

	err = write_all_to_fd(self->write_fd, buffer_slice(&status_line));
	buffer_deinit(&status_line);
	if (err != ERR_SUCCESS) return err;

	// The end of this response's headers should be marked by two newlines. It
	// doesn't really belong in `self->headers`, but a buffer is a buffer.
	err = buffer_concat(
		&self->headers,
		slice_from_cstr("\r\n")
	);
	if (err != ERR_SUCCESS) return err;

	err = write_all_to_fd(self->write_fd, buffer_slice(&self->headers));
	if (err != ERR_SUCCESS) return err;

	// We won't need to look at headers again; might as well free the memory.
	buffer_clear_capacity(&self->headers);

	return ERR_SUCCESS;
}

Error http_response_end_with_body(HttpResponse *self, Slice body) {
	Buffer content_length;
	buffer_init(&content_length);

	Error err;

	err = buffer_concat_printf(&content_length, "%zu", body.len);
	if (err != ERR_SUCCESS) {
		buffer_deinit(&content_length);
		return err;
	}

	err = http_response_add_header(
		self,
		slice_from_cstr("Content-Length"),
		buffer_slice(&content_length)
	);
	buffer_deinit(&content_length);
	if (err != ERR_SUCCESS) return err;

	err = http_response_send_headers(self);
	// Don't send headers twice, even if there's an error while sending headers.
	self->state = HTTP_RESPONSE_STATE_DONE;
	if (err != ERR_SUCCESS) return err;

	if (self->was_head_request) return ERR_SUCCESS;

	err = write_all_to_fd(self->write_fd, body);
	if (err != ERR_SUCCESS) return err;

	return ERR_SUCCESS;
}
