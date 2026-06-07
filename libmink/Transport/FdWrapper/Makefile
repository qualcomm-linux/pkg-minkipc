REMOTE_SHAREMM = true

ifdef ONTARGET
TOP = $(WORKSPACE)

else
TOP = ../../../../../../..
FDWRAPPER_CFLAGS = -fsanitize=address -static-libasan -DOFFTARGET -DSTUB
DESTDIR = out
endif

TRANSPORT_PATH =$(TOP)/mink/vendor/qcom/proprietary/mink/Transport
SHIP_PATH = $(TOP)/security/securemsm

FDWRAPPER_CFLAGS += -fPIC -Wall -Werror \
                   -Wno-unused-parameter -Wno-unused-function \
                   -Wno-missing-field-initializers

INCLUDES += -I interface

FDWRAPPER_SRCS = src/fdwrapper.c

ifeq ($(DEBUG), true)
FDWRAPPER_CFLAGS += -g
endif

ifeq ($(REMOTE_SHAREMM), true)
FDWRAPPER_CFLAGS += -DREMOTE_SHAREMM
endif

FDWRAPPER_OBJS = $(FDWRAPPER_SRCS:c=o)

all: libfdwrapper_vendor.so libfdwrapper_vendor.a

libfdwrapper_vendor.so: $(FDWRAPPER_OBJS)
	@echo "compiling fdwrapper dynamic library"
	$(CC) -shared -o $@ $^ $(CFLAGS) $(cflags) $(LDFLAGS)

libfdwrapper_vendor.a: $(FDWRAPPER_OBJS)
	@echo "compiling fdwrapper static library"
	$(AR) crs $@ $^

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $(FDWRAPPER_CFLAGS) $(INCLUDES) $(INC) $<

clean: clean_objs
	rm -f libfdwrapper_vendor.so libfdwrapper_vendor.a
	rm -rf $(DESTDIR)

clean_objs:
	rm -f $(FDWRAPPER_OBJS)

install:
ifndef ONTARGET
	mkdir -p $(DESTDIR)$(prefix)/lib/
endif
	install -D -m 755 libfdwrapper_vendor.a $(DESTDIR)$(prefix)/lib/libfdwrapper_vendor.a
	install -D -m 755 libfdwrapper_vendor.so $(DESTDIR)$(prefix)/lib/libfdwrapper_vendor.so
	install -D -m 755 interface/fdwrapper.h $(DESTDIR)$(prefix)/include/fdwrapper.h 
