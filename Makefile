main main.c:
	gcc -Wall main.c main_helper.c `pkg-config fuse3 --cflags --libs` -o main

hello hello.c:
	gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
