#include <stdio.h>
#include <string.h>
#include <glib/gprintf.h>
#include <json-glib/json-glib.h>
#include <i3ipc-glib/i3ipc-glib.h>
#include <sys/stat.h>

typedef unsigned int uint_t;
#define WINDOW_ADD_CMD "window-add"
#define LAYOUT_CHANGE_CMD "layout-change"
#define I3_WORKSPACE_ADD_CMD "workspace %d, exec gnome-terminal"

void i3_layout_construct( JsonBuilder* builder ) {
     GError *err = NULL;
     JsonParser *parser;

     json_builder_begin_object( builder );
     json_builder_set_member_name( builder, "type" );
     json_builder_add_string_value( builder, "con" );

     /* TODO: DO we always have the id? */
     uint_t sx, sy, xoff, yoff, wp_id;

     if ( scanf( "%ux%u,%u,%u", &sx, &sy, &xoff, &yoff) != 4 )
          return;

     char saved[BUFSIZ];
     char bufchar = fgetc(stdin);
	if ( bufchar == ',') {
          uint_t saved_val;
          if ( scanf( "%u", &saved_val ) == 1 ) {
               bufchar = fgetc(stdin);
               if (bufchar == 'x') {
                    ungetc('x', stdin);
                    sprintf( saved, "%u", saved_val );
                    int i;
                    for( i = strlen(saved)-1; i >= 0; i-- ) {
                         ungetc( saved[ i ], stdin );
                    }
               }
               else {
                    ungetc( bufchar, stdin );
               }
          }
	}
     else {
          ungetc( bufchar, stdin );
     }
     /* No need to validate the layout format string is correct for now */
     /* TODO: Build a json representation of the equivalent i3 layout */
     bufchar = fgetc(stdin);
     switch (bufchar) {
          case '\n':
          case ',':
          case '}':
          case ']':
               json_builder_set_member_name( builder, "swallows" );
               json_builder_begin_array( builder );
               json_builder_begin_object( builder );
               json_builder_set_member_name( builder, "class" );
               json_builder_add_string_value( builder, ".*" );
               json_builder_end_object( builder );
               json_builder_end_array( builder );
               json_builder_end_object( builder );
               ungetc(bufchar, stdin);
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
		i3_layout_construct(builder);
          bufchar = fgetc(stdin);
	} while ( bufchar == ',');

     switch (bufchar) {
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

gint main() {
     i3ipcConnection *conn;
     gchar *reply = NULL;

     conn = i3ipc_connection_new(NULL, NULL);
     char buf[BUFSIZ];
     char tmux_cmd[BUFSIZ];
     char i3_cmd[BUFSIZ];
     char bufchar;
     char endc;
     int workspace, n_scanned;
     while ( 1 ) {
          if (scanf( "%%%s @%d", tmux_cmd, &workspace ) == 2 ) {
               /* REACHED LINE END */
               if ( !strcmp( tmux_cmd, WINDOW_ADD_CMD ) ) {
                    sprintf( i3_cmd, I3_WORKSPACE_ADD_CMD, workspace );
                    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    g_printf("Reply: %s\n", reply);
                    g_free(reply);
               }
               if ( !strcmp( tmux_cmd, LAYOUT_CHANGE_CMD ) ) {
                    /* Ignore checksum for now */
                    char checksum[5];
                    scanf( "%4s,", checksum );

                    /* TODO: MORE INTENSE LAYOUTS LATER */
                    
                    JsonBuilder* builder = json_builder_new();
                    i3_layout_construct( builder );

                    /* Generate a string from the JsonBuilder */
                    JsonGenerator *gen = json_generator_new ();
                    JsonNode * root = json_builder_get_root (builder);
                    json_generator_set_root (gen, root);
                    gchar *layout_str = json_generator_to_data (gen, NULL);

                    json_node_free (root);
                    g_object_unref (gen);
                    g_object_unref (builder);

                    char tmpfile[] = "/tmp/layout_XXXXXX";
                    int layout_fd = mkstemp( tmpfile );
                    fchmod( layout_fd, 0666 );
                    dprintf( layout_fd, "%s", layout_str );
                    close( layout_fd );
                    sprintf( i3_cmd, "workspace %d, append_layout %s", workspace, tmpfile );
                    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    g_printf("Reply: %s\n", reply);
                    g_free(reply);
                    //remove ( tmpfile );
               }
          }
          else {
next_cmd:
               /* Seek to end of line */
               fgets( buf, BUFSIZ, stdin); 
               printf("%s\n", buf);
          }
          bufchar = fgetc( stdin );
          if ( bufchar == EOF )
               return 0;
          else
               ungetc( bufchar, stdin );
     }

     g_object_unref(conn);

     return 0;
}

