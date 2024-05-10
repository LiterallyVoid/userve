#include "types/slice.h"

#include <assert.h>
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

bool slice_equal(Slice a, Slice b) {
	// If both strings are empty, don't look at their bytes.
	if (a.len == 0 && b.len == 0) return true;

	if (a.len != b.len) return false;
	if (memcmp(a.bytes, b.bytes, a.len) != 0) return false;

	return true;
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


