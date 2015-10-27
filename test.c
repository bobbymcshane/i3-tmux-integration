#include <stdio.h>
#include <glib/gprintf.h>
#include <i3ipc-glib/i3ipc-glib.h>

#define WINDOW_ADD_CMD "window-add"
#define I3_WORKSPACE_ADD_CMD "workspace %d"
gint main() {
     i3ipcConnection *conn;
     gchar *reply = NULL;

     conn = i3ipc_connection_new(NULL, NULL);
     char buf[BUFSIZ];
     char i3_cmd[BUFSIZ];
     char bufchar;
     char endc;
     int workspace, n_scanned;
     while ( 1 ) {
          if (scanf( "%%%s @%d%c", buf, &workspace, &endc ) == 3 && endc == '\n') {
               /* REACHED LINE END */
               if ( !strcmp( buf, WINDOW_ADD_CMD ) ) {
                    sprintf( i3_cmd, I3_WORKSPACE_ADD_CMD, workspace );
                    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    g_printf("Reply: %s\n", reply);
                    g_free(reply);
               }
          }
          else {
               /* 2000 is not enough? */
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

