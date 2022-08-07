// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(id) (id % NBUCKET)

struct bucket {
  struct spinlock lock;
  struct buf head;
};


struct {
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char lockname[NBUCKET][16]; // 为每个桶的锁声明空间用来存储

  for (int i = 0; i < NBUCKET; i++) {
    snprintf(lockname[i], sizeof(lockname[i]), "bcache_%d", i); // 为每个桶的锁生成名字
    initlock(&bcache.buckets[i].lock, lockname[i]); // 初始化每个桶的锁
    bcache.buckets[i].head.next = &bcache.buckets[i].head; // 设置head为自己指向自己的环，表示空
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
  }

  for (b = bcache.buf; b < bcache.buf + NBUF; b++) { // 用头插法把所有的buf都插入到第一个桶中
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bid = HASH(blockno);

  // 先尝试在对应的哈希桶中查找有没有已经缓存了的
  acquire(&bcache.buckets[bid].lock);
  for(b = bcache.buckets[bid].head.next; b != &bcache.buckets[bid].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){ // 如果有直接返回
      b->refcnt++;

      // 更新使用时间戳
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[bid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 如果没有则根据LRU原则找个空buffer
  b = 0;
  struct buf *tmp;
  // 先从当前桶开始找
  for (int i = 0, tid = bid; i < NBUCKET; i++, tid = (tid+1) % NBUCKET) {
    if (tid != bid) { // 如果找的不是自己的桶，对其他桶加锁可能会产生死锁
      if (!holding(&bcache.buckets[tid].lock)) { // 所以需要先判断下另一个桶是否持有了锁
        acquire(&bcache.buckets[tid].lock); // 如果没持有，再尝试加锁。这种方式只能减小发生死锁的概率，并不能完全避免。不过实验的要求中说这种情况不会发生
      } else {
        continue; // 如果持有了，则放弃对这个桶的检查
      }
    }
    //遍历这个桶，尝试找到一个空buf
    for (tmp = bcache.buckets[tid].head.next; tmp != &bcache.buckets[tid].head; tmp = tmp->next) {
      // refcnt == 0 表示空buf
      // b==0 是遇到第一个空buf的情况
      // tmp->timestamp < b->timestamp是选择更久远的buf
      if (tmp->refcnt == 0 && (b == 0 || tmp->timestamp < b->timestamp)) {
        b = tmp;
      }
    }
    if (b) { // 如果找到了空buf
      if (tid != bid) { // 如果是从别的桶里找到的，需要把它挪到自己的桶里
        // 连接tmp的前后连个节点，实现删除效果
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.buckets[tid].lock);

        // 放入自己的桶里
        b->next = bcache.buckets[bid].head.next;
        b->prev = &bcache.buckets[bid].head;
        bcache.buckets[bid].head.next->prev = b;
        bcache.buckets[bid].head.next = b;
      }
      // 初始化buf
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[bid].lock);
      acquiresleep(&b->lock);
      return b;
    } else { // 如果在当前桶没找，释放锁，去下一个桶继续找
      if (tid != bid) {
        release(&bcache.buckets[tid].lock);
      }
    }
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bid = HASH(b->blockno);

  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  // 更新使用时间戳
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);

  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[bid].head.next;
    b->prev = &bcache.buckets[bid].head;
    bcache.buckets[bid].head.next->prev = b;
    bcache.buckets[bid].head.next = b;
  }
  
  release(&bcache.buckets[bid].lock);
}

void
bpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt++;
  release(&bcache.buckets[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  release(&bcache.buckets[bid].lock);
}


