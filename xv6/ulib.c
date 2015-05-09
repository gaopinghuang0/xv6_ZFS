#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint
strlen(char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

void*
memset(void *dst, int c, uint n)
{
  stosb(dst, c, n);
  return dst;
}

char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

int
stat(char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

void*
memmove(void *vdst, void *vsrc, int n)
{
  char *dst, *src;
  
  dst = vdst;
  src = vsrc;
  while(n-- > 0)
    *dst++ = *src++;
  return vdst;
}

int hasdittos(char *path)
{
	int fd;
	struct stat st;
	fd = forceopen(path, O_RDONLY);
	fstat(fd, &st);
	close(fd);
	//printf(1, "st.child1=%d\n", st.child1);
	return st.child1 > 0 ? 1 : 0;
}

static inline int isdigit(int ch)
{
	return (ch >= '0') && (ch <= '9');
}

static inline int isalpha(int ch)
{
	return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z'));
}

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
