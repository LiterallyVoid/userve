#include "response.h"

void http_response_init(HttpResponse *self, int write_fd) {
	self->write_fd = write_fd;
	buffer_init(&self->headers);
}

void http_response_deinit(HttpResponse *self) {
	buffer_deinit(&self->headers);
}
