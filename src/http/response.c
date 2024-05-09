#include "response.h"

#include "../util.h"

const char *http_status_to_string(HttpStatus status) {
	switch (status) {
	case HTTP_OK:	return "OK";
	case HTTP_BAD_REQUEST:	return "Bad Request";
	case HTTP_NOT_FOUND:	return "Not Found";
	case HTTP_INTERNAL_SERVER_ERROR:	return "Internal Server Error";
	}

	return "";
}

void http_response_init(HttpResponse *self, int write_fd) {
	self->write_fd = write_fd;

	buffer_init(&self->headers);

	self->status = HTTP_INTERNAL_SERVER_ERROR;
}

void http_response_deinit(HttpResponse *self) {
	buffer_deinit(&self->headers);
}

void http_response_set_status(HttpResponse *self, HttpStatus status) {
	self->status = status;
}

Error http_response_add_header(HttpResponse *self, Slice name, Slice value) {
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

static Error http_response_send_headers(HttpResponse *self) {
	Error err;

	Buffer status;
	buffer_init(&status);

	err = buffer_concat_printf(
		&status,
		"HTTP/1.1 %03d %s\r\n",
		self->status,
		http_status_to_string(self->status)
	);
	if (err != ERR_SUCCESS) return err;

	err = write_all_to_fd(self->write_fd, buffer_slice(&status));
	if (err != ERR_SUCCESS) return err;

	err = write_all_to_fd(self->write_fd, buffer_slice(&self->headers));
	if (err != ERR_SUCCESS) return err;
}

Error http_response_end_with_body(HttpResponse *self, Slice body) {
	Error err;
	
	err = http_response_send_headers(self);
	if (err != ERR_SUCCESS) return err;
}
