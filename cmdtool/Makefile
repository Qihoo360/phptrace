NAME = 
VERSION = 0.1

DIST := $(NAME)-$(VERSION)

CC = gcc
RM = rm 

### src directory
MW_DIR=$(PWD)
OBJ_PATH = objs
TARGET=trace
MAIN=
INSTALL_PATH = 
type = release


#################################
# flags, includes and libs
# ################################
# # for test
ifeq ($(type), debug)
CFLAGS = -g -D DEBUG
else
CFLAGS = -g
endif

# CFLAGS = -g -Wall -fPIC \
	# #   -shared \
	#  # -I/home/work/lib/libmcrypt/include\
	# -I/home/work/caoguoliang/lua/include\
	#   -I.
#   # for production
#CFLAGS = -Wall -O2 -fPIC -I/home/work/lib/libmcrypt/include 

INCLUDES = 

#INCLUDES = -I$(MW_DIR)/include
#-I/home/work/caoguoliang/lua/include
LIBS = 
# -Wl,--rpath=/home/work/lib/libmcrypt/lib \
	#  # -L/home/work/lib/libmcrypt/lib -lmcrypt\
	#	-L/home/work/caoguoliang/lua/lib -llua
LFLAGS=



###################################
# search all the sub directorires
# ###################################

#	SRC_DIR=$(MW_DIR)
#	
#	### header files 
#	HEADER_FILES=$(shell find $(SRC_DIR) -name '*.h')
#	HEADER_DIR=$(sort -u $(dir $(HEADER_FILES)))
#	INCLUDES+= $(foreach temp,$(HEADER_DIR), -I$(temp)) 
#	
#	### C src files C_SOURCES = $(wildcard *.c)
#	C_SRCDIR = $(SRC_DIR)
#	C_FILES = $(shell find $(SRC_DIR) -name '*.c')  ## all the c files in SRC_DIR
#	#C_FILES+=$(wildcard *.c)   ## c files in current directory
#	C_OBJS = $(patsubst %.c, %.o, $(C_FILES))
#	#C_OBJS = $(patsubst %.c, $(OBJ_PATH)/%.o, $(C_FILES))
#	
#	
#	### CPP src files  CPP_SOURCES = $(wildcard *.cpp)
#	CPP_SRCDIR = $(SRC_DIR)
#	CPP_FILES = $(shell find $(SRC_DIR) -name '*.cpp')  ## all the cpp files in SRC_DIR
#	#CPP_SOURCES = $(foreach d,$(CPP_SRCDIR),$(wildcard $(d)/*.cpp) )
#	CPP_OBJS = $(patsubst %.cpp, %.o, $(CPP_SOURCES))
#	#CPP_OBJS = $(patsubst %.cpp, $(OBJ_PATH)/%.o, $(CPP_SOURCES))

###################################
# search specific sub directorires
# ###################################

SRC_DIR=$(MW_DIR)/../util 

### header files 
HEADER_FILES=$(shell find $(SRC_DIR) -name '*.h')
HEADER_DIR=$(sort -u $(dir $(HEADER_FILES)))
INCLUDES+= $(foreach temp,$(HEADER_DIR), -I$(temp)) 

### C src files C_SOURCES = $(wildcard *.c)
C_SRCDIR = $(SRC_DIR)
C_FILES = $(foreach d,$(C_SRCDIR),$(wildcard $(d)/*.c) )
#C_FILES+=$(wildcard *.c)   ## c files in current directory
C_FILES+=phptrace.c
#C_FILES+=fpm_ptrace.c
#C_FILES+=phpstack.c
#C_FILES+=p_object.c
C_OBJS = $(patsubst %.c, %.o, $(C_FILES))
#C_OBJS = $(patsubst %.c, $(OBJ_PATH)/%.o, $(C_FILES))
STACK_OBJS = fpm_ptrace.o phpstack.o


### CPP src files  CPP_SOURCES = $(wildcard *.cpp)
#	CPP_SRCDIR = $(SRC_DIR)
#	#CPP_FILES = $(shell find $(SRC_DIR) -name '*.cpp')  ## all the cpp files in SRC_DIR
#	CPP_FILES = $(foreach d,$(CPP_SRCDIR),$(wildcard $(d)/*.c) )
#	CPP_FILES+=$(wildcard *.cpp)   ## cpp files in current directory
#	#CPP_SOURCES = $(foreach d,$(CPP_SRCDIR),$(wildcard $(d)/*.cpp) )
#	CPP_OBJS = $(patsubst %.cpp, %.o, $(CPP_SOURCES))
#	#CPP_OBJS = $(patsubst %.cpp, $(OBJ_PATH)/%.o, $(CPP_SOURCES))



default:init compile stack

$(C_OBJS):%.o:%.c
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

$(CPP_OBJS):$(OBJ_PATH)/%.o:%.cpp
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

init:
	# $(foreach d,$(SRC_DIR), mkdir -p $(OBJ_PATH)/$(d);)

gen:
	gcc -c -g ../util/phptrace_protocol.c -o ../util/phptrace_protocol.o
	gcc -c -g ../util/phptrace_mmap.c -o ../util/phptrace_mmap.o
	gcc -c -g ../util/phptrace_string.c -o ../util/phptrace_string.o
	gcc -c -g gen.c -o gen.o
	gcc ../util/phptrace_mmap.o ../util/phptrace_protocol.o ../util/phptrace_string.o gen.o -o gen  -g

stack: $(STACK_OBJS)
	gcc $(STACK_OBJS) -o stack

test:
	@echo "C_FILES: $(C_FILES)"
	@echo "C_OBJS: $(C_OBJS)"
	@echo "CPP_FILES: $(CPP_FILES)"
	@echo "CPP_OBJS: $(CPP_OBJS)"

compile:$(C_OBJS) $(CPP_OBJS)
	$(CC)  $^ -o $(TARGET)  $(LFLAGS) $(LIBS) $(CFLAGS) 

clean:
	$(RM) $(C_OBJS) $(CPP_OBJS) $(STACK_OBJS)
	$(RM) -rf $(OBJ_PATH)
	$(RM) -f $(TARGET) stack
	
	$(RM) -f gen.o gen
	$(RM) -f phpstack.o fpm_ptrace.o stack 

install: $(TARGET)
	cp $(TARGET) $(PREFIX_BIN)

uninstall:
	$(RM) -f $(PREFIX_BIN)/$(TARGET)

rebuild: clean init compile

dist:
	if [ -d $(DIST) ]; then $(RM) $(DIST); fi
	mkdir -p $(DIST)
	cp *.c Makefile $(DIST)/
	tar czvf $(DIST).tar.gz $(DIST)
	$(RM) $(DIST)

.PHONY: default clean dist
