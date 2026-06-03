UNAME := $(shell uname)

cflags = -fPIC # -Wall

top      = ../..

ifneq ($(STUB), false)
cflags += -DSTUB
endif

includes =  -I inc

ifdef ONTARGET
includes += -I $(SDK_INT_DIR)/inc
includes += -I $(WORKSPACE)/security/securemsm/smcinvoke/inc
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
includes += -I $(top)/../../SDK/QTVM/int/inc
endif

TARGET = libtunnelinvoke.a

SRCS = $(wildcard ./src/*.c)
OBJS = $(SRCS:c=o)
CPP_SRCS = $(wildcard src/*.cpp)
CPP_OBJS = $(CPP_SRCS:cpp=o)

ME = $(shell basename $(CURDIR))

all $(ME) : $(TARGET)

$(TARGET): $(OBJS) $(CPP_OBJS)
	@echo UNAME: $(UNAME)
	$(AR) crs $@ $^

%.o: %.c
	$(CC) -c -o $@ $(includes) $(CFLAGS) $(cflags) -g $(sanc) $<

%.o: %.cpp
	$(CXX) -c -o $@ $^ $(CXXFLAGS) $(cflags) -std=c++11 -g $(includes) $(sanc)

clean: clean_objs
	rm -f $(TARGET)

clean_objs:
	rm -f $(OBJS) $(CPP_OBJS)

install:
