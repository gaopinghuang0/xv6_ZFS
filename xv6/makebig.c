#include"types.h"
#include"stat.h"
#include"user.h"
#include"fcntl.h"


#define BIGFILESIZE  15
#define MIDIUMFILESIZE  10
#define SMALLFILESIZE  5


char* fill_buf(char *buf, char *s, int size)
{
	int i, j;
	char *cbuf = buf;
	for (i = 0; i < size; i += strlen(s)) {
		for (j = 0; j < strlen(s); j++) {
			cbuf[i+j] = s[j];
		}
	}
	return (char *)buf;
}

void print_buf(char buf[], int size)
{
	int i;
	for (i=0; i<size; i++)
		printf(1, "%d", buf[i]);
	printf(1, "\n");
}	

int main(int argc, char* argv[])
{
	char buf[512];
	int fd;
	int size[3] = {BIGFILESIZE, MIDIUMFILESIZE, SMALLFILESIZE};
	char *filename[3] = {"big.file", "medium.file", "small.file"};
	int i, j, k;

	if (argc < 2) {
		printf(1, "Usage: makebig [char]\n");
		exit();
	}

	for (i = 0; i < 3; i++) {
		fd = open(filename[i], O_CREATE | O_WRONLY);
		if (fd < 0) {
			printf(2, "creating %dth file falied\n", i);
			close(fd);
			continue;  // skip current file
		}
		for (j = 0; j < size[i]; j++) {
			for(k = 0; k < sizeof(buf); k++){
			    buf[k] = 'a' + ((i+j+k)*(int)argv[1] % 26); // generate unregular char
			}
			// introduce unexpected char
			*(int*)buf = (int)argv[1] % (1 + j);
			// fill_buf(buf, argv[1], sizeof(buf));
			// print_buf(buf, sizeof(buf));
			int cc = write(fd, buf, sizeof(buf));
			if (cc <= 0) break;
		}
		close(fd);
	}

	exit();

}
