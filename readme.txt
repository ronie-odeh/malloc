malloc function, this is a code for malloc function which manages the memory and freeing memory for user.
the functions created are smalloc (behaves like malloc), scalloc (behaves like calloc), sfree (behaves like free), srealloc (behaves like realloc).
1)my implementation maintains a sortedlist (by size in ascending order, and if sizes are equal, then sort by the memory address)
 of all the free memory regions, such that allocations will use the ‘tightest’ fit possible i.e. the memory region with the minimal
 size that can fit the requested allocation.
2)if a pre-allocated block is reused and is large enough, the function will cut the block into two smaller blocks with two separate meta-data structs.
 One will serve the current allocation, and another will remain unused for later (marked free and added to the list).
 Definition of “large enough”:
 After splitting, the remaining block (the one that is not used) has at least 128bytes of free memory, excludingthe size of your meta-data structure.
3)if one adjacent block (next or previous) was free, the function will automatically combine both free blocks (the current one and the adjacent one) into one large free block.
 On the corner case where both the next and previous blocks are free, you should combine all 3 of them into one large block.
4)“Wilderness” chunk is the topmost allocated chunk.
  if:
  1.A new request has arrived, and no free memory chunk was found big enough.
  2.And the wilderness chunk is free.
  Then enlarge the wilderness chunk enough to store the new request.  
5)used mmap()and munmap()instead of sbrk()for memory allocation unit. Used this only for allocations that require 128kb space or more (128*1024 B).
6)detecting (but not preventint) heap overflows using “cookies” –32bit integers that are placedin the metadata of each allocation. If an overflow happens,
 the cookie value will change and we can detect this beforeaccessing the allocation’s metadataby comparing the cookie value with the expected cookie value.
 Note: cookie values are randomized.
 In case of overwrite detection,  exit(0xdeadbeef) immediately called, as the process memory is corrupted and it cannot continue(not recommendedin practice).
 