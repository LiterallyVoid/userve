#include "types/hashmap.h"

#include "util.h"

void hashmap_init(HashMap *self) {
	set_undefined(self, sizeof(*self));
}

void hashmap_deinit(HashMap *self) {

	set_undefined(self, sizeof(*self));
}
