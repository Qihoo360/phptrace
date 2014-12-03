CFLAGS=-g -Wall
all:tracedump ctrl
tracedump:tracedump.c ../util/phptrace_mmap.c  ../util/phptrace_protocol.c  ../util/phptrace_string.c
ctrl:ctrl.c  ../util/phptrace_mmap.c

clean:
	rm -f tracedump ctrl
