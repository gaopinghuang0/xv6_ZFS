#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

char buf[512];

void
cat(int fd)
{
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, n);
  if(n < 0){
    printf(1, "mystat cat: read error\n");
    exit();
  }
}


void
pinode(int fd)
{
	struct stat st;
	int fd;

	if ((fd = iopen(dev, iint)) < 0) {
		printf(2, "mystat pinode: cannot open inode %d on dev %d\n", iint, dev);
		return;
	}

	if (fstat(fd, &st) < 0) {
		printf(2, "mystat pinode: cannot stat inode %d on dev %d (fd=%d)\n", iint, dev, fd);
		close(fd);
		return;
	}

	close(fd);

	printf(1, "%d  %d  %d  %x\n", iint, st.child1, st.child2, st.checksum);
}

// return index of cmd if exists pattern
// or return 0 if not exists pattern
int parse_cmd(int argc, char *cmd, char *pattern)
{


	return 0;
}

// open file, but not close, return fd
int mystat_open(char *devnum, char *inum)
{
	int fd;
	int dev, iint;

	dev = atoi(devnum);
	iint = atoi(inum);

	if ((fd = iopen(dev, iint)) < 0) {
		if (fd == E_CORRUPTED) {
			printf(2, "mystat: inode %d is corrupted.\n", iint);
		} else {
			printf(2, "mystat: cannot open inode %d on dev %d\n", iint, dev);
		}
		exit(1);
	}

	return fd;
}

int
main(int argc, char *argv[])
{
	int i;
	char dev = "1";  // default dev is 1, unless set by -d DEV
	int fd;

	if (argc < 2) {
		printf(1, "Usage: mystat [-d DEV] [-f] [FILE/DIR]+\n");
		exit();
	}

	if (argc == 2) {
		// normal case, default dev
		fd = mystat_open(dev, argv[1]);
		pinode(fd);
		cat(fd);
		close(fd);
		exit();
	}
//
//	int has_force = parse_cmd(argc, argv, "-f");
//	int has_dev = parse_cmd(argc, argv, "-d");
//
//	for (i = 1; i < argc; i+=2)
//		pinode(argv[i], argv[i+1]);

	exit();
}
