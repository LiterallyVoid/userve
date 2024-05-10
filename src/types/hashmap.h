#pragma once

#include "types/error.h"
#include "types/slice.h"

#define HASHMAP_HASH_SENTINEL_EMPTY 0
#define HASHMAP_HASH_SENTINEL_TOMBSTONE 1

typedef struct HashMapEntry {
	uint32_t *hash_ptr;

	Slice *key_ptr;
	Slice *value_ptr;

	bool occupied;
} HashMapEntry;

// A hashmap with string keys.
typedef struct HashMap {
	// How may bytes each value takes up.
	size_t value_size;

	// Each item of this array is either a valid hash, `HASHMAP_HASH_SENTINEL_EMPTY`,
	// or `HASHMAP_HASH_SENTINEL_TOMBSTONE`.
	uint32_t *hashes;
	Slice *keys;

	// An array of values, each of size `value_size`.
	void *values;

	// How many values are in this hashmap.
	size_t values_count;

	// How many tombstones are in this hashmap.
	size_t tombstones_count;

	// How many long the `hashes`, `keys`, and `values` arrays are.
	size_t cap;
} HashMap;

void hashmap_init(HashMap *self, size_t value_size);
void hashmap_deinit(HashMap *self);

// Panic if any of `self`'s invariants are broken.
void hashmap_check_consistency(HashMap *self);

// Make sure `self` has the capacity to store `total` items.
Error hashmap_reserve_total(HashMap *self, size_t total);

// Make sure `self` has the capacity to store `additional` more items.
Error hashmap_reserve_additional(HashMap *self, size_t additional);

// If `self` doesn't contain the entry `key`, this will return an all-NULL `HashMapEntry`.
HashMapEntry hashmap_get(HashMap *self, Slice key);

// If `self` doesn't contain the entry `key`, this will insert an entry at `key`
// and output a `HashMapEntry` with `occupied` set to `false`, whose `key_ptr`
// and `value_ptr` must be written to.
//
// The key isn't written to the hashmap, because I want to make it clear that
// callsites are responsible for knowing whether or not this hashmap owns its keys.
Error hashmap_put(HashMap *self, Slice key, HashMapEntry *out_entry);
