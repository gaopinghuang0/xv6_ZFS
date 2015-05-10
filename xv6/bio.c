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
// 
// The implementation uses three state flags internally:
// * B_BUSY: the block has been returned from bread
//     and has not been passed back to brelse.  
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "fs.h"
#include "buf.h"

// A Queue (A LRU collection of Queue Nodes)
struct Queue {
	struct spinlock lock;
	struct buf buf[NBUF];
	uint count;  // Number of filled blocks

	struct buf *front, *rear;
} bcache;

// A hash table (Collection of pointers to cache block)
typedef struct Hash_t {
  uint capacity;  // how many blocks in total
  struct buf *htable[HASHSIZE];  // an array of buf nodes
} Hash_t;

Hash_t *hash_t;
// TODO: use open addressing, Robin Hood Hashing


void* malloc(uint);
void free(void*);

// hash function helper
static uint _hash_func(uint blockno, uint hashsize)
{
	return (blockno % hashsize);
}

// hash function
static inline uint hash_func(uint blockno)
{
	return _hash_func(blockno, (uint)HASHSIZE);
}

// A utility function to create an empty queue
struct Queue initQueue(void)
{
	struct Queue queue;
	struct buf *b;

	// The queue is empty
	queue.count = 0;
	queue.front = queue.rear = NULL;

	for (b = queue.buf; b < queue.buf+NBUF; b++) {
		b->dev = -1;
	}

	return queue;
}

// A utility function to create an empty Hash of given capacity
Hash_t *initHash(void)
{
	// allocate memory for hash table
	Hash_t *hash_t = (Hash_t *) malloc(sizeof(Hash_t));
	hash_t->capacity = (uint)HASHSIZE;

	// create an array of pointers for refering cache blocks
	hash_t->htable = (struct buf **)malloc(hash_t->capacity * sizeof(struct buf *));

	// initialize all hash entries as empty
	int i;
	for (i=0; i<hash_t->capacity; i++)
		hash_t->htable[i] = NULL;

	return hash_t;
}

// A utility function to create an empty block buf node
struct buf* newNode(uint blockno)
{
	struct buf *temp = (struct buf *)malloc(sizeof(struct buf));
	temp->blockno = blockno;  // duplicate with bget
	temp->dev = -1;   // new node initializes to 1
	temp->prev = temp->next = NULL;

	return temp;
}

static struct buf* blookup(struct Queue *queue, uint dev, uint blockno)
{
	struct buf *b;
	uint bval = hash_func(blockno);
	b = hash_t->htable[bval];
	// TODO: handle collision and wander to next location
	if (b != NULL && b->dev == dev && b->blockno == blockno) {
		return b;
	}

	return NULL;
}

int isQueueFull(struct Queue *queue)
{
	return queue->count == (uint)NBUF;
}

int isQueueEmpty(struct Queue *queue)
{
	return queue->rear == NULL;
}

void bdequeue(struct Queue *queue)
{
	if (isQueueEmpty(queue)) return;

	if (queue->front == queue->rear)
		queue->front = NULL;

	struct buf *temp = queue->rear;
	queue->rear = queue->rear->prev;

	if (queue->rear)
		queue->rear->next = NULL;

	free(temp);

	queue->count--;
}

// A function to add a page with given blockno to
// both queue and hash
static struct buf* benqueue(struct Queue *queue, uint dev, uint blockno)
{
	struct buf *b;
	// if all queue are full, remove block at the rear
	if (isQueueFull(queue)) {
		// remove block from hash and queue
		uint rearbno = queue->rear->blockno;
		b = blookup(queue, dev, rearbno);
		b = NULL;
		bdequeue(queue);
	}

	// find a empty node with the given blockno
	// add it to the front of the queue
	b = newNode(blockno);
	b->next = queue->front;

	if (isQueueEmpty(queue)) {
		queue->rear = queue->front = b;
	} else {
		queue->front->prev = b;
		queue->front = b;
	}

	uint bval = hash_func(blockno);
	hash_t->htable[bval] = b;

	queue->count++;

	return b;
}

void
binit(void)
{

  bcache = initQueue();

  initlock(&bcache.lock, "bcache");

//PAGEBREAK!
  hash_t = initHash();
  cprintf("hash_t size=%d\n", hash_t->capacity);

}


// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return B_BUSY buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

 loop:
  // Is the block already cached?
	b = blookup(&bcache, dev, blockno);
	if (b != NULL) {  // cached
	  if(!(b->flags & B_BUSY)){
		b->flags |= B_BUSY;
		release(&bcache.lock);
		return b;
	  }
	  sleep(b, &bcache.lock);
	  goto loop;
	}

  // Not cached; enqueue to queue and hash table,
  // recycle and return new buf.
  // "clean" because B_DIRTY and !B_BUSY means log.c
  // hasn't yet committed the changes to the buffer.
	b = benqueue(&bcache, dev, blockno);
    if((b->flags & B_BUSY) == 0 && (b->flags & B_DIRTY) == 0){
      b->dev = dev;
      b->blockno = blockno;
      b->flags = B_BUSY;
      release(&bcache.lock);
      return b;
    }

  panic("bget: no buffers");
}

// Return a B_BUSY buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!(b->flags & B_VALID)) {
    iderw(b);
  }
  return b;
}

// Write b's contents to disk.  Must be B_BUSY.
void
bwrite(struct buf *b)
{
  if((b->flags & B_BUSY) == 0)
    panic("bwrite");
  b->flags |= B_DIRTY;
  iderw(b);
}

// Release a B_BUSY buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if((b->flags & B_BUSY) == 0)
    panic("brelse");

  acquire(&bcache.lock);

	if (b != bcache.front) {  // b is not front of queue
		// unlink requested block from its current location
		// in queue
		b->prev->next = b->next;
		if (b->next)
			b->next->prev = b->prev;

		// if the requested block page is rear, then change
		// rear as this node will be moved to front
		if (b == bcache.rear){
			bcache.rear = b->prev;
			bcache.rear->next = NULL;
		}

		// put the requested block before current front
		b->next = bcache.front;
		b->prev = NULL;

		// change prev of current front
		b->next->prev = b;

		// change front to the requested block
		bcache.front = b;
	}

  b->flags &= ~B_BUSY;
  wakeup(b);

  release(&bcache.lock);
}


//PAGEBREAK!
// Blank page.

