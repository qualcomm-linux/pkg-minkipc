TARGET = libbacktrace.a

top = ../..

INC_DIRS = -I inc \
           -I $(top)/int/inc

cflags = -g -fPIC -rdynamic

ifndef ONTARGET
cflags += -DOFFTARGET
endif

ifneq ($(STUB), false)
cflags += -DSTUB
endif

ifneq ($(TEST), false)
cflags += -DTEST
endif

SRC_DIR = ./src

INCS     = $(wildcard inc/*.h)
C_SRCS   = $(wildcard src/*.c)
CPP_SRCS = $(wildcard src/*.cpp)
OBJS     = $(C_SRCS:c=o)
OBJS    += $(CPP_SRCS:cpp=o)

# # Uncomment the following lines to disable demnagle support in Backtrace
# CPP_SRCS := $(filter-out src/backtracedemangle.cpp, $(CPP_SRCS))
# cflags += -DBACKTRACE_NO_DEMANGLE_SUPPORT

PHONY := $(TARGET)

ME = $(shell basename $(CURDIR))

all $(ME) : $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c $(INCS)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(INC_DIRS) $<

%.o: %.cpp $(INCS)
	$(CXX) -o $@ -c -std=c++11 $(CXXFLAGS) $(cflags) $(INC_DIRS) $<

PHONY += clean
clean:
	rm -rf $(TARGET) $(OBJS)

install:

.PHONY = $(PHONY)
