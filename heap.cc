#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "defs.hh"
#include "heap.hh"

Heap::~Heap()
{
  if(array)
    {
      free(array);
      array = 0;
      n = 0;
      N = 0;
    }
}

void Heap::upheap(unsigned int index)
{
  DEBUG_ASSERT(n >= 1);
  DEBUG_ASSERT(index >= 1 && index <= n);
  const unsigned int v = array[index];
  array[0] = 0;
  while(array[index/2] > v)
    {
      array[index] = array[index/2];
      index = index/2;
    }
  array[index] = v;
}

void Heap::downheap(unsigned int index)
{
  const unsigned int v = array[index];
  while(index <= n/2)
    {
      unsigned int new_index = index + index;
      if((new_index < n) and (array[new_index] > array[new_index+1]))
	new_index++;
      if(v <= array[new_index])
	break;
      array[index] = array[new_index];
      index = new_index;
    }
  array[index] = v;
}

void Heap::init(unsigned int size)
{
  DEBUG_ASSERT(size > 0);
  if(size > N)
    {
      if(array)
	free(array);
      array = (unsigned int*)malloc((size + 1) * sizeof(unsigned int));
      N = size;
    }
  n = 0;
}

void Heap::insert(unsigned int v)
{
  DEBUG_ASSERT(n < N);
  array[++n] = v;
  upheap(n);
}

unsigned int Heap::remove()
{
  DEBUG_ASSERT(n >= 1 && n <= N);
  const unsigned int v = array[1];
  array[1] = array[n--];
  downheap(1);
  return v;
}

