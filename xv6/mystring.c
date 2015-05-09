#include <stdio.h>
#include <ctype.h>

// judge whether str is a pure number, return 0 if not
int isnum(char *s)
{
	for(; *s; s++)
		if(!isdigit(*s))
			return 0;
	return 1;
}

// judge whether str is a string, return 0 if not
int isstr(char *s)
{
	for(; *s; s++)
		if(isalpha(*s))
			return 1;
	return 0;
}

// return the number of strings in cmdline
// exclude string starting with hyphen '-'
int cmd_has_str(int argc, char *cmd[])
{
	int i;
	int counter = 0;

	for (i=1; i < argc; i++) {
		if (cmd[i][0] == '-' && isalpha(cmd[i][1])) {  // skip -[*]
			continue;
		}

		if (isstr(cmd[i])) {
			counter++;
		}
	}

	return counter;
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



int
main(int argc, char *argv[])
{
	int i;
	char *dev = "1";  // default dev is 1, unless set by -d DEV
	int fd;

	if (argc < 2) {
		printf(1, "Usage: mystat [-d DEV] [-f path] [FILE/DIR]+\n");
		exit(1);
	}

	int num = isnum(argv[1]);
	printf("input is %s, num=%d\n", argv[1], num);
	int str = isstr(argv[1]);
	printf("input is %s, str=%d\n", argv[1], str);
	int hasstr = cmd_has_str(argc, argv);
	printf("cmdline hasstr=%d\n", hasstr);

	return 0;
}
