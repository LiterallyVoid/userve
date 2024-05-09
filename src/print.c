#include "print.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void print_address_ipv4(FILE *fp, struct sockaddr_in *addr) {
	fprintf(
		fp,
		"%d.%d.%d.%d:%d",
		0xFF & (addr->sin_addr.s_addr >> 0),
		0xFF & (addr->sin_addr.s_addr >> 8),
		0xFF & (addr->sin_addr.s_addr >> 16),
		0xFF & (addr->sin_addr.s_addr >> 24),

		ntohs(addr->sin_port)
	);
}

static void print_address_ipv6(FILE *fp, struct sockaddr_in6 *addr) {
	const int chunks_count = 8;

	// Same number as above. Using a variable (even if it's `const`) would make this a VLA,
	// and a `#define` would escape the scope of this function. Sigh.
	uint16_t chunks[8] = { 0 };

	assert(sizeof(chunks) == chunks_count * sizeof(uint16_t));

	int chunk = 0;
	for (size_t octet = 0; octet < sizeof(addr->sin6_addr); octet += 2) {
		assert(chunk >= 0 && chunk < chunks_count);

		chunks[chunk] =
			(uint16_t) addr->sin6_addr.s6_addr[octet] << 8 |
			(uint16_t) addr->sin6_addr.s6_addr[octet + 1];

		chunk += 1;
	}

	fprintf(fp, "[");

	size_t compress_start = 0, compress_len = 0;

	// Find the longest consecutive run of zeros to compress.
	size_t tentative_compress_start = 0;
	for (size_t chunk = 0; chunk < chunks_count; chunk++) {
		if (chunks[chunk] != 0) {
			tentative_compress_start = chunk + 1;
			continue;
		}

		// All chunks in the range [tentative_compress_start, chunk] are zero; add one.

		// To see this:

		// If `chunks` are        [1 0 0 1 2]
		// Its indices are         0 1 2 3 4
		//                           ^ ^
		// tentative_compress_start _/ |
		//                             \_ chunk
		//
		// The inclusive range [1, 2] is equivalent to the exclusive range [1, 3), which
		// is `2` items long.

		size_t len = (chunk - tentative_compress_start) + 1;

		if (len >= compress_len) {
			compress_start = tentative_compress_start;
			compress_len = len;
		}
	}

	bool skip_colon = true;

	for (size_t chunk = 0; chunk < chunks_count; chunk++) {
		if (chunk >= compress_start && chunk < compress_start + compress_len) {
			assert(chunks[chunk] == 0);

			// Write the `::` for only the first chunk within the compression range.
			if (chunk == compress_start) {
				fprintf(fp, "::");
				skip_colon = true;
			}

			continue;
		}
		
		if (skip_colon) {
			skip_colon = false;
		} else {
			fprintf(fp, ":");
		}

		fprintf(fp, "%X", chunks[chunk]);
	}

	fprintf(fp, "]");
	fprintf(fp, ":%d", ntohs(addr->sin6_port));

	if (addr->sin6_flowinfo != 0) {
		fprintf(fp, ":(%ux)", ntohl(addr->sin6_flowinfo));
	}
}

void print_address(FILE *fp, struct sockaddr *addr, socklen_t addr_len) {
	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *addr_ipv4 = (struct sockaddr_in*) addr;
		if (addr_len < sizeof(*addr_ipv4)) {
			fprintf(fp, "(address too long!)");
			return;
		}

		print_address_ipv4(fp, addr_ipv4);
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6 *addr_ipv6 = (struct sockaddr_in6*) addr;
		if (addr_len < sizeof(*addr_ipv6)) {
			fprintf(fp, "(address too long!)");
			return;
		}

		print_address_ipv6(fp, addr_ipv6);
		break;
	}
	default:
		fprintf(fp, "(unknown address family %d)", addr->sa_family);
		break;
	}
}

void print_slice(FILE *fp, Slice slice) {
	fprintf(fp, "\"");

	for (size_t cursor = 0; cursor < slice.len; ) {
		// First, try to print a run of printable characters.
		{
			size_t printable_start = cursor;
			while (
				cursor < slice.len &&
				slice.bytes[cursor] >= 32 &&
				slice.bytes[cursor] < 127 &&
				slice.bytes[cursor] != '"'
			) {
				cursor++;
			}

			if (cursor > printable_start) {
				fwrite(
					slice.bytes + printable_start,
					1,
					cursor - printable_start,
					fp
				);

				continue;
			}

		}

		// There were no printable characters, so let's print a single non-printable character.
		uint8_t byte = slice.bytes[cursor];

		// Skip to the next character early, so we can't forget to do it later.
		cursor++;

		if (byte == '"') {
			fprintf(fp, "\"");
		} else if (byte == '\t') {
			fprintf(fp, "\\t");
		} else if (byte == '\r') {
			fprintf(fp, "\\r");
		} else if (byte == '\n') {
			// To be nice, print a newline after printing the character escape for a newline.
			fprintf(fp, "\\n\n");
		} else {
			fprintf(fp, "\\x%02X", slice.bytes[cursor]);
		}
	}

	fprintf(fp, "\"");
}

void print_http_request(FILE *fp, const HttpRequest *request) {
	fprintf(fp, "HTTP ");
	print_slice(fp, request->method);

	fprintf(fp, " target: ");
	print_slice(fp, request->target);

	fprintf(fp, " version: ");
	print_slice(fp, request->version);
	fprintf(fp, "\n");
}

