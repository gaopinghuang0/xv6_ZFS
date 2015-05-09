#include"types.h"
#include"stat.h"
#include"user.h"
#include"fcntl.h"

void print_buf(char buf[], int size)
{
	int i;
	for (i=0; i<size; i++)
		printf(1, "%d", buf[i]);
	printf(1, "\n");
}	

int main(int argc, char* argv[])
{
	char buf1[512];
	char buf2[512];
	char buf3[512];
	int fd1,fd2,fd3;
	int size1, size2, size3;
	int i,j,k;

	fd1 = open("big.file", O_CREATE | O_WRONLY);
	if(fd1<0){
		printf(2,"big: can't open big.file for writing \n");
		exit();
	}
	size1 = 2;
	size2 = 2;
	size3 = 1;
	// this is big file
	for(i=0;i<size1;i++){
		*(int*)buf1 = 5;
		printf(1, "fd1\n");
		print_buf(buf1, sizeof(buf1));
		int bb = write(fd1,buf1,sizeof(buf1));
		if(bb<=0)
			break;
	}
	close(fd1);
	// this is medium file
	fd2 = open("medium.file", O_CREATE | O_WRONLY);
	if(fd2<0){
			printf(2,"medium: can't open medium.file for writing \n");
			exit();
		}
	for(j=0;j<size2;j++){
		*(int*)buf2 = j;
		printf(1, "fd2\n");
		print_buf(buf2, sizeof(buf2));
		int mm = write(fd2,buf2,sizeof(buf2));
		if(mm<=0)
			break;
	}
	close(fd2);
	// this is small file
	fd3 = open("small.file", O_CREATE | O_WRONLY);
	if(fd3<0){
			printf(2,"small: can't open small.file for writing \n");
			exit();
		}
	for(k=0;k<size3;k++){
		*(int*)buf3 = *argv[1];
		printf(1, "fd3\n");
		print_buf(buf3, sizeof(buf3));
		int ss = write(fd3,buf3,sizeof(buf3));
		if(ss<=0)
			break;

	}
	close(fd3);
	exit();

}
