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


int fsfd;

int corrupt(uint inum, uint n, int pct);
void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint inum, struct dinode *ip);
void rsect(uint sec, void *buf);

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
	int i, pct;

	// open img file
	if (argc < 4) {
		fprintf(stderr, "Usage: crpfs *.img inum1 [inum2..] percent\n");
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
		int counter = 0;

		inum = atoi(argv[i]);
		rinode(inum, &din);
		size = xint(din.size);
		printf("Going to corrupt inode %d with size %d bits \n", inum, size * 8);
		counter = corrupt(inum, size, pct);  // corrupt the inode
		// print corrupt stat
		printf("Corrupt 1 in every %d bits, in total %d bits\n", pct, counter);
	}

	exit(0);
}

// return 1 only random eq a multiple of p
int toss(int p)
{
	return (rand() % p) == 0 ? 1 : 0;
}

int set_bits(char *cbuf, uint n, int pct)
{
	uint i, j;
	char bit = 0x01;
	int counter = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < 8; j++) {
			if (toss(pct)) {
				counter++;
				*(cbuf + i) ^= bit << j;
			}
		}
	}
	return counter;
}


int corrupt(uint inum, uint n, int pct)
{
	//printf("in corrupt, inum=%d, size=%d, pct=%d\n", inum, n, pct);
	struct dinode din;
	uint fbn, x;
	uint off, tot, m;
	uint indirect[NINDIRECT];
	char buf[512], *cbuf;
	int counter = 0;

	// read inode
	rinode(inum, &din);

	off = 0;
	for (tot=0; tot<n; tot+=m, off+=m) {
		fbn = off / 512;  // find the block num in inode
		assert(fbn < MAXFILE);

		// get sector number
		if (fbn < NDIRECT) {
			x = xint(din.addrs[fbn]);
		} else {
			rsect(xint(din.addrs[NDIRECT]), (char *)indirect);
			x = xint(indirect[fbn - NDIRECT]);
		}
		m = min(n, (fbn + 1) * 512 -off);
		rsect(x, buf);	// read data
		cbuf = (char *) &buf;
		counter += set_bits(cbuf, m, pct);
		wsect(x, buf);  // write data
	}

	// set the inode with new din
	winode(inum, &din);
	return counter;
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

