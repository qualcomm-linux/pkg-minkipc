TARGET = libminkhub.a

ifndef ONTARGET
WORKSPACE = ../../../../../../..
endif

minkcomponentpath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink
minkheaderpath    = $(WORKSPACE)/security/securemsm/mink
interfacepath     = $(minkheaderpath)/inc/interface

INC_DIRS = -I inc
INC_DIRS += -I interface \
            -I $(interfacepath)

cflags += -fPIC

ifdef ONTARGET
INC_DIRS += -I $(WORKSPACE)/security/securemsm/smcinvoke/inc
else
cflags += -DOFFTARGET
INC_DIRS += -I $(minkcomponentpath)/Transport/MinkSocket/sock
INC_DIRS += -I $(minkcomponentpath)/Transport/MinkSocket/kernel
INC_DIRS += -I $(minkcomponentpath)/Transport/MinkSocket/include
INC_DIRS += -I $(minkcomponentpath)/Credentials/inc
DESTDIR = out
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG -g
endif

INCS = $(wildcard inc/*.h)
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:c=o)

PHONY := $(TARGET)

ME = $(shell basename $(CURDIR))

all $(ME) : $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c $(INCS)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(INC_DIRS) $<

PHONY += clean
clean:
	rm -rf $(TARGET) $(OBJS)
	rm -rf $(DESTDIR)

.PHONY = $(PHONY)

install:
ifndef ONTARGET
	mkdir -p $(DESTDIR)$(prefix)/include/minkhub/
	install -D -m 755 $(TARGET) $(DESTDIR)$(prefix)/lib/$(TARGET)
	install -D -m 755 interface/MinkHub.h $(DESTDIR)$(prefix)/include/minkhub/MinkHub.h
endif
