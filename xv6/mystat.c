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

	if (fstat(fd, &st) < 0) {
		printf(2, "mystat pinode: open error\n");
		close(fd);
		return;
	}
	printf(1, "inum ch1 ch2 checksum\n");
	printf(1, "%d  %d  %d  %x\n", st.ino, st.child1, st.child2, st.checksum);
}

// return index of cmd if exists pattern
// or return 0 if not exists pattern
int parse_cmd(int argc, char *cmd[], const char *pattern)
{
	int i;

	for (i=1; i < argc; i++) {
		if (strcmp(cmd[i], pattern) == 0) {
			return i;
		}
	}

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
		exit();
	}

	return fd;
}

int
main(int argc, char *argv[])
{
	int i;
	char *dev = "1";  // default dev is 1, unless set by -d DEV
	int fd;

	if (argc < 2) {
		printf(1, "Usage: mystat [-d DEV] [-f path] [FILE/DIR]+\n");
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

	int has_force = parse_cmd(argc, argv, "-f");
	int has_dev = parse_cmd(argc, argv, "-d");

	// has -d, the next is devnum
	if (has_dev) {
		dev = strcpy(dev, argv[has_dev+1]);
		printf(1, "dev num is %s\n", argv[has_dev+1]);
	}

	for (i = 1; i < argc; i++) {
		if (i == has_dev) {
			i++;  // skip the devnum
			continue;
		}

		if (has_force && has_force == i) {  //
			char *path = argv[has_force + 1];
			printf(1, "start forceopen: %s\n", path);
			fd = forceopen(path, 0);
			if (fd < 0) {
				printf(1, "forceopen %s failed\n", path);
				printf(1, "usage: [-f PATH] not [-f ino]\n");
				exit();
			}
			pinode(fd);
			cat(fd);
			close(fd);
			i++;  // skip the path
		} else {  // normal case, without -f
			fd = mystat_open(dev, argv[i]);
			pinode(fd);
			cat(fd);
			close(fd);
		}
	}


	exit();
}
