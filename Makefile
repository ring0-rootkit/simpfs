main: main.c main_helper.c
	gcc -Wall main.c -g main_helper.c `pkg-config fuse3 --cflags --libs` -o main

dump: dump.c
	gcc -Wall dump.c main_helper.c `pkg-config fuse3 --cflags --libs` -o dump

hello: hello.c
	gcc -Wall -Wextra hello.c `pkg-config fuse3 --cflags --libs` -o hello

val: main
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./main fs
