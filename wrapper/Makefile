all: so

so:
	gcc -shared -o libdummy.so -fPIC dummy.c
	gcc -I../include -shared -fPIC preload.c -o preload.so -ldl -lpthread -lrt


.PHONY: clean preload.so
clean:
	rm -f *.o preload.so libdummy.so
