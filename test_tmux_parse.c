#include <stdio.h>
#include <stdbool.h>
typedef unsigned int uint_t;
#define TMUX_CONTROL_CMD_RX_LIST_PANES "%[^:]: [%ux%u] [history %*u/%*u, %*u bytes] %%%u"
int main () {
     char name[BUFSIZ];
     uint_t width, height, pane_ix;
     char buf[BUFSIZ];
     bool begin_cmd_rx = false;
     while ( 1 ) {
          fgets( buf, BUFSIZ, stdin); 
          if ( begin_cmd_rx ) {
               printf ( "PARSING COMMAND\n");
               int res = sscanf( buf, TMUX_CONTROL_CMD_RX_LIST_PANES, name, &width, &height, &pane_ix );
               if (res == 4) {
                    printf( "name: %s, w: %u, h: %u, pane_ix: %u\n", name, width, height, pane_ix );
                    continue;
               }
               else if ( !strncmp( buf, "%end", 4 ) ) {
                    printf ( "ENDING COMMAND\n");
                    begin_cmd_rx = false;
               }
               printf ("RESULT: %d, %s\n", res, name);
          }
          if ( !strncmp( buf, "%begin", 6 ) ) {
               printf ( "BEGINNING COMMAND\n");
               begin_cmd_rx = true;
          }
     }
     return 0;
}
