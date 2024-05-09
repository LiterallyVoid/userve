#include "arraylist.h"

#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void arraylist_init(ArrayList *self, size_t item_size) {
	*self = (ArrayList) {
		.item_size = item_size,

		.items = NULL,
		.len = 0,
		.cap = 0,
	};
}

void arraylist_deinit(ArrayList *self) {
	free(self->items);

	set_undefined(self, sizeof(*self));
}

void *arraylist_get(ArrayList *self, size_t index) {
	assert(index >= 0 && index < self->len);

	return self->item_size * index + (char*) self->items;
}

Error arraylist_reserve_total(ArrayList *self, size_t total) {
	if (self->cap >= total) return ERR_SUCCESS;

	size_t new_cap = next_power_of_two(total);
	if (new_cap < 8) new_cap = 8;

	void *new_items = realloc(self->items, self->item_size * new_cap);
	if (new_items == NULL) return ERR_OUT_OF_MEMORY;

	self->items = new_items;
	self->cap = new_cap;

	return ERR_SUCCESS;
}

Error arraylist_reserve_additional(ArrayList *self, size_t additional) {
	size_t total = self->len + additional;

	// Unsigned integer overflow is defined to wrap, so this isn't undefined behavior.
	assert(total >= self->len);

	arraylist_reserve_total(self, total);

	return ERR_SUCCESS;
}

Error arraylist_append(ArrayList *self, void *item) {
	Error err;

	err = arraylist_reserve_additional(self, 1);
	if (err != ERR_SUCCESS) return err;

	arraylist_append_assume_capacity(self, item);
	return ERR_SUCCESS;
}

void arraylist_append_assume_capacity(ArrayList *self, void *item) {
	assert(self->len + 1 <= self->cap);

	size_t index = self->len;
	self->len += 1;
	
	memcpy(arraylist_get(self, index), item, self->item_size);
}
