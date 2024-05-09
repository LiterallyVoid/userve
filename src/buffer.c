#include "buffer.h"
#include "util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

Slice slice_new(void) {
	return (Slice) {
		.bytes = NULL,
		.len = 0,
	};
}

Slice slice_from_len(uint8_t *bytes, size_t len) {
	return (Slice) {
		.bytes = bytes,
		.len = len,
	};
}

Slice slice_from_cstr(const char *cstr) {
	return slice_from_len((uint8_t*) cstr, strlen(cstr));
}

Slice slice_remove_start(Slice self, size_t n) {
	assert(n <= self.len);
	return slice_from_len(
		self.bytes + n,
		self.len - n
	);
}

Slice slice_keep_bytes_from_end(Slice self, size_t n) {
	assert(n <= self.len);
	return slice_from_len(
		self.bytes + self.len - n,
		n
	);
}

void buffer_init(Buffer *self) {
	*self = (Buffer) { 0 };
}

void buffer_deinit(Buffer *self) {
	free(self->bytes);

	set_undefined(self, sizeof(*self));
}

Error buffer_reserve_total(Buffer *self, size_t total) {
	if (self->cap >= total) return ERR_SUCCESS;

	size_t new_cap = next_power_of_two(total);
	assert(new_cap >= self->cap);
	assert(new_cap >= total);

	uint8_t *new_bytes = realloc(self->bytes, new_cap);
	if (new_bytes == NULL) return ERR_OUT_OF_MEMORY;

	self->cap = new_cap;
	self->bytes = new_bytes;

	return ERR_SUCCESS;
}

Error buffer_reserve_additional(Buffer *self, size_t additional) {
	// Unsigned integer overflow isn't undefined behavior, so this is well-defined.
	assert(self->len + additional >= self->len);

	return buffer_reserve_total(self, self->len + additional);
}

Error buffer_concat(Buffer *self, Slice slice) {
	Error err;

	err = buffer_reserve_additional(self, slice.len);
	if (err != ERR_SUCCESS) return err;

	memcpy(self->bytes + self->len, slice.bytes, slice.len);
	self->len += slice.len;

	return ERR_SUCCESS;
}

int buffer_concat_printf(Buffer *self, const char *fmt, ...) {
	Error err;

	// Have a reasonable amount of space for the fast path of only calling
	// `vsnprintf` once.
	buffer_reserve_additional(self, 128);

	for (int i = 0; i < 2; i++) {
		va_list args;
		va_start(args, fmt);

		size_t space_available = buffer_uninitialized(self).len;
		int space_required = vsnprintf(
			(char*) buffer_uninitialized(self).bytes,
			space_available,
			fmt, args
		);
		va_end(args);

		if (space_required < 0) {
			return ERR_UNKNOWN;
		}

		if ((size_t) space_required <= space_available) {
			// If `bytes_written` is equal to `space_available`, `self->bytes` won't
			// have been-NUL terminated. This is fine, as `Buffer` isn't guaranteed
			// to be NUL-terminated anyway.
			self->len += space_required;
		
			return space_required;
		}

		// `vsnprintf` shouldn't require a different amount of bytes the second time; if this happens, something else has gone wrong.
		if (i != 0) return ERR_UNKNOWN;

		// Reserve more space for the second time around.
		err = buffer_reserve_additional(self, space_required);
		if (err != ERR_SUCCESS) return err;
	}

	return ERR_UNKNOWN;
}

Slice buffer_slice(Buffer *self) {
	return slice_from_len(
		self->bytes,
		self->len
	);
}

Slice buffer_uninitialized(Buffer *self) {
	return slice_from_len(
		self->bytes + self->len,
		self->cap - self->len
	);
}
