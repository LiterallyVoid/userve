#include "util.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

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
