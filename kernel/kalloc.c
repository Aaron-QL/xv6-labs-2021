// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint8 refcount[(PHYSTOP - KERNBASE) / PGSIZE];
#define PA2IDX(pa) (((uint64)pa - KERNBASE) / PGSIZE)

void incref(uint64 pa)
{
  uint pn = PA2IDX(pa);
  acquire(&kmem.lock);
  if (pa >= PHYSTOP || refcount[pn] == 0) {
    panic("incref");
  }
  refcount[pn]++;
  release(&kmem.lock);
}

uint8 getref(uint64 pa)
{
  if (pa >= PHYSTOP) {
    panic("getref");
  }
  return refcount[PA2IDX(pa)];
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    refcount[PA2IDX(p)] = 1;
    kfree(p);
  }
}


// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  uint pn = PA2IDX(pa);
  if (refcount[pn] == 0) {
    printf("pa: %p, ref: %d\n", pa, refcount[pn]);
    panic("kfree: refcount == 0");
  }
  if (--refcount[pn] == 0) {
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    uint pn = PA2IDX(r);
    if (refcount[pn] != 0) {
      panic("kalloc: refcount != 0");
    }
    refcount[pn] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
