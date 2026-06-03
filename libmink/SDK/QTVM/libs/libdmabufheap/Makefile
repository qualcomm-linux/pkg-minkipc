cflags = -fPIC

ifndef ONTARGET
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

ifneq ($(STUB), false)
cflags += -DSTUB
endif

cflags += -W                              \
          -Wall                           \
          -Wextra                         \
          -Wunused                        \
          -Wno-missing-field-initializers \
          -Wno-error=pointer-to-int-cast

top      = ../..

includes =  -I ./include                     \
            -I ${top}/libs/libvmmem/include  \
            -I ${top}/int/inc    \
            -I ${top}/libs/libcommon/inc

TARGET = libdmabufheap.a

DEPS = $(wildcard ./include/*.h)
SRCS = $(wildcard ./*.c)
OBJS = $(SRCS:c=o)

ME = $(shell basename $(CURDIR))

all $(ME) : $(TARGET)

$(TARGET): $(OBJS)
	$(AR) crs $@ $^

%.o: %.c $(DEPS)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(includes) $(sanc) $<

clean:
	rm -f $(TARGET) $(OBJS)

install:
