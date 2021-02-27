// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

// struct run {
//   struct run *next;
// };

struct node
{
    struct node *next;
    struct node *prev;
};

struct{
  struct spinlock lock;
  int use_lock;
  struct node *start;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;

  kmem.start = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct node *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);

if(kmem.start == 0){
  kmem.start = (struct node*)v;
  kmem.start -> next = kmem.start -> prev = kmem.start;
}  
else{
  r = (struct node*)v;
  r->next = kmem.start;
  r->prev = kmem.start->prev;
  kmem.start->prev->next = r;
  kmem.start->prev = r;
  kmem.start = r;
}
  if(kmem.use_lock)
    release(&kmem.lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{  
  struct node *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.start;
  // If linked list not empty
  if(r){
    struct node *last = kmem.start -> prev;
    // If only one node left
    if (kmem.start == last){
      kmem.start = 0;
    }
    else{
      r->next->prev = r->prev;
      r->prev->next = r->next;
      kmem.start = r->next;
    }
    r->next = 0;
    r->prev = 0;  
  }
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}