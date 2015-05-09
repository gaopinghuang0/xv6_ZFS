#include"types.h"
#include"stat.h"
#include"fcntl.h"
#include"user.h"
int main(){

      int child;
      int fd1,fd2;
      char buf1[512];
      char buf2[512];
      child = fork();	
      if(child == 0){
      		fd1 = open("child.file",O_CREATE | O_WRONLY);
      		*(int*)buf1 = 'a';
	        int cc = write(fd1, buf1, sizeof(buf1));
		if(cc<=0)exit();

		close(fd1);
      }
      else{
      		fd2 = open("parent.file",O_CREATE | O_WRONLY);
     		*(int*)buf2 = 'b';
		int pp = write(fd2, buf2, sizeof(buf2));
		if(pp<=0)exit();
		close(fd2);
     }
    
     exit();


}
