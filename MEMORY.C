// memory management routines for x36 profiling.
// allocates memory in 1k pages full of void*s.
// Used by configs and commands.
//
// Memory now works in a 2-level tree.
// 256 entries of 256 entries.
// The top level is staticly allocated.
// The second level is dynamically allocated.

//#define DBG 1
#include <wdm.h>
#include <filter.h>


void* get(ptr_block* top_level, unsigned int loc) // Gets a pointer to a location.
{
  unsigned char msb = (loc >> 8) & 0xFF;
  unsigned char lsb = loc & 0xFF;
  if (top_level->ptr[msb] == NULL) {
  	 DbgPrn(("X36F: Get %d %d: No sublevel.\n",msb,lsb));
     return NULL;
  }
  DbgPrn(("X36F: Get %d %d:  %X.\n",msb,lsb,((ptr_block*)(top_level->ptr[msb]))->ptr[lsb]));
  return ((ptr_block*)(top_level->ptr[msb]))->ptr[lsb];
}

void** getloc2(ptr_block* top_level, unsigned int loc) // gets a pointer to a pointer to a location. :)  for writing, allocs as necessary.
{
  unsigned char msb = (loc >> 8) & 0xFF;
  unsigned char lsb = loc & 0xFF;
  if (top_level->ptr[msb] == NULL) { 
	DbgPrn(("X36F: Getloc %d %d: No sublevel, allocating..\n",msb,lsb));  	
  	if ((top_level->ptr[msb] = ExAllocatePool(NonPagedPool, sizeof(ptr_block))) == NULL) {
  		 DbgPrn(("X36F: Getloc %d %d: Allocation failed!\n",msb,lsb));  	
  		 return NULL; 
  	}
  	RtlZeroMemory(top_level->ptr[msb],sizeof(ptr_block));
  	DbgPrn(("X36F: Getloc %d %d: allocated %X.\n",msb,lsb,top_level->ptr[msb]));
  };
  DbgPrn(("X36F: Getloc %d %d:  %X.\n",msb,lsb,&(((ptr_block*)(top_level->ptr[msb]))->ptr[lsb])));  
  return &(((ptr_block*)(top_level->ptr[msb]))->ptr[lsb]);
}

void flush(ptr_block* p) // empties a ptr_block and frees all allocations.
{
  int i;
  for (i=0; i<255; i++)
  {
    if (p->ptr[i] != NULL)
    {
      ExFreePool(p->ptr[i]);
      p->ptr[i] = NULL;
    }
  }
}

void flush2(ptr_block* top_level) // empties a ptr_block of ptr_blocks. :)
{
  int i;
  for (i=0; i<255; i++)
  {
    if (top_level->ptr[i] != NULL)
    {
      flush(top_level->ptr[i]);
      ExFreePool(top_level->ptr[i]);
      top_level->ptr[i] = NULL;
    }
  }
}

