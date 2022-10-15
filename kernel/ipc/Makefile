
all: pipe.o shm.o

CFLAGS += -I .

pipe.o: pipe.c
	gcc $(CFLAGS) -c pipe.c -o pipe.o

shm.o: shm.c
	gcc $(CFLAGS) -c shm.c -o shm.o

clean:
	echo "Done."