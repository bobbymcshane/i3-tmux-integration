#define _XOPEN_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <json-glib/json-glib.h>
#include <string.h>
typedef unsigned int uint_t;
typedef __u_char u_char;

#define ISODIGIT(c) ((int)(c) >= '0' && (int)(c) <= '7')
char * unescape(char *orig)
{
     char c, *cp, *new = orig;
     int i;

     for (cp = orig; (*orig = *cp); cp++, orig++) {
          if (*cp != '\\')
               continue;

          switch (*++cp) {
          case 'a': /* alert (bell) */
               *orig = '\a';
               continue;
          case 'b': /* backspace */
               *orig = '\b';
               continue;
          case 'e': /* escape */
               *orig = '\e';
               continue;
          case 'f': /* formfeed */
               *orig = '\f';
               continue;
          case 'n': /* newline */
               *orig = '\n';
               continue;
          case 'r': /* carriage return */
               *orig = '\r';
               continue;
          case 't': /* horizontal tab */
               *orig = '\t';
               continue;
          case 'v': /* vertical tab */
               *orig = '\v';
               continue;
          case '\\':     /* backslash */
               *orig = '\\';
               continue;
          case '\'':     /* single quote */
               *orig = '\'';
               continue;
          case '\"':     /* double quote */
               *orig = '"';
               continue;
          case '0':
          case '1':
          case '2':
          case '3': /* octal */
          case '4':
          case '5':
          case '6':
          case '7': /* number */
               for (i = 0, c = 0;
                    ISODIGIT((unsigned char)*cp) && i < 3;
                    i++, cp++) {
                    c <<= 3;
                    c |= (*cp - '0');
               }
               *orig = c;
               --cp;
               continue;
          case 'x': /* hexidecimal number */
               cp++;     /* skip 'x' */
               for (i = 0, c = 0;
                    isxdigit((unsigned char)*cp) && i < 2;
                    i++, cp++) {
                    c <<= 4;
                    if (isdigit((unsigned char)*cp))
                         c |= (*cp - '0');
                    else
                         c |= ((toupper((unsigned char)*cp) -
                             'A') + 10);
               }
               *orig = c;
               --cp;
               continue;
          default:
               --cp;
               break;
          }
     }

     return (new);
}

void tmux_layout_to_i3_layout_impl( char** tmux_layout, JsonBuilder* builder ) {
     GError *err = NULL;
     JsonParser *parser;

     json_builder_begin_object( builder );
     json_builder_set_member_name( builder, "type" );
     json_builder_add_string_value( builder, "con" );

     /* TODO: DO we always have the id? */
     uint_t sx, sy, xoff, yoff;
     uint_t wp_id = 0;
     char wp_id_str[64];

     if (!isdigit((u_char) (**tmux_layout))) {
          printf( "BARF %s\n", (*tmux_layout));
          return;
     }
     size_t n_scanned = sscanf((*tmux_layout), "%ux%u,%u,%u,%u", &sx, &sy, &xoff, &yoff, &wp_id);
     if ( n_scanned = 5 )
          sprintf( wp_id_str, "%u", wp_id );
     else if ( n_scanned == 4 )
          *wp_id_str = '\0';
     else {
          printf( "BLURGH\n");
          return;
     }

     while (isdigit((u_char) (**tmux_layout)))
          (*tmux_layout)++;
     if ((**tmux_layout) != 'x') {
          printf( "AAGH\n");
          return;
     }
     (*tmux_layout)++;
     while (isdigit((u_char) (**tmux_layout)))
          (*tmux_layout)++;
     if ((**tmux_layout) != ',') {
          printf( "OOOF\n");
          return;
     }
     (*tmux_layout)++;
     while (isdigit((u_char) (**tmux_layout)))
          (*tmux_layout)++;
     if ((**tmux_layout) != ',') {
          printf( "OUCH\n");
          return;
     }
     (*tmux_layout)++;
     while (isdigit((u_char) (**tmux_layout)))
          (*tmux_layout)++;
     if ((**tmux_layout) == ',') {
          /* TODO: Does the saved go before or after the increment? */
          char* saved = (*tmux_layout);
          (*tmux_layout)++;
          while (isdigit((u_char) (**tmux_layout)))
               (*tmux_layout)++;
          if ((**tmux_layout) == 'x') {
               *wp_id_str = '\0';
               (*tmux_layout) = saved;
          }
     }

     /* No need to validate the (*tmux_layout) format string is correct for now */
     /* TODO: Build a json representation of the equivalent i3 layout */
     switch (**tmux_layout) {
          case '\0':
          case '\n':
          case ',':
          case '}':
          case ']':
               if ( *wp_id_str != '\0' ) {
                    json_builder_set_member_name( builder, "mark" );
                    json_builder_add_string_value( builder, wp_id_str );
               }
               json_builder_set_member_name( builder, "swallows" );
               json_builder_begin_array( builder );
               json_builder_begin_object( builder );
               json_builder_set_member_name( builder, "class" );
               json_builder_add_string_value( builder, "^Gnome\\-terminal$" );
               json_builder_end_object( builder );
               json_builder_end_array( builder );
               
               json_builder_end_object( builder );
               //(*tmux_layout)++;
               return;
          case '{':
               json_builder_set_member_name( builder, "layout" );
               json_builder_add_string_value( builder, "splith" );
               json_builder_set_member_name( builder, "nodes" );
               json_builder_begin_array( builder );
               //lc->type = LAYOUT_LEFTRIGHT;
               break;
          case '[':
               json_builder_set_member_name( builder, "layout" );
               json_builder_add_string_value( builder, "splitv" );
               json_builder_set_member_name( builder, "nodes" );
               json_builder_begin_array( builder );
               //lc->type = LAYOUT_TOPBOTTOM;
               break;
          default:
               return;
     }

	do {
          (*tmux_layout)++;
		tmux_layout_to_i3_layout_impl( tmux_layout, builder );
	} while ( **tmux_layout == ',');

     switch (**tmux_layout) {
          case '}':
          case ']':
               json_builder_end_array( builder );
               json_builder_end_object( builder );
               return;
          default:
               return;
     }

     json_builder_end_object( builder );
     return;
}

gchar* tmux_layout_to_i3_layout( char* tmux_layout ) {
     JsonBuilder* builder = json_builder_new();
     tmux_layout_to_i3_layout_impl( &tmux_layout, builder );

     /* Generate a string from the JsonBuilder */
     JsonGenerator *gen = json_generator_new ();
     JsonNode * root = json_builder_get_root (builder);
     json_generator_set_root (gen, root);
     gchar *layout_str = json_generator_to_data (gen, NULL);

     json_node_free (root);
     g_object_unref (gen);
     g_object_unref (builder);
     /* TODO: Free the layout_str later */
     return layout_str;
}
