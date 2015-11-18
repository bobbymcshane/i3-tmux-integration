#ifndef _ESC_H_
#define	_ESC_H_
#include <sys/cdefs.h>

__BEGIN_DECLS
char	*echar(char *, int, int);
int	estr(char *, const char *);
int	aestr(char **, const char *);

__END_DECLS

#endif /* !_ESC_H_ */
