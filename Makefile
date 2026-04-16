CC     = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

.PHONY: all clean

all: P2

P2: P2.o parser.o tree.o lexer.o
	$(CC) $(CFLAGS) -o P2 P2.o parser.o tree.o lexer.o

P2.o: P2.c parser.h lexer.h token.h
	$(CC) $(CFLAGS) -c P2.c

parser.o: parser.c parser.h tree.h lexer.h token.h
	$(CC) $(CFLAGS) -c parser.c

tree.o: tree.c tree.h token.h
	$(CC) $(CFLAGS) -c tree.c

lexer.o: lexer.c lexer.h token.h
	$(CC) $(CFLAGS) -c lexer.c

clean:
	rm -f *.o P2
