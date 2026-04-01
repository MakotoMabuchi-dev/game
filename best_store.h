#ifndef BEST_STORE_H
#define BEST_STORE_H

#include <stdbool.h>
#include <stdint.h>

bool best_store_load_u32(const char *key, uint32_t *value, bool *has_value);
bool best_store_save_u32(const char *key, uint32_t value, bool has_value);

#endif
