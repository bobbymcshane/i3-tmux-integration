#ifndef INCLUDED_PARSE_H
#define INCLUDED_PARSE_H
#include <glib/gprintf.h>

char * unescape(char *orig);
gchar* tmux_layout_to_i3_layout( char* tmux_layout );

#endif /* #ifndef INCLUDED_PARSE_H */
