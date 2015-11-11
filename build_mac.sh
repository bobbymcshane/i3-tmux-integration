gcc -ggdb3 -o test parse.c test.c $(pkg-config --libs --cflags json-glib-1.0) -lutil -lpthread
