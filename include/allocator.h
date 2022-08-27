#ifndef _AM_ALLOCATOR_H
#define _AM_ALLOCATOR_H

#define NULL ((void *)0)

typedef unsigned int MEMTYPE;

extern MEMTYPE *mem;
extern MEMTYPE memSize;
extern MEMTYPE avail; // 1st index of the 1st free range

void meminit(void *ptr, unsigned size);
void *malloc(unsigned size);
void free(void *ptr);

#endif