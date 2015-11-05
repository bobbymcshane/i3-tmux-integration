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

typedef unsigned int uint_t;
#define I3_WORKSPACE_ADD_CMD "workspace %d, exec gnome-terminal"

gint main() {
     i3ipcConnection *conn;
     gchar *reply = NULL;

     conn = i3ipc_connection_new(NULL, NULL);
     char buf[BUFSIZ];
     char output_buf[BUFSIZ];
     char input_buf[BUFSIZ];
     char tmux_cmd[BUFSIZ];
     char i3_cmd[BUFSIZ];
     char bufchar;
     char endc;
     int workspace, n_scanned, pane;

     /* TRY OPENING A SINGLE TTY AND PUMPING THE OUTPUT TO IT */
#if 1
     struct termios pane_io_settings;
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
     pid_t i;
     char buf2[10];
     int fds, fdm, status;
     

     /* Open a new unused tty */
     openpty( &fdm, &fds, NULL, &pane_io_settings, NULL );

     // Save the existing flags
     int saved_flags = fcntl(fdm, F_GETFL);
     // Set the new flags with O_NONBLOCK
     fcntl(fdm, F_SETFL, saved_flags | O_NONBLOCK );

     //grantpt(fdm);
     //unlockpt(fdm);

     //printf("%s\n", ptsname(fdm));

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
     /* END PRINT TO OTHER TERMINAL TEST */
#endif
     while ( 1 ) {
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
                         fgetc(stdin); /* READ A SINGLE SPACE FROM AFTER THE PANE */
                         fgets( buf, BUFSIZ, stdin); 
                         //printf( "%s", buf);
                         buf[(strlen(buf)-1)] = '\0';
                         int out_len = sprintf( output_buf, "%s", unescape(buf));
                         //fflush(stdout);
                         write( fdm, output_buf, out_len );
                         fsync( fdm );
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

          /* Check if the slave tty received any input */
          char in_buffer[BUFSIZ];
          ssize_t size = read(fdm, &in_buffer, sizeof(in_buffer));
          if ( size > 0 ) {
               /* WE HAVE DATA */
               in_buffer[size]='\0';
               printf("send-keys %s\n", in_buffer);
               fflush(stdout);
          }
#if 0
          fd_set rfds;
          struct timeval tv;
          int retval;

          /* Watch stdin on the slave (fdm) to see when it has input. */
          FD_ZERO(&rfds);
          FD_SET(fdm, &rfds);

          /* Wait up to five seconds. */
          tv.tv_sec = 5;
          tv.tv_usec = 0;

          retval = select(1, &rfds, NULL, NULL, &tv);
          /* Don't rely on the value of tv now! */

          if (retval == -1)
               perror("select()");
          else if (retval)
               printf("Data is available now.\n");
          /* FD_ISSET(0, &rfds) will be true. */
          else
               printf("No data within five seconds.\n");
#endif

     }

     g_object_unref(conn);

     return 0;
}

