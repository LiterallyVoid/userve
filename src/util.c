#include "util.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <limits.h>
#include <unistd.h>

size_t next_power_of_two(size_t num) {
	num |= num >> 1;
	num |= num >> 2;
	num |= num >> 4;
	num |= num >> 8;
	num |= num >> 16;

#if SIZE_MAX == 0xFFFF'FFFF
#elif SIZE_MAX == 0xFFFF'FFFF'FFFF'FFFF
	num |= num >> 32;
#else
	#error unknown size_t bit width
#endif

	// Increment `num`.
	num++;

	// Unsigned overflow is defined to wrap.
	assert(num != 0);

	return num;
}

void set_undefined(void *ptr, size_t len) {
	memset(ptr, 0xAA, len);
}

Error write_all_to_fd(int fd, Slice slice) {
	while (slice.len > 0) {
		size_t attempt_write = slice.len;
		if (attempt_write > INT_MAX) attempt_write = INT_MAX;

		ssize_t amount_written = write(fd, slice.bytes, attempt_write);
		if (amount_written < 0) {
			perror("write");
			return ERR_UNKNOWN;
		}

		slice = slice_remove_start(slice, amount_written);
	}

	return ERR_SUCCESS;
}

Slice detect_content_type(Slice path) {
	struct ContentType {
		Slice suffix;
		Slice content_type;
	} content_types[] = {
		// Text.
		{ slice_from_cstr(".txt"), slice_from_cstr("text/plain; charset=utf-8") },

		// HTML
		{ slice_from_cstr(".html"), slice_from_cstr("text/html; charset=utf-8") },
		{ slice_from_cstr(".css"), slice_from_cstr("text/css; charset=utf-8") },
		{ slice_from_cstr(".js"), slice_from_cstr("text/javascript; charset=utf-8") },

		// Documents?
		{ slice_from_cstr(".md"), slice_from_cstr("text/markdown; charset=utf-8") },
		{ slice_from_cstr(".pdf"), slice_from_cstr("application/pdf") },

		// Fonts
		{ slice_from_cstr(".ttf"), slice_from_cstr("font/ttf") },
		{ slice_from_cstr(".otf"), slice_from_cstr("font/otf") },
		{ slice_from_cstr(".woff"), slice_from_cstr("font/woff") },
		{ slice_from_cstr(".woff2"), slice_from_cstr("font/woff2") },

		// Multimedia
		// - Images
		{ slice_from_cstr(".png"), slice_from_cstr("image/png") },
		{ slice_from_cstr(".jpg"), slice_from_cstr("image/jpeg") },
		{ slice_from_cstr(".jpeg"), slice_from_cstr("image/jpeg") },
		{ slice_from_cstr(".svg"), slice_from_cstr("image/svg+xml") },
		{ slice_from_cstr(".ico"), slice_from_cstr("image/vnd.microsoft.icon") },

		// - Audio
		{ slice_from_cstr(".mp3"), slice_from_cstr("audio/mpeg") },
		{ slice_from_cstr(".ogg"), slice_from_cstr("application/ogg") },

		// - Video
		{ slice_from_cstr(".mp4"), slice_from_cstr("video/mp4") },
	};

	for (size_t i = 0; i < sizeof(content_types) / sizeof(content_types[0]); i++) {
		struct ContentType content_type = content_types[i];
		if (!slice_remove_suffix(&path, content_type.suffix)) continue;

		return content_type.content_type;
	}

	return slice_from_cstr("application/octet-stream");
}

