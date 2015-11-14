#include "tmux_event_lib.h"
#include <stdlib.h>
/* I just want to scope out a basic event system which speaks the tmux control
 * mode text protocol. Maybe I will eventually use this abstraction for i3mux? */

/* ---- DEFINITIONS ---- */

/* stream read definitions */
char         s[ BUFSIZ ]; // stream read buffer

/* pane output event definitions */
unsigned int pane;
char*        output;
LIST_HEAD( PaneOutputEventHandlerList, OnPaneOutput ) pane_output_handlers;

/* window add event definitions */
unsigned int window;
LIST_HEAD( WindowAddEventHandlerList, OnWindowAdd ) window_add_handlers;

/* layout change event definitions */
// unsigned int window; -- defined above
char* layout;
LIST_HEAD( LayoutChangeEventHandlerList, OnLayoutChange ) layout_change_handlers;

/* ---- END DEFINITIONS ---- */

void tmux_event_init( )
{
  LIST_INIT( &pane_output_handlers );
  LIST_INIT( &window_add_handlers );
  LIST_INIT( &layout_change_handlers );
}

/* ---- EVENT REGISTRATION FUNCTIONS ---- */

void register_pane_output_handler( struct OnPaneOutput* handler )
{
  LIST_INSERT_HEAD( &pane_output_handlers, handler, entries );
}

void register_window_add_handler( struct OnWindowAdd* handler )
{
  LIST_INSERT_HEAD( &window_add_handlers, handler, entries );
}

void register_layout_change_handler( struct OnLayoutChange* handler )
{
  LIST_INSERT_HEAD( &layout_change_handlers, handler, entries );
}

/* ---- END EVENT REGISTRATION FUNCTIONS ---- */

/* NOTE: Eventually I think I would like to make a parse tree. For now, I will
 * simply use a series of if statements that are each checking for an entire
 * control command */

#define HANDLE_EVENTS( head, ... ) do {                                       \
  __typeof__( (head)->lh_first ) handler;                                     \
  LIST_FOREACH( handler, head, entries )                                      \
  {                                                                           \
    handler->handle( __VA_ARGS__, handler->ctxt );                            \
  }                                                                           \
} while(0)

void tmux_event_loop( FILE* tmux_control_stream )
{
  while( fgets( s, sizeof( s ), tmux_control_stream ) != NULL )
  {
    if( sscanf( s, "%%output %%%u%*c%m[^\n]", &pane, &output ) == 2 )
    {
      /* tmux pane output event */
      HANDLE_EVENTS( &pane_output_handlers, pane, output );
      free( output );
    }
    else if( sscanf( s, "%%window-add @%d", &window ) == 1 )
    {
      /* tmux window add event */
      HANDLE_EVENTS( &window_add_handlers, window );
    }
    else if ( sscanf( s, "%%layout-change @%d %m[^\n]", &window, &layout ) == 2 )
    {
      /* tmux layout change event */
      HANDLE_EVENTS( &layout_change_handlers, window, layout );
      free( layout );
    }
    else {
      /* unhandled event */
    }
  }
}
