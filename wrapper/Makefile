all: preload.so

.PHONY: clean
preload.so: dummy.c preload.c
	gcc -shared -o libdummy.so -fPIC dummy.c
	gcc -I../include -shared -fPIC -Wall preload.c -o preload.so -ldl -lpthread -lrt

clean:
	rm -f *.o preload.so libdummy.so target-preload.c
