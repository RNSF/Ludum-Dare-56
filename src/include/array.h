
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct {
  void *array;
  size_t elementSize;
  size_t used;
  size_t size;
} Array;

Array aCreate(size_t initialCount, size_t elementSize);

size_t aAppend(Array *a, void* element);

void aFree(Array *a);


Array aCreate(size_t initialCount, size_t elementSize) 
{
  Array a;
  a.array = malloc(initialCount * elementSize);
  a.elementSize = elementSize;
  a.used = 0;
  a.size = initialCount;
  return a;
}


size_t aAppend(Array *a, void* element) 
{
  if (a->used == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * a->elementSize);
  }
  memcpy(a->array + a->used * a->elementSize, element, a->elementSize);
  a->used++;

  return a->used;
}

size_t aAppendArray(Array *a, Array *other) 
{
  assert(a->elementSize == other->elementSize);

  if (a->used + other->used > a->size) {
    while (a->used + other->used > a->size) {
      a->size *= 2;
    }
    a->array = realloc(a->array, a->size * a->elementSize);
  }

  memcpy(a->array + a->used * a->elementSize, other->array, other->used * a->elementSize);

  a->used += other->used;

  return a->used;
}

size_t aAppendStaticArray(Array *a, void* other, size_t otherCount) 
{
  if (a->used + otherCount > a->size) {
    while (a->used + otherCount > a->size) {
      a->size *= 2;
    }
    a->array = realloc(a->array, a->size * a->elementSize);
  }

  

  memcpy(a->array + a->used * a->elementSize, other, otherCount * a->elementSize);

  a->used += otherCount;

  return a->used;
}


void aFree(Array *a) 
{
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}

extern inline bool aHas(Array *a, size_t i)
{
  return i >= 0 && i < a->used;
}

extern inline void* aGet(Array *a, size_t i)
{
  assert(aHas(a, i));
  return (a->array + i * a->elementSize);
}


extern inline void* aSet(Array *a, size_t i, void* element)
{
  assert(aHas(a, i));
  return memcpy(a->array + i * a->elementSize, element, a->elementSize);
}

