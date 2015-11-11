/* TODO: I think I should change my approach.
 * When a new window is created, react by sending a command to list the panes
 * for that window and then make my new pane based on the result of that
 * command.
 *
 * I need to start keeping track of which session I am in.
 */

#define _XOPEN_SOURCE
#ifndef __APPLE__
#include <i3ipc-glib/i3ipc-glib.h>
#endif
#include <stdio.h>
#include <string.h>
#include <glib/gprintf.h>
#include <json-glib/json-glib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "tmux_commands.h"
#include "parse.h"
#include <string.h>
#include <strings.h>
#ifndef __APPLE__
#include <pty.h>
#else
#include <util.h>
#endif
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <pthread.h>

typedef unsigned int uint_t;
#define I3_WORKSPACE_ADD_CMD "workspace tmux %d"

#define TMUX_CONTROL_CMD_RX_BEGIN "%%begin %u %u %u"
#define TMUX_CONTROL_CMD_RX_END "%%end %u %u %u"

/* LIST_SESSIONS_RX:
<session_name>: <#> windows (created <date/time>) [WxH] (attached)
0: 1 windows (created Fri Nov  6 16:42:43 2015) [80x24] (attached) */
/* TODO: Can I use wildcards with scanf? */
#define TMUX_CONTROL_CMD_RX_LIST_SESSIONS "%[^:]: %u windows (created %*[^)]) [%ux%u] (attached)"

/* LIST_WINDOWS_RX:
<session_name>: <window_name> (<#> panes) [WxH] [layout <layout_str>] @<window_id> (active)
0: zsh* (1 panes) [80x24] [layout b260,80x24,0,0,3] @3 (active) */
/* TODO: Does that grab the layout string correctly if the string contains a right bracket */
#define TMUX_CONTROL_CMD_RX_LIST_WINDOWS "%[^:]: %*[^(] (%u panes) [%ux%u] [layout %[^]] @%u (active)"

/* LIST_PANES_RX:
<window_name>: [WxH] [history unsure/unsure, unsure bytes] %<pane_ix> (active)
0: [80x24] [history 0/2000, 0 bytes] %3 (active) */
/* TODO: later decide if certain inputs are useful */
#define TMUX_CONTROL_CMD_RX_LIST_PANES "%[^:]: [%ux%u] [history %*u/%*u, %*u bytes] %%%u"

/* ALL THESE GLOBALS WILL BE FIXED UP */
#ifndef __APPLE__
i3ipcConnection *conn;
#endif
gchar *reply = NULL;
struct termios pane_io_settings;
volatile int n_tmux_panes = 0;
typedef struct {
     int fd;
     int pane_number;
} TmuxPaneInfo_t;
TmuxPaneInfo_t pane_infos[ 64 ]; /* Cap at 64 for now */
TmuxPaneInfo_t* pane_info_ptrs[ 64 ]; /* Cap at 64 for now */

void spawn_tmux_pane( TmuxPaneInfo_t** pane_info_ptr, int tmux_pane_number ) {
     pid_t i;
     int fds, status;
     char buf2[10];

     struct winsize test_size = { 0, 0, 1024, 768 };
     /* Open a new unused tty */
     openpty( &pane_infos[n_tmux_panes].fd, &fds, NULL, &pane_io_settings, &test_size );

     // Save the existing flags
     int saved_flags = fcntl(pane_infos[n_tmux_panes].fd, F_GETFL);
     // Set the new flags with O_NONBLOCK

     fcntl(pane_infos[n_tmux_panes].fd, F_SETFL, saved_flags | O_NONBLOCK );

     *pane_info_ptr = &pane_infos[n_tmux_panes];
     pane_infos[n_tmux_panes].pane_number = tmux_pane_number;
     i = fork();
     if ( i == 0 ) { // child
          /* Spawn a urxvt terminal which looks at the specified pty */
          sprintf(buf2, "/usr/bin/urxvt -title pane%d -pty-fd %d", tmux_pane_number, fds);
          system(buf2);
          exit(0);
     } else {  // parent
          //dprintf(fdm,"Where do I pop up?\n");
          //printf("Where do I pop up - 2?\n");
          //waitpid(i, &status, 0); /* TODO: Do I need to do something with waitpid?
     }

     n_tmux_panes++;
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
#ifndef __APPLE__
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         //g_printf("Reply: %s\n", reply);
                         g_free(reply);
#endif
                    }
               }
               else if ( !strcmp( tmux_cmd, "output" ) ) {
                    if (scanf( "%%%d", &pane ) == 1) {
                         if ( !pane_info_ptrs[ pane ] ) {
                              /* I probably don't want to spawn my panes here... */
                              spawn_tmux_pane( &pane_info_ptrs[ pane ], pane );
                              /* Select the newly spawned urxvt window by its title */
#ifndef __APPLE__
                              sprintf( i3_cmd, "[title=\"^pane%d$\"] move window to mark pane_container%1$d", pane );
                              reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                              g_free(reply);
#endif
                         }
                         fgetc(stdin); /* READ A SINGLE SPACE FROM AFTER THE PANE */
                         fgets( buf, BUFSIZ, stdin); 
                         //printf( "%s", buf);
                         buf[(strlen(buf)-1)] = '\0';
                         int out_len = sprintf( output_buf, "%s", unescape(buf));
                         //fflush(stdout);
                         write( pane_info_ptrs[ pane ]->fd, output_buf, out_len );
                         fsync( pane_info_ptrs[ pane ]->fd );
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

                         /* Maybe this method should also build a command string to launch panes/move panes to marks */
                         gchar *layout_str = tmux_layout_to_i3_layout( parse_str );

                         char tmpfile[] = "/tmp/layout_XXXXXX";
                         int layout_fd = mkstemp( tmpfile );
                         fchmod( layout_fd, 0666 );
                         dprintf( layout_fd, "%s", layout_str );
                         close( layout_fd );
                         g_free( layout_str );
                         sprintf( i3_cmd, "workspace %s, append_layout %s, rename workspace to \"tmux %d\"", "tmp_workspace", tmpfile, workspace );
#ifndef __APPLE__
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         g_free(reply);
#endif
                         /* Strategy move window to mark then kill the marked pane */
                         //remove ( tmpfile );
                         //sprintf( i3_cmd, "exec gnome-terminal" );
#if 0
#ifndef __APPLE__
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         g_printf("Reply: %s\n", reply);
                         g_free(reply);
#endif
#endif
                    }
               }
          }
          else {
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
     bzero ( pane_info_ptrs, sizeof ( pane_info_ptrs ) );
     pthread_t tmux_read_thread;

#ifndef __APPLE__
     conn = i3ipc_connection_new(NULL, NULL);
#endif

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
               ssize_t size = read(pane_infos[pane_ix].fd, &in_buffer, sizeof(in_buffer));
               if ( size > 0 ) {
                    /* WE HAVE DATA */
                    in_buffer[size]='\0';
                    /* TODO: I might want to listen for control keys here or something... */
                    printf("send-keys -t %d \"%s\"\n", pane_infos[pane_ix].pane_number, in_buffer);
                    fflush(stdout);
               }
          }
     }

     pthread_cancel( tmux_read_thread );

#ifndef __APPLE__
     g_object_unref(conn);
#endif

     return 0;
}

