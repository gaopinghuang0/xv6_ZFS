#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#include "param.h"

#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#define min(a, b) ((a) < (b) ? (a) : (b))


#define NINODES  200
// Disk layout:
// [ boot block | sb block | inode blocks | bit map | data blocks | log ]

int nbitmap = FSSIZE/(BSIZE * 8) + 1;
int nblocks;  // Number of data blocks
int nmeta;    // Number of meta blocks (inode, bitmap, and 2 extra)
int nlog = LOGSIZE;
int ninodeblocks = NINODES / IPB + 1;
//int size = 2048;

int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeblock;
uint freeinode = 1;


int corrupt(uint inum, uint n, int pct);
void balloc(int);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);
uint ichecksum(struct dinode *din);
void rblock(struct dinode *din, uint bn, char * dst);
int readi(struct dinode *din, char * dst, uint off, uint n);
void copy_dinode_content(struct dinode *src, uint dst);

// convert to intel byte order
ushort
xshort(ushort x)
{
  ushort y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

uint
xint(uint x)
{
  uint y;
  uchar *a = (uchar*)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

int
main(int argc, char *argv[])
{
//  int i, cc, fd;
//  uint rootino, inum, off;
//  struct dirent de;
//  char buf[BSIZE];
//  struct dinode din, din2;
//  unsigned int checksum;
//
//
//  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");
//
//  if(argc < 3){
//    fprintf(stderr, "Usage: corrupt fs.img inum percent...\n");
//    exit(1);
//  }
//
//  assert((BSIZE % sizeof(struct dinode)) == 0);
//  assert((BSIZE % sizeof(struct dirent)) == 0);
//
//  fsfd = open(argv[1], O_RDWR, 0666);
//  if(fsfd < 0){
//    perror(argv[1]);
//    exit(1);
//  }
//
//  nmeta = 2 + ninodeblocks + nbitmap;
//  nblocks = FSSIZE - nlog - nmeta;

	int i, pct;


	// open img file
	if (argc < 4) {
		fprintf(stderr, "Usage: corruptfs fs.img inum1 [inum2..] percent\n");
		exit(1);
	}

	fsfd = open(argv[1], O_RDWR, 0666);
	if (fsfd < 0) {
		perror(argv[1]);
		exit(1);
	}

	// percent should always be the last argv
	pct = atoi(argv[argc-1]);
	printf("Corrupt percent is %d\n", pct);

	srand(time(NULL));
	for (i=2; i < argc-1; i++) {
		// find the inode based on the inum, here the img file is just like the memory
		// we can still build some function based on fs.h
		struct dinode din;
		uint inum, size;

		inum = atoi(argv[i]);
		rinode(inum, &din);
		size = xint(din.size);
		printf("Going to corrupt inode %d with size %d bits \n", inum, size * 8);
		corrupt(inum, size, pct);  // corrupt the inode

		// print corrupt stat
	}

  
  exit(0);
}


int corrupt(uint inum, uint n, int pct)
{
	printf("in corrupt, inum=%d, size=%d, pct=%d\n", inum, n, pct);
	return 0;
}


void
wsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, 512) != 512){
    perror("write");
    exit(1);
  }
}

//Inum to block
//You have to add 2 for the boot block and the super block
//After that its just simply indexing into the inode "table"
uint
i2b(uint inum)
{
  return (inum / IPB) + 2;
}

void
winode(uint inum, struct dinode *ip)
{
  char buf[512];
  uint bn;
  struct dinode *dip;

  bn = i2b(inum);
  //read section of the inode table into the buffer
  rsect(bn, buf);
  //find the right dinode
  dip = ((struct dinode*)buf) + (inum % IPB);
  //Set that dinode to the new one
  *dip = *ip;
  wsect(bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
  char buf[512];
  uint bn;
  struct dinode *dip;

  bn = i2b(inum);
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % IPB);
  *ip = *dip;
}

//Abstraction that reads sectors from fs.img
void
rsect(uint sec, void *buf)
{
  if(lseek(fsfd, sec * 512L, 0) != sec * 512L){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, 512) != 512){
    perror("read");
    exit(1);
  }
}

/* Allocates an inode by incrementing freeinode, returns inum
 * which started at 1(root directory)
 * Then it creates the dinode struct and writes that to fs.img*/
uint
ialloc(ushort type)
{
  uint inum = freeinode++;
  struct dinode din;

  bzero(&din, sizeof(din));
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

//Writes the bitmap
void
balloc(int used)
{
  uchar buf[512];
  int i;

  printf("balloc: first %d blocks have been allocated\n", used);
  assert(used < 512*8);
  bzero(buf, 512);
  for(i = 0; i < used; i++){
    buf[i/8] = buf[i/8] | (0x1 << (i%8));
  }
  printf("balloc: write bitmap block at sector %zu\n", NINODES/IPB + 3);
  wsect(NINODES / IPB + 3, buf);
}


void
iappend(uint inum, void *xp, int n)
{
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[512];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);

  off = xint(din.size);
  while(n > 0){
    fbn = off / 512;
    assert(fbn < MAXFILE);
    if(fbn < NDIRECT){
      if(xint(din.addrs[fbn]) == 0){
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if(xint(din.addrs[NDIRECT]) == 0){
        // printf("allocate indirect block\n");
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      // printf("read indirect block\n");
      // The address just points to a block
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn-NDIRECT]);
    }
    n1 = min(n, (fbn + 1) * 512 - off);
    rsect(x, buf);
    bcopy(p, buf + off - (fbn * 512), n1);
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}

int
readi(struct dinode *din, char *dst, uint off, uint n){
    uint tot, m, fbn;
    char data[BSIZE];
    char *cdata = (char *)data;

    if(xint(din->type) == T_DEV){
	fprintf(stderr, "Reading DEV file. Not implemented in readi in mkfs\n");
	return -1;
    }
    if(off > xint(din->size) || off + n < off){
	return -1;
    }
    if(off + n > xint(din->size)){
	n = xint(din->size) - off;
    }

    for(tot = 0; tot < n;tot +=m, off+=m, dst+=m){
	fbn = off / BSIZE;
	rblock(din, fbn, (char*)data);
	m = min(n - tot, BSIZE - off%BSIZE);
	memmove(dst, cdata + off%BSIZE, m);
    }
    return n;
}

void
rblock(struct dinode *din, uint bn, char *dst){
    uint indirect[NINDIRECT];
    uint addr;
    if(bn < NDIRECT){
	rsect(xint(din->addrs[bn]), dst);
	return;
    }
    bn -= NDIRECT;

    if(bn < NINDIRECT){
	rsect(xint(din->addrs[NDIRECT]), (char*)indirect);
	addr = xint(indirect[bn]);
	rsect(addr, dst);
	return;
    }
}

uint
ichecksum(struct dinode *din){
    unsigned int buf[512];
    char* cbuf = (char*) buf;
    uint n = sizeof(buf);
    uint off = 0;
    uint r, i;
    unsigned int checksum = 0;
    memset((void*) cbuf, 0, n);
    unsigned int * bp;

    while ((r = readi(din, cbuf, off, n)) > 0) {
      off += r;
      bp = (unsigned int *)buf;
      for (i = 0; i < sizeof(buf) / sizeof(uint); i++){
	  checksum ^= *bp;
	  bp++;
      }
      memset((void *) cbuf, 0, n);
    }

    return checksum;
}

void
copy_dinode_content(struct dinode *src, uint dst){
    char buf[512];
    char *cbuf = (char *) buf;
    uint n = sizeof(buf);
    uint off = 0;
    uint r;
    memset((void *) cbuf,0,n);

    while((r = readi(src, cbuf,off,n)) > 0){
	off += r;
	iappend(dst, cbuf, r);
	memset((void *) cbuf,0,n);
    }
}


