
CC=gcc
CFLAGS=-Wextra -Werror
ADEPT=adept
ADEPTFLAGS=--no-typeinfo
RM=rm -f

.PHONY: release clean

release: library_a.o library_b.o main.o
	$(CC) main.o library_b.o library_a.o -o main

library_a.o:
	$(CC) $(CFLAGS) -c library_a.c -o library_a.o

library_b.o:
	$(ADEPT) $(ADEPTFLAGS) -c library_b.adept -o library_b.o

main.o:
	$(CC) $(CFLAGS) -c main.c -o main.o

clean:
	$(RM) library_a.o
	$(RM) library_b.o
	$(RM) main.o
	$(RM) main
