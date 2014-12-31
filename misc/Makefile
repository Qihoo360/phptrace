CFLAGS=-g -Wall
all:tracedump ctrl
tracedump:tracedump.c ../common/phptrace_mmap.c  ../common/phptrace_protocol.c  ../common/sds/sds.c
ctrl:ctrl.c  ../common/phptrace_mmap.c

clean:
	rm -f tracedump ctrl
