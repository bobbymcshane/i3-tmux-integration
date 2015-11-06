#define _XOPEN_SOURCE
#include <stdio.h>
#include <string.h>
#include <glib/gprintf.h>
#include <json-glib/json-glib.h>
#include <i3ipc-glib/i3ipc-glib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "tmux_commands.h"
#include "parse.h"
#include <string.h>
#include <strings.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <pthread.h>

typedef unsigned int uint_t;
#define I3_WORKSPACE_ADD_CMD "workspace %d"

/* ALL THESE GLOBALS WILL BE FIXED UP */
i3ipcConnection *conn;
gchar *reply = NULL;
struct termios pane_io_settings;
volatile int n_tmux_panes = 0;
int pane_fds[ 64 ]; /* Cap at 64 for now */
int* pane_fd_ptrs[ 64 ]; /* Cap at 64 for now */

void spawn_tmux_pane( int** pane_fd_ptr ) {
     pid_t i;
     int fds, status;
     char buf2[10];

     /* Open a new unused tty */
     openpty( &pane_fds[n_tmux_panes], &fds, NULL, &pane_io_settings, NULL );

     // Save the existing flags
     int saved_flags = fcntl(pane_fds[n_tmux_panes], F_GETFL);
     // Set the new flags with O_NONBLOCK

     fcntl(pane_fds[n_tmux_panes], F_SETFL, saved_flags | O_NONBLOCK );

     *pane_fd_ptr = &pane_fds[n_tmux_panes];
     n_tmux_panes++;
     printf("INCREMENTED %d\n", n_tmux_panes);
     i = fork();
     if ( i == 0 ) { // parent
          //dprintf(fdm,"Where do I pop up?\n");
          //printf("Where do I pop up - 2?\n");
          waitpid(i, &status, 0);
     } else {  // child
          /* Spawn a urxvt terminal which looks at the specified pty */
          sprintf(buf2, "/usr/bin/urxvt -pty-fd %d", fds);
          printf("%s\n",buf2);
          system(buf2);
          exit(0);
     }
}

/* read args are unused for now. Later I will probably moe away from maintaining a global list of fds or something... */
void* tmux_read_init( void* tmux_read_args ) {
     char buf[BUFSIZ];
     char output_buf[BUFSIZ];
     char input_buf[BUFSIZ];
     char tmux_cmd[BUFSIZ];
     char i3_cmd[BUFSIZ];
     char bufchar;
     char endc;
     int workspace, n_scanned, pane;
     while( 1 ) {
          if (scanf( "%%%s ", tmux_cmd ) == 1 ) {
               /* REACHED LINE END */
               if ( !strcmp( tmux_cmd, TMUX_WINDOW_ADD ) ) {
                    if (scanf( "@%d", &workspace ) == 1 ) {
                         sprintf( i3_cmd, I3_WORKSPACE_ADD_CMD, workspace );
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         //g_printf("Reply: %s\n", reply);
                         g_free(reply);
                    }
               }
               else if ( !strcmp( tmux_cmd, "output" ) ) {
                    if (scanf( "%%%d", &pane ) == 1) {
                         if ( !pane_fd_ptrs[ pane ] ) {
                              spawn_tmux_pane( &pane_fd_ptrs[ pane ] );
                         }
                         fgetc(stdin); /* READ A SINGLE SPACE FROM AFTER THE PANE */
                         fgets( buf, BUFSIZ, stdin); 
                         //printf( "%s", buf);
                         buf[(strlen(buf)-1)] = '\0';
                         int out_len = sprintf( output_buf, "%s", unescape(buf));
                         //fflush(stdout);
                         write( *pane_fd_ptrs[ pane ], output_buf, out_len );
                         fsync( *pane_fd_ptrs[ pane ] );
                    }
               }
               else if ( !strcmp( tmux_cmd, TMUX_LAYOUT_CHANGE ) ) {
                    if (scanf( "@%d", &workspace ) == 1 ) {
                         /* TODO: Do I need to remove the newline at the end of the layout string? */
                         /* retrieve the entire layout string */
                         fgets( buf, BUFSIZ, stdin);
                         char* parse_str = buf;
                         /* Ignore checksum for now */
                         char checksum[5];
                         sscanf( parse_str, "%4s,", checksum );
                         parse_str += 6;
                         
                         gchar *layout_str = tmux_layout_to_i3_layout( parse_str );

                         char tmpfile[] = "/tmp/layout_XXXXXX";
                         int layout_fd = mkstemp( tmpfile );
                         fchmod( layout_fd, 0666 );
                         dprintf( layout_fd, "%s", layout_str );
                         close( layout_fd );
                         g_free( layout_str );
                         sprintf( i3_cmd, "workspace %d, append_layout %s", workspace, tmpfile );
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         //g_printf("Reply: %s\n", reply);
                         g_free(reply);
                         /* Strategy move window to mark then kill the marked pane */
                         //remove ( tmpfile );
                         //sprintf( i3_cmd, "exec gnome-terminal" );
#if 0
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         g_printf("Reply: %s\n", reply);
                         g_free(reply);
#endif
                    }
               }
          }
          else {
next_cmd:
               /* Seek to end of line */
               fgets( buf, BUFSIZ, stdin); 
               //printf("%s", buf);
          }
          bufchar = fgetc( stdin );
          if ( bufchar == EOF )
               return 0;
          else
               ungetc( bufchar, stdin );

     }
}

gint main() {
     bzero ( pane_fd_ptrs, sizeof ( pane_fd_ptrs ) );
     pthread_t tmux_read_thread;

     conn = i3ipc_connection_new(NULL, NULL);

     /* TRY OPENING A SINGLE TTY AND PUMPING THE OUTPUT TO IT */
     bzero( &pane_io_settings, sizeof( pane_io_settings));

     /*if (tcgetattr(STDOUT_FILENO, &pane_io_settings)){
          printf("Error getting current terminal settings, %s\n", strerror(errno));
          return -1;
     }*/

     /* We want to disable the canonical mode */
     //pane_io_settings.c_lflag &= ~ICANON;
     //pane_io_settings.c_lflag |= ECHO; /* TODO: AM I SURE I WANT THIS? */
     pane_io_settings.c_lflag &= ~(ICANON | ECHO);
     pane_io_settings.c_cc[VTIME]=0;
     pane_io_settings.c_cc[VMIN]=1;

     //grantpt(fdm);
     //unlockpt(fdm);

     //printf("%s\n", ptsname(fdm));

     pthread_create( &tmux_read_thread, NULL, tmux_read_init, NULL );

     while ( 1 ) {
          /* Check if the slave tty received any input */
          int pane_ix;
          for ( pane_ix = 0; pane_ix < n_tmux_panes; pane_ix++ ) {
               char in_buffer[BUFSIZ];
               ssize_t size = read(pane_fds[pane_ix], &in_buffer, sizeof(in_buffer));
               if ( size > 0 ) {
                    /* WE HAVE DATA */
                    in_buffer[size]='\0';
                    printf("send-keys \"%s\"\n", in_buffer);
                    fflush(stdout);
               }
          }
     }

     pthread_cancel( tmux_read_thread );

     g_object_unref(conn);

     return 0;
}

