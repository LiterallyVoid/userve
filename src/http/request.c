#include "request.h"

void http_request_deinit(HttpRequest *self) {
	buffer_deinit(&self->buffer);
}
