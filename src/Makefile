CC = gcc
CFLAGS = -I../mir/ -I../mir/c2mir -L../mir/
LIBS = -lmir

all: main.o chunk.o debug.o compiler.o memory.o object.o scanner.o table.o value.o vm.o jit.o
	$(CC) ${CFLAGS} main.o chunk.o debug.o compiler.o memory.o object.o scanner.o table.o 	\
	value.o vm.o jit.o -o lox $(LIBS)

main.o: common.h main.c chunk.h vm.h
	$(CC) ${CFLAGS} -c main.c -o main.o 

chunk.o: common.h chunk.c chunk.h memory.h vm.h
	$(CC) ${CFLAGS} -c chunk.c -o chunk.o

debug.o: common.h debug.c debug.h value.h object.h
	$(CC) ${CFLAGS} -c debug.c -o debug.o

compiler.o: common.h compiler.h compiler.c scanner.h memory.h object.h
	$(CC) ${CFLAGS} -c compiler.c -o compiler.o

memory.o: common.h memory.c memory.h debug.h vm.h
	$(CC) ${CFLAGS} -c memory.c -o memory.o

object.o: common.h object.c object.h memory.h value.h vm.h table.h
	$(CC) ${CFLAGS} -c object.c -o object.o

scanner.o: common.h scanner.c scanner.h 
	$(CC) ${CFLAGS} -c scanner.c -o scanner.o

table.o: common.h table.c table.h memory.h object.h value.h 
	$(CC) ${CFLAGS} -c table.c -o table.o

value.o: common.h value.c value.h memory.h object.h
	$(CC) ${CFLAGS} -c value.c -o value.o

vm.o: common.h vm.c vm.h compiler.h debug.h memory.h object.h jit.h
	$(CC) ${CFLAGS} -c vm.c -o vm.o

jit.o: common.h
	$(CC) ${CFLAGS} -c jit.c -o jit.o

clean:
	rm -rf *.o
	rm lox