LIBS    = -lpthread
CFLAGS	= -g -Wall -O0 -fno-strict-aliasing
CC      = gcc

TARGET  = phptrace
PWD := $(shell pwd)
INCLUDE_FLAG = -I$(PWD)/../common -I$(PWD)/../common/sds
DEPS_DIR   = $(PWD)/../common $(PWD)/../common/sds
DEPS_FILES = $(foreach d, $(DEPS_DIR), $(wildcard $(d)/*.c))
TRACE_OBJS = $(patsubst %.c, %.o, $(DEPS_FILES))
TRACE_OBJS += trace_util.o trace_status.o trace_count.o log.o sys_trace.o trace.o

all: $(TARGET)

.SUFFIXES: .o .c .h

$(TARGET): $(TRACE_OBJS)
	$(CC) $(CFLAGS) $(INCLUDE_FLAG) -o $@ $^ $(LIBS) 

$(TRACE_OBJS):%.o:%.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAG) -c $< -o $@ 

clean:
	rm -rf $(TARGET)
	rm -rf $(TRACE_OBJS)
	rm -rf *.o *.so *.a *~

.PHONY: all clean
