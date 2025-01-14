#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED

#include <stdlib.h>

#define ALLOCATE(type) (type *)calloc(1, sizeof(type))
#define ALLOCATE_ARRAY(type, n) (type *)calloc((n), sizeof(type))
#define ALLOCATE_SIZED(size) calloc(1, (size))
#define ALLOCATE_SIZED_ARRAY(size, n) calloc((n), (size))
#define FREE(ptr) free(ptr)

#endif
