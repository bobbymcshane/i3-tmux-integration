gcc -ggdb3 -o test tmux_event_lib.c parse.c test.c $(pkg-config --libs --cflags json-glib-1.0) $(pkg-config --libs --cflags i3ipc-glib-1.0 2>/dev/null) -lutil -lpthread
