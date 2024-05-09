#pragma once

#include "../buffer.h"

typedef struct {
	// A file descriptor to write the response to.
	int write_fd;

	Buffer headers;
} HttpResponse;

void http_response_init(HttpResponse *self, int write_fd);
void http_response_deinit(HttpResponse *self);

void http_response_add_header(HttpResponse *res, Slice header, Slice foo);
