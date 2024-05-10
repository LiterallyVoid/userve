#include "types/hashmap.h"

#include "common/hash.h"
#include "types/error.h"
#include "util.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static HashMapEntry hashmap_construct_entry(HashMap *self, size_t slot, bool occupied) {
	uint32_t *hash_ptr = &self->hashes[slot];
	Slice *key_ptr = &self->keys[slot];
	void *value_ptr = (char*) self->values + self->value_size * slot;

	return (HashMapEntry) {
		.hash_ptr = hash_ptr,
		.key_ptr = key_ptr,
		.value_ptr = value_ptr,
		.occupied = occupied,
	};
}

static HashMapEntry hashmap_get_or_put_assume_capacity(
	HashMap *self,
	Slice key,
	bool put
) {
	// `slot_of_hash` does a division by zero for an empty hash map. Let's just
	// circumvent that.
	if (self->cap == 0) {
		return (HashMapEntry) { 0 };
	}

	bool seen_empty_slot = false;
	HashMapEntry empty_slot;

	uint32_t key_hash = hash_sentinel_aware(key);
	size_t base_slot = slot_of_hash(self, key_hash);

	for (size_t probe_length = 0; probe_length < self->cap; probe_length++) {
		size_t slot = (base_slot + probe_length) % self->cap;

		uint32_t slot_hash = self->hashes[slot];
		if (!seen_empty_slot && (
			slot_hash == HASHMAP_HASH_SENTINEL_EMPTY ||
			slot_hash == HASHMAP_HASH_SENTINEL_TOMBSTONE
		)) {
			seen_empty_slot = true;
			empty_slot = hashmap_construct_entry(self, slot, false);
		}

		if (slot_hash == HASHMAP_HASH_SENTINEL_EMPTY) break;
		if (slot_hash == HASHMAP_HASH_SENTINEL_TOMBSTONE) continue;
		if (slot_hash != key_hash) continue;

		if (!slice_equal(key, self->keys[slot])) continue;

		return hashmap_construct_entry(self, slot, true);
	}

	if (put) {
		assert(seen_empty_slot);

		// If `empty_slot` was a tombstone slot, decrement `tombstones_count` because
		// we're transitioning that slot into a value slot.
		if (*empty_slot.hash_ptr == HASHMAP_HASH_SENTINEL_TOMBSTONE) {
			self->tombstones_count--;
		}

		self->values_count++;

		*empty_slot.hash_ptr = key_hash;
		return empty_slot;
	}

	return (HashMapEntry) { 0 };
}

Error hashmap_reserve_additional(HashMap *self, size_t additional) {
	// Careful! We have to take tombstones into account here---for example, if this
	// map's half full of tombstones, adding half of our capacity would make the map
	// three-quarters full (average case) or entirely full (worst case).

	// For now, just require room for tombstones as well.
	size_t new_cap = self->values_count + self->tombstones_count + additional;
	if (new_cap <= self->values_count) return ERR_OUT_OF_MEMORY;

	// Make sure `new_cap` didn't overflow.
	// This is safe because unsigned integer overflow is defined to wrap.
	assert(new_cap >= self->values_count);

	new_cap += new_cap / 4;

	// Make sure `new_cap` didn't overflow, again.
	// We know `values_count + additional` didn't overflow.
	assert(new_cap >= self->values_count + additional);

	// Make sure it's a power of two.
	new_cap = next_power_of_two(new_cap);

	// Whoops.
	if (new_cap <= self->cap) return ERR_SUCCESS;

	// We have a 32-bit hash function; more capacity is useless.
	if (new_cap > UINT32_MAX) return ERR_OUT_OF_MEMORY;

	// Create a new hashmap and move all elements of the old one into it.
	HashMap prev = *self;

	self->hashes =	malloc(sizeof(uint32_t) * new_cap);
	self->keys =	malloc(sizeof(Slice) * new_cap);
	self->values =	malloc(self->value_size * new_cap);

	if (
		self->hashes == NULL ||
		self->keys == NULL ||
		self->values == NULL
	) {
		free(self->hashes);
		free(self->keys);
		free(self->values);

		// Copy all of `prev` back into `self`.
		*self = prev;

		return ERR_OUT_OF_MEMORY;
	}

	memset(self->hashes,	0, sizeof(uint32_t) * new_cap);
	memset(self->keys,	0, sizeof(Slice) * new_cap);
	memset(self->values,	0, self->value_size * new_cap);

	self->values_count = 0;
	self->tombstones_count = 0;
	self->cap = new_cap;

	for (size_t slot = 0; slot < prev.cap; slot++) {
		uint32_t prev_hash = prev.hashes[slot];

		// We only reinsert actual values;
		if (prev_hash == HASHMAP_HASH_SENTINEL_EMPTY) continue;
		if (prev_hash == HASHMAP_HASH_SENTINEL_TOMBSTONE) continue;

		HashMapEntry old_entry = hashmap_construct_entry(&prev, slot, true);

		HashMapEntry new_entry = hashmap_get_or_put_assume_capacity(self, *old_entry.key_ptr, true);
		assert(!new_entry.occupied);

		*new_entry.hash_ptr = *old_entry.hash_ptr;
		*new_entry.key_ptr = *old_entry.key_ptr;

		memcpy(new_entry.value_ptr, old_entry.value_ptr, self->value_size);
	}

	return ERR_SUCCESS;
}

HashMapEntry hashmap_get(HashMap *self, Slice key) {
	return hashmap_get_or_put_assume_capacity(self, key, false);
}

Error hashmap_put(HashMap *self, Slice key, HashMapEntry *out_entry) {
	set_undefined(out_entry, sizeof(*out_entry));

	Error err;
	
	err = hashmap_reserve_additional(self, 1);
	if (err != ERR_SUCCESS) return err;

	*out_entry = hashmap_get_or_put_assume_capacity(self, key, true);
	return ERR_SUCCESS;
}

// To iterate over a hashmap, create a zero-initialized HashMapIterator and call
// `hashmap_next` until it returns an entry with `occupied` set to `false`.
HashMapEntry hashmap_next(HashMap *self, HashMapIterator *it) {
	while (true) {
		// There aren't any more items.
		if (it->index >= self->cap) return (HashMapEntry) { 0 };

		if (
			self->hashes[it->index] == HASHMAP_HASH_SENTINEL_EMPTY ||
			self->hashes[it->index] == HASHMAP_HASH_SENTINEL_TOMBSTONE
		) {
			it->index++;
		}

		break;

	}

	HashMapEntry entry = hashmap_construct_entry(self, it->index, true);
	it->index++;

	return entry;
}
