// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#include "fs.h"

char *argv[] = { "sh", 0 };

int name_be_banned(struct dirent *de) {
	static char *ban_name[] = {"console", "stressfs", "usertests", "mystat", "idup", "makebig"};
	int size = sizeof(ban_name) / sizeof(ban_name[0]);
	int i;

	for (i=0; i<size; i++) {
		if (strcmp(de->name, ban_name[i]) == 0) return 1;
	}
	return 0;
}


int
main(void)
{
  int pid, wpid;

  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1);
    open("console", O_RDWR);
  }
  dup(0);  // stdout
  dup(0);  // stderr

  int root_fd;
  char buf[512];
  struct dirent *de;

  root_fd = open(".", O_RDONLY);
  read(root_fd, buf, sizeof(buf));
  close(root_fd);

  de = (struct dirent *)buf;

  while (*(de->name)) {
	  if ((*(de->name)) == '.' || name_be_banned(de)) {
		  de++;
		  continue;  // skip these files
	  }

	  if(hasdittos(de->name) == 0) {  // create two copies for the first level files
		  printf(1, "Creating backup copy for: %s \n", de->name);
		  duplicate(de->name, 2);
	  }
	  de++;
  }


  for(;;){
    printf(1, "init: starting sh\n");
    pid = fork();
    if(pid < 0){
      printf(1, "init: fork failed\n");
      exit();
    }
    if(pid == 0){
      exec("sh", argv);
      printf(1, "init: exec sh failed\n");
      exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
      printf(1, "zombie!\n");
  }
}
