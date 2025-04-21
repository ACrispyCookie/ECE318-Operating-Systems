#######################################
# 
# Operating Systems Lab
# 
# Project 02 Makefile Template
#
#######################################

# The C compiler
CC = gcc

# Put any flags for the C compiler here
CFLAGS = -Wall -g $(GOODNESS_FLAG) -DTIMESLICE=$(TIMESLICE)

# Default goodness flag
GOODNESS_FLAG = -DENABLE_GOODNESS_ALGORITHM=1

# Default value for timeslice (in jiffies)
TIMESLICE ?= 10

# The source files for the Virtual Machine
LIB_SRCS = cpuinit.c cpu.c

# The corresponding object files
LIB_OBJS = cpuinit.o cpu.o

# The scheduler's source files
SCHED_SRCS = schedule.c

# Put the name of any additional source file here
ADD_SRCS = 

# Include files
INC_FILES = privatestructs.h macros.h schedule.h

# The name of the virtual machine's library, without the lib prefix and the .a suffix
LIB_NAME = sched_vm

# The directory where the virtual machine's library resides.
LIB_DIR = .

# The name of the final executable
EXEC_NAME = sjf_sched


all: $(EXEC_NAME)

$(EXEC_NAME): $(SCHED_SRCS) $(INC_FILES) $(LIB_NAME)
	$(CC) $(CFLAGS) $(SCHED_SRCS) $(ADD_SRCS) -o $(EXEC_NAME) -lm -l$(LIB_NAME) -L$(LIB_DIR)

$(LIB_NAME): $(LIB_SRCS) $(INC_FILES)
	$(CC) -c $(CFLAGS) $(LIB_SRCS)
	ar rcsv lib$(LIB_NAME).a $(LIB_OBJS)

no-goodness:
	$(MAKE) GOODNESS_FLAG=-DENABLE_GOODNESS_ALGORITHM=0

.PHONY: clean no-goodness

clean:
	rm -v $(EXEC_NAME) lib$(LIB_NAME).a *.o

