#include "types/hashmap.h"

#include "common/hash.h"
#include "types/error.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>

static uint32_t hash_sentinel_aware(Slice key) {
	uint32_t hash = hash_fnv1a32(key);
	if (
		hash == HASHMAP_HASH_SENTINEL_EMPTY ||
		hash == HASHMAP_HASH_SENTINEL_TOMBSTONE
	) {
		// If the map size is a power of two (which it should be) and less than the
		// maximum 32-bit unsigned integer, this won't affect which slot the hash
		// corresponds to.
		hash += 0x8000'0000;
	}

	return hash;
}

void hashmap_init(HashMap *self, size_t value_size) {
	set_undefined(self, sizeof(*self));

	self->value_size = value_size;

	self->hashes = NULL;
	self->keys = NULL;
	self->values = NULL;

	self->values_count = 0;
	self->tombstones_count = 0;

	self->cap = 0;
}

void hashmap_deinit(HashMap *self) {
	free(self->hashes);
	free(self->keys);
	free(self->values);

	set_undefined(self, sizeof(*self));
}

static size_t slot_of_hash(HashMap *self, uint32_t hash) {
	assert(hash != HASHMAP_HASH_SENTINEL_EMPTY);
	assert(hash != HASHMAP_HASH_SENTINEL_TOMBSTONE);

	return hash % self->cap;
}

void hashmap_check_consistency(HashMap *self) {
	if (self->cap == 0) {
		assert(self->hashes == NULL);
		assert(self->keys == NULL);
		assert(self->values == NULL);

		return;
	}

	// Make sure our capacity is a power of two. Not required as of yet, but it's
	// a lot easier to remove this requirement later than to add it.
	assert(next_power_of_two(self->cap - 1) == self->cap);

	size_t values_count = 0;
	size_t tombstones_count = 0;

	for (size_t slot = 0; slot < self->cap; slot++) {
		uint32_t hash = self->hashes[slot];
		Slice key = self->keys[slot];

		if (hash == HASHMAP_HASH_SENTINEL_EMPTY) {
			continue;
		}
		
		if (hash == HASHMAP_HASH_SENTINEL_TOMBSTONE) {
			tombstones_count++;
			continue;
		}

		values_count += 1;

		assert(hash == hash_sentinel_aware(key));
		assert(slot == slot_of_hash(self, hash));
	}

	assert(values_count == self->values_count);
	assert(tombstones_count == self->tombstones_count);
}

Error hashmap_reserve_total(HashMap *self, size_t total) {
	// Careful! We have to take tombstones into account here---for example, if this
	// map's half full of tombstones, adding half of our capacity would make the map
	// three-quarters full (average case) or entirely full (worst case).
	//
	// And it's also hard to 

	assert(false); // todo
}

Error hashmap_reserve_additional(HashMap *self, size_t additional) {
	return hashmap_reserve_total(self, self->values_count + additional);
}

static HashMapEntry hashmap_construct_entry(HashMap *self, size_t slot, bool occupied) {
	Slice *key_ptr = &self->keys[slot];
	void *value_ptr = (char*) self->values + self->value_size * slot;

	return (HashMapEntry) {
		.key_ptr = key_ptr,
		.value_ptr = value_ptr,
		.occupied = occupied,
	};
}

// static HashMapEntry hashmap_get_or_put_assume_capacity(
// 	HashMap *self,
// 	Slice key,
// 	bool put
// ) {
// }

HashMapEntry hashmap_get(HashMap *self, Slice key) {
	// `slot_of_hash` does a division by zero for an empty hash map. Let's just
	// circumvent that.
	if (self->cap == 0) {
		return (HashMapEntry) { 0 };
	}

	uint32_t key_hash = hash_sentinel_aware(key);
	size_t base_slot = slot_of_hash(self, key_hash);

	for (size_t probe_length = 0; probe_length < self->cap; probe_length++) {
		size_t slot = (base_slot + probe_length) % self->cap;

		uint32_t slot_hash = self->hashes[slot];
		if (slot_hash == HASHMAP_HASH_SENTINEL_EMPTY) continue;
		if (slot_hash == HASHMAP_HASH_SENTINEL_TOMBSTONE) continue;
		if (slot_hash != key_hash) continue;

		if (!slice_equal(key, self->keys[slot])) continue;

		return hashmap_construct_entry(
			self,
			slot,
			true
		);
	}

	return (HashMapEntry) { 0 };
}

Error hashmap_put(HashMap *self, Slice key, HashMapEntry *out_entry) {
	Error err;
	
	err = hashmap_reserve_additional(self, 1);
	if (err != ERR_SUCCESS) return err;

	// There's no error code for @TODO :(
	return ERR_UNKNOWN;
}

