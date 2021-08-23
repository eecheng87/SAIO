CC = gcc
OBJS = test

all: test
test: test.c
	$(CC) -o $@ $<

%.o: %.c
	$(CC) -c -o $@ $<

.PHONY: clean test
clean:
	rm -f *.o $(OBJS)