#include "string_utils.h"

#include <stdlib.h>
#include <string.h>

void *allocate_memory(size_t size) { return calloc(1, size); }

char *string_copy(const char *value) {
  size_t length = strlen(value) + 1;
  char *copy = allocate_memory(length);
  if (copy != NULL) {
    memcpy(copy, value, length);
  }
  return copy;
}
