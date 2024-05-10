#pragma once

#include "types/buffer.h"

#include <sys/types.h>

typedef struct HttpRequest {
	// All slices point into ranges of this buffer.
	// At the moment, this is simply the entire HTTP request, but this may change later.
	Buffer buffer;

	Slice method;
	Slice target;
	Slice version;
} HttpRequest;

void http_request_deinit(HttpRequest *self);


