CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -g
LDFLAGS = -lreadline

myshell: main.c
	$(CC) $(CFLAGS) -o myshell main.c $(LDFLAGS)

clean:
	rm -f myshell
