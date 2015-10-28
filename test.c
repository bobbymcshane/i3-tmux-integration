#include <stdio.h>
#include <glib/gprintf.h>
#include <json-glib/json-glib.h>
#include <i3ipc-glib/i3ipc-glib.h>

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

     if ( scanf( "%ux%u,%u,%u,%u", &sx, &sy, &xoff, &yoff, &wp_id) != 5 )
          return;
     /* No need to validate the layout format string is correct for now */
     /* TODO: Build a json representation of the equivalent i3 layout */
     char bufchar = fgetc(stdin);
     switch (bufchar) {
          case '\n':
               json_builder_end_object( builder );
          case ',':
          case '}':
          case ']':
               return;
          case '{':
               json_builder_set_member_name( builder, "layout" );
               json_builder_add_string_value( builder, "splitv" );
               json_builder_set_member_name( builder, "nodes" );
               json_builder_begin_array( builder );
               //lc->type = LAYOUT_LEFTRIGHT;
               break;
          case '[':
               json_builder_set_member_name( builder, "layout" );
               json_builder_add_string_value( builder, "splith" );
               json_builder_set_member_name( builder, "nodes" );
               json_builder_begin_array( builder );
               //lc->type = LAYOUT_TOPBOTTOM;
               break;
          default:
               return;
     }

	do {
		i3_layout_construct(builder);
	} while (fgetc(stdin) == ',');

     switch (fgetc(stdin)) {
          case '}':
          case ']':
               json_builder_end_array( builder );
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
                    g_printf( " Layout is:\n%s\n", layout_str );

                    //sprintf( i3_cmd, I3_WORKSPACE_ADD_CMD, workspace );
                    //reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    //g_printf("Reply: %s\n", reply);
                    //g_free(reply);
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

