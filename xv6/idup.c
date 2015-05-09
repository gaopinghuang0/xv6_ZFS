#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"


int
main(int argc, char *argv[])
{

  int r;
  if(argc < 3){
    printf(1, "usage: idup [PATH] [ndittos: 1 or 2]\n");
    exit();
  }

  int has_force = parse_cmd(argc, argv, "-f");

  if (has_force) {
	  // has -f, remove the dittos and re-duplicate
	  // TODO: add a sys_iunlink()

	  exit();
  }



  if(hasdittos(argv[1]) == 0){  // useful function, so defined in ulib.c
    r = duplicate(argv[1], atoi(argv[2]));
    if(r < 0){
	printf(1,"Something went wrong with duplicate\n");
    }
  }
  else{
    printf(1, "Inode already has ditto inodes\n");
  }


  exit();
}
