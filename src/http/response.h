#pragma once

#include "../buffer.h"

// Long ago, the four nations lived in harmony.
typedef enum HttpStatus {
	HTTP_OK = 200,

	HTTP_BAD_REQUEST = 400,

	HTTP_NOT_FOUND = 404,

	HTTP_INTERNAL_SERVER_ERROR = 500,
} HttpStatus;

typedef struct {
	// A file descriptor to write the response to.
	int write_fd;

	Buffer headers;
} HttpResponse;

// Initialize `self`, in preparation for writing an HTTP response to `write_fd`.
void http_response_init(HttpResponse *self, int write_fd);

// If headers haven't been sent yet, send 500 Internal Server Error in response.
void http_response_deinit(HttpResponse *self);

void http_response_set_status(HttpResponse *self, HttpStatus status);
void http_response_add_header(HttpResponse *self, Slice header, Slice foo);

void http_response_end(HttpResponse *self);
