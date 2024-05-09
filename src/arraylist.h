#pragma once

#include "error.h"

#include <sys/types.h>

// A growable of fixed-size items.
typedef struct ArrayList {
	size_t item_size;

	void *items;
	size_t len;
	size_t cap;
} ArrayList;

void arraylist_init(ArrayList *self, size_t item_size);
void arraylist_deinit(ArrayList *self);

// Return a pointer to the item at `index` index of `self`.
//
// Panics if `index` is out of range.
void *arraylist_get(ArrayList *self, size_t index);

// Ensure that `self` has enough capacity to store `total` items without reallocation.
//
// This function can return ERR_OUT_OF_MEMORY.
Error arraylist_reserve_total(ArrayList *self, size_t total);

// Ensure that `additional` items can be appended to `self` without reallocation.
//
// This function can return ERR_OUT_OF_MEMORY.
Error arraylist_reserve_additional(ArrayList *self, size_t additional);

// Copy `item` and append it to `self`.
//
// This function can return ERR_OUT_OF_MEMORY.
Error arraylist_append_one(ArrayList *self, void *item);
