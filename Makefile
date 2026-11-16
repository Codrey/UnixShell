CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -g

myshell: main.c
	$(CC) $(CFLAGS) -o myshell main.c

clean:
	rm -f myshell
