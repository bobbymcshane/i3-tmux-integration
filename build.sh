gcc -DUSE_I3 -ggdb3 -o test parse.c test.c $(pkg-config --libs --cflags i3ipc-glib-1.0 json-glib-1.0) -lutil -lpthread
