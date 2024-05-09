#include "request.h"

#include "../util.h"

void http_request_deinit(HttpRequest *self) {
	buffer_deinit(&self->buffer);
	set_undefined(self, sizeof(*self));
}
