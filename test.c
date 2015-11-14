/* TODO: I think I should change my approach.
 * When a new window is created, react by sending a command to list the panes
 * for that window and then make my new pane based on the result of that
 * command.
 *
 * I need to start keeping track of which session I am in.
 */

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED
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
#include "tmux_event_lib.h"

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
#define TMUX_CONTROL_CMD_RX_LIST_PANES "%[^:]: [%ux%u] [history %*u/%*u, %*u bytes] %%%u (active)"

#define TMUX_CONTROL_CMD_TX_LIST_PANES "list-panes"
#define TMUX_CONTROL_CMD_TX_SEND_KEYS "send-keys"
#define DEBUG
#ifdef DEBUG
#define debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define debug(...) ({})
#endif

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

void send_tmux_cmd( const char* command ) {
     puts( command );
     fflush( stdout );
}

void send_keys_to_pane( const char* keys, uint_t tmux_pane_index ) {
     char* buffer;
     asprintf( &buffer, "send-keys -t %d \"%s\"", tmux_pane_index, keys );
     send_tmux_cmd( buffer );
}

void spawn_tmux_pane( TmuxPaneInfo_t** pane_info_ptr, int tmux_pane_number ) {
     pid_t i;
     int fds, status;
     char buf2[10];

     /* TODO: Get window sizing working correctly */
     struct winsize test_size = { 100, 100, 1024, 768 };
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

void handle_layout_change( unsigned int window, const char* layout, void* ctxt )
{
  /* TODO: Do I need to remove the newline at the end of the layout string? */
  /* retrieve the entire layout string */
  const char* parse_str = layout;
  /* Ignore checksum for now */
  char checksum[5];
  sscanf( parse_str, "%4s,", checksum );
  parse_str += 6;

  /* Maybe this method should also build a command string to launch panes/move panes to marks */
  gchar *layout_str = tmux_layout_to_i3_layout( layout );

  char tmpfile[] = "/tmp/layout_XXXXXX";
  int layout_fd = mkstemp( tmpfile );
  fchmod( layout_fd, 0666 );
  dprintf( layout_fd, "%s", layout_str );
  close( layout_fd );
  g_free( layout_str );
  char* i3_cmd;
  asprintf( &i3_cmd, "workspace %s, append_layout %s, rename workspace to \"tmux %d\"", "tmp_workspace", tmpfile, window );
#ifndef __APPLE__
  reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
  g_free(reply);
#endif
  free( i3_cmd );
  //remove ( tmpfile );
  /* Strategy move window to mark then kill the marked pane */
}

struct OnLayoutChange layout_change_handler = { { NULL }, handle_layout_change, NULL };

void handle_window_add( unsigned int window, void* ctxt )
{
  /* Get the layout for the new window */
  send_tmux_cmd( TMUX_CONTROL_CMD_TX_LIST_PANES );

  char* i3_cmd;
  asprintf( &i3_cmd, I3_WORKSPACE_ADD_CMD, window );
#ifdef __APPLE__
  reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
  g_free(reply);
#endif
  free( i3_cmd );
}

struct OnWindowAdd window_add_handler = { { NULL }, handle_window_add, NULL };

void handle_pane_output( unsigned int pane, const char* output, void* ctxt )
{
  if ( !pane_info_ptrs[ pane ] ) {
    /* I probably don't want to spawn my panes here... */
    spawn_tmux_pane( &pane_info_ptrs[ pane ], pane );
    /* Select the newly spawned urxvt window by its title */
    char* i3_cmd;
    asprintf( &i3_cmd, "[title=\"^pane%d$\"] move window to mark pane_container%1$d", pane );
#ifndef __APPLE__
    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
    g_free(reply);
#endif
    free( i3_cmd );
  }
  char* pane_terminal_output;
  char* output_copy = strdup( output );
  int out_len = asprintf(&pane_terminal_output, "%s", unescape(output_copy));
  /* TODO: Check return codes for write/fsync */
  write( pane_info_ptrs[ pane ]->fd, pane_terminal_output, out_len );
  fsync( pane_info_ptrs[ pane ]->fd );
  free( output_copy );
}

struct OnPaneOutput pane_output_handler = { { NULL }, handle_pane_output, NULL };

/* read args are unused for now. Later I will probably moe away from maintaining a global list of fds or something... */
void* tmux_read_init( void* tmux_read_args ) {
  tmux_event_init( );
  register_pane_output_handler( &pane_output_handler );
  register_window_add_handler( &window_add_handler );
  register_layout_change_handler( &layout_change_handler );
  tmux_event_loop( stdin );
#if 0
     char* output_buf;
     char tmux_rx_line[BUFSIZ];
     /* TODO: What is a good command size? I probably eventually want to do
      * this a different way */
     char tmux_cmd[BUFSIZ];
     char* i3_cmd;
     char bufchar;
     char endc;
     int workspace, n_scanned, pane;

     while( 1 ) {
          /* Read in the entire line from tmux */
          if ( fgets( tmux_rx_line, sizeof tmux_rx_line, stdin ) != NULL ) {
               if (sscanf(tmux_rx_line, "%%output %%%d%*c%m[^\n]", &pane, &output_buf ) == 2) {
                    if ( !pane_info_ptrs[ pane ] ) {
                         /* I probably don't want to spawn my panes here... */
                         spawn_tmux_pane( &pane_info_ptrs[ pane ], pane );
                         /* Select the newly spawned urxvt window by its title */
                         asprintf( &i3_cmd, "[title=\"^pane%d$\"] move window to mark pane_container%1$d", pane );
#ifndef __APPLE__
                         reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                         g_free(reply);
#endif
                    }
                    char* pane_terminal_output;
                    int out_len = asprintf(&pane_terminal_output, "%s", unescape(output_buf));
                    free( output_buf );
                    /* TODO: Check return codes for write/fsync */
                    write( pane_info_ptrs[ pane ]->fd, pane_terminal_output, out_len );
                    fsync( pane_info_ptrs[ pane ]->fd );
               }
               else if (sscanf( tmux_rx_line, "%%window-add @%d", &workspace ) == 1 ) {
                    /* Get the layout for the new window */
                    send_tmux_cmd( TMUX_CONTROL_CMD_TX_LIST_PANES );

                    asprintf( &i3_cmd, I3_WORKSPACE_ADD_CMD, workspace );
#ifdef __APPLE__
                    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    g_free(reply);
#endif
               }
               else if (sscanf( tmux_rx_line, "%%layout-change @%d %m[^\n]", &workspace, &output_buf ) == 2 ) {
                    /* TODO: Do I need to remove the newline at the end of the layout string? */
                    /* retrieve the entire layout string */
                    char* parse_str = output_buf;
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
                    free( output_buf );
                    asprintf( &i3_cmd, "workspace %s, append_layout %s, rename workspace to \"tmux %d\"", "tmp_workspace", tmpfile, workspace );
#ifndef __APPLE__
                    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    g_free(reply);
#endif
                    remove ( tmpfile );
                    /* Strategy move window to mark then kill the marked pane */
                    //sprintf( i3_cmd, "exec gnome-terminal" );
#if 0
                    reply = i3ipc_connection_message(conn, I3IPC_MESSAGE_TYPE_COMMAND, i3_cmd, NULL);
                    g_printf("Reply: %s\n", reply);
                    g_free(reply);
#endif
               }
               else {
                    debug( "%s", tmux_rx_line );
               }

               bufchar = fgetc( stdin );
               if ( bufchar == EOF )
                    return 0;
               else
                    ungetc( bufchar, stdin );

          }
     }
#endif
}

gint main() {
     bzero ( pane_info_ptrs, sizeof ( pane_info_ptrs ) );
     pthread_t tmux_read_thread;

#ifndef __APPLE__
     conn = i3ipc_connection_new(NULL, NULL);
#endif

     bzero( &pane_io_settings, sizeof( pane_io_settings));

     /*if (tcgetattr(STDOUT_FILENO, &pane_io_settings)){
          printf("Error getting current terminal settings, %s\n", strerror(errno));
          return -1;
     }*/

     /* We want to disable the canonical mode */
     pane_io_settings.c_lflag &= ~(ICANON | ECHO);
     pane_io_settings.c_cc[VTIME]=0;
     pane_io_settings.c_cc[VMIN]=1;

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
                    send_keys_to_pane( in_buffer, pane_infos[pane_ix].pane_number );
               }
          }
     }

     pthread_cancel( tmux_read_thread );

#ifndef __APPLE__
     g_object_unref(conn);
#endif

     return 0;
}

