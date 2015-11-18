#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include "esc.h"

char* echar(char *dst, int c, int nextc)
{
  if (c == '\'' ) {
    *dst++ = '\'';
    *dst++ = '"';
    *dst++ = '\'';
    *dst++ = '"';
  }
  else if (c == ';') {
    *dst++ = '\\';
  }
  *dst++ = c;
  *dst = '\0';
  return (dst);
}

int estr(char *dst, const char *src)
{
	char c;
	char *start;

	for (start = dst; (c = *src);)
		dst = echar(dst, c, *++src);
	*dst = '\0';
	return (dst - start);
}

int aestr(char **outp, const char *src)
{
	char *buf;
	int len, serrno;

	buf = calloc(4, strlen(src) + 1);
	if (buf == NULL)
		return -1;
	len = estr(buf, src);
	serrno = errno;
	*outp = realloc(buf, len + 1);
	if (*outp == NULL) {
		*outp = buf;
		errno = serrno;
	}
	return (len);
}
