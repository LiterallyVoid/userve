#include "buffer.h"
#include "../util.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

void buffer_clear(Buffer *self) {
	self->len = 0;
}

void buffer_clear_capacity(Buffer *self) {
	free(self->bytes);

	self->bytes = NULL;
	self->len = 0;
	self->cap = 0;
}

Error buffer_concat(Buffer *self, Slice slice) {
	Error err;

	err = buffer_reserve_additional(self, slice.len);
	if (err != ERR_SUCCESS) return err;

	memcpy(self->bytes + self->len, slice.bytes, slice.len);
	self->len += slice.len;

	return ERR_SUCCESS;
}

Error buffer_concat_printf(Buffer *self, const char *fmt, ...) {
	Error err;

	// Have a reasonable amount of space for the fast path of only calling
	// `vsnprintf` once.
	err = buffer_reserve_additional(self, 128);
	if (err != ERR_SUCCESS) return err;

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
		
			return ERR_SUCCESS;
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
	if (self->bytes == NULL || self->cap == 0) {
		// It's undefined behavior to offset a NULL pointer.
		assert(self->bytes == NULL && self->len == 0 && self->cap == 0);

		return slice_new();
	}

	return slice_from_len(
		self->bytes + self->len,
		self->cap - self->len
	);
}
