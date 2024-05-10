#include "common/hash.h"

// I just made a realization.
//
// A few days ago, I was copying Wikipedia's implementation of FNV-1a. I
// wanted to know if that function was working properly. I didn't care about
// cryptographic security, and realistically even if I copied the wrong constant
// it'd still work, but I still wanted to make sure I got the right numbers.
//
// My immediate action was to look up "fnv-1a example", but that didn't turn up
// anything in the first five seconds.
//
// And there's a niche there, right? So I wanted to write an article for my
// blog, that was just like, "The FNV-1a32 hash of 'FNV-1a32' is 0xECD894E2".
// And so on, for a bunch of hash functions (realistically just all the
// different widths of FNV-1a). And to do that, I wanted to preview it locally.
//
// Previously, I'd been using a hacky one-off Python script to serve my blog
// locally (which still ran, after two years of not touching it), but I recently
// started using a new computer which didn't have that Python script, and it'd
// be a chore to copy a single file over.
//
// So I started writing *this* server, and it'd be nice to have a hash map to
// store HTTP headers and all the static files to host.
//
// And it's in the name, right? Hash map. You need a hash function for a hash
// map.
//
// I'm writing the same hash function.
//
// 
// I hope I copied the right constants from Wikipedia.
uint32_t hash_fnv1a32(Slice slice) {
	uint32_t hash = 0x811C9DC5u;

	for (size_t i = 0; i < slice.len; i++) {
		hash ^= slice.bytes[i];
		hash *= 0x01000193u;
	}

	return hash;
}
