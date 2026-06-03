VNDR_UNIXSOCK = true
VNDR_SMCINVOKE_QRTR = true
VNDR_VSOCK = true
VNDR_QMSGQ = true
REMOTE_SHAREMM = true
CLOSE_NOTIFIER = true
PLATFORM_CUSTOMIZED =

ifdef ONTARGET
TOP = $(WORKSPACE)
LINKAGE = -lvmmem -ldmabufheap -lqrtr -lpthread
else
TOP = ../../../../../../..
MINKSOCK_CFLAGS = -fsanitize=address -static-libasan -DOFFTARGET -DSTUB
VNDR_SMCINVOKE_QRTR = false
VNDR_VSOCK = false
VNDR_QMSGQ = false
MINKSOCK_SRCS = sharemm/src/stub.c
MINKSOCK_SRCS += sharemm/src/MinkMem.c
DESTDIR = out
endif

SHIP_PATH = $(TOP)/security/securemsm
LIB_VMMEM_PATH = $(TOP)/vendor/qcom/proprietary/memory-ship/libvmmem

MINKSOCK_CFLAGS += -fPIC -Wall -Werror \
                   -Wno-unused-parameter -Wno-unused-function \
                   -Wno-missing-field-initializers

INCLUDES = -I ./kernel       \
           -I ./include      \
           -I ./sock         \
           -I ./sharemm/inc  \
           -I $(SHIP_PATH)/mink/inc/interface \
           -I $(SHIP_PATH)/mink/inc/qtvm      \
           -I $(LIB_VMMEM_PATH)/include

MINKSOCK_SRCS += sock/minkipc.c              \
                 sock/minksocket.c           \
                 sock/msforwarder.c          \
                 sock/Marshalling.c          \
                 sock/ObjectTable.c          \
                 sock/ThreadPool.c           \
                 sock/Primordial.c           \
                 sock/SockAgnostic.c         \
                 sock/Utils.c                \
                 sock/Profiling.c            \
                 sock/LinkCredentials.c      \
                 sock/CredentialsAdapter.c   \
                 sock/xtzdCredentials.c      \
                 sharemm/src/MSMem.c         \
                 $(wildcard ./ext/*.c)

ifeq ($(VNDR_UNIXSOCK), true)
MINKSOCK_CFLAGS += -DVNDR_UNIXSOCK
MINKSOCK_SRCS += sock/SockUNIX.c
endif

ifeq ($(VNDR_SMCINVOKE_QRTR), true)
MINKSOCK_CFLAGS += -DVNDR_SMCINVOKE_QRTR
MINKSOCK_SRCS += sock/SockQRTR.c
endif

ifeq ($(VNDR_VSOCK), true)
MINKSOCK_CFLAGS += -DVNDR_VSOCK
MINKSOCK_SRCS += sock/SockVSOCK.c
endif

ifeq ($(VNDR_QMSGQ), true)
MINKSOCK_CFLAGS += -DVNDR_QMSGQ
MINKSOCK_SRCS += sock/SockQMSGQ.c
endif

ifeq ($(CLOSE_NOTIFIER), true)
MINKSOCK_CFLAGS += -DCLOSE_NOTIFIER
MINKSOCK_SRCS += sock/CloseNotifier.c
endif

ifeq ($(REMOTE_SHAREMM), true)
MINKSOCK_CFLAGS += -DREMOTE_SHAREMM
MINKSOCK_SRCS += sharemm/src/CConfinement.c \
        sharemm/src/WrappedMemparcel.c      \
        sharemm/src/ShareMemory.c           \
        sharemm/src/RemoteShareMemory.c
endif

ifeq ($(PLATFORM_CUSTOMIZED), true)
MINKSOCK_CFLAGS += -D_PLATFORM_CUSTOMIZED
MINKSOCK_SRCS += vmosal/VmOsalCustomize.c
else
MINKSOCK_SRCS += vmosal/VmOsalUnix.c
endif

ifeq ($(OEM_VM), true)
MINKSOCK_CFLAGS += -DENABLE_TUI_OEM_VM
endif

ifeq ($(DEBUG), true)
MINKSOCK_CFLAGS += -g
endif

MINKSOCK_OBJS = $(MINKSOCK_SRCS:c=o)
MINKSOCK_OBJS_NOSTUB = $(filter-out sharemm/src/stub.o, $(MINKSOCK_OBJS))

all: libminksocket_vendor.so libminksocket_vendor.a

libminksocket_vendor.so: $(MINKSOCK_OBJS)
	@echo "compiling minksocket dynamic library"
	$(CC) -shared -o $@ $^ $(CFLAGS) $(cflags) $(LDFLAGS)

libminksocket_vendor.a: $(MINKSOCK_OBJS_NOSTUB)
	@echo "compiling minksocket static library"
	$(AR) crs $@ $^

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $(MINKSOCK_CFLAGS) $(INCLUDES) $<

clean: clean_objs
	rm -f libminksocket_vendor.so libminksocket_vendor.a
	rm -rf $(DESTDIR)

clean_objs:
	rm -f $(MINKSOCK_OBJS)

install:
ifndef ONTARGET
	mkdir -p $(DESTDIR)$(prefix)/lib/
	mkdir -p $(DESTDIR)$(prefix)/include/minkipc/
endif
	install -D -m 755 libminksocket_vendor.so $(DESTDIR)$(prefix)/lib/libminksocket_vendor.so
	install -D -m 755 libminksocket_vendor.a $(DESTDIR)$(prefix)/lib/libminksocket_vendor.a
	# TO-DO : Automate this
	install -D -m 755 sock/VmOsal.h $(DESTDIR)$(prefix)/include/minkipc/VmOsal.h
	install -D -m 755 sock/minkipc.h $(DESTDIR)$(prefix)/include/minkipc/minkipc.h
	install -D -m 755 sock/LinkCredentials.h $(DESTDIR)$(prefix)/include/minkipc/LinkCredentials.h
	install -D -m 755 sock/xtzdCredentials.h $(DESTDIR)$(prefix)/include/minkipc/xtzdCredentials.h
	install -D -m 755 sharemm/inc/MSMem.h $(DESTDIR)$(prefix)/include/minkipc/MSMem.h
	install -D -m 755 sharemm/inc/RemoteShareMemory.h $(DESTDIR)$(prefix)/include/minkipc/RemoteShareMemory.h
	install -D -m 755 kernel/qlist.h $(DESTDIR)$(prefix)/include/minkipc/qlist.h
	install -D -m 755 kernel/kdefs.h $(DESTDIR)$(prefix)/include/minkipc/kdefs.h
	install -D -m 755 include/cdefs.h $(DESTDIR)$(prefix)/include/minkipc/cdefs.h
	install -D -m 755 include/check.h $(DESTDIR)$(prefix)/include/minkipc/check.h
	install -D -m 755 include/Types.h $(DESTDIR)$(prefix)/include/minkipc/Types.h
	install -D -m 755 include/vmuuid.h $(DESTDIR)$(prefix)/include/minkipc/vmuuid.h
	install -D -m 755 include/memscpy.h $(DESTDIR)$(prefix)/include/minkipc/memscpy.h
	install -D -m 755 include/logging.h $(DESTDIR)$(prefix)/include/minkipc/logging.h
	install -D -m 755 sock/minksocket.h $(DESTDIR)$(prefix)/include/minkipc/minksocket.h
	install -D -m 755 sock/msforwarder.h $(DESTDIR)$(prefix)/include/minkipc/msforwarder.h
	install -D -m 755 sock/SockQRTR.h $(DESTDIR)$(prefix)/include/minkipc/SockQRTR.h
	install -D -m 755 sock/SockUNIX.h $(DESTDIR)$(prefix)/include/minkipc/SockUNIX.h
	install -D -m 755 sock/SockVSOCK.h $(DESTDIR)$(prefix)/include/minkipc/SockVSOCK.h
	install -D -m 755 sock/SockQMSGQ.h $(DESTDIR)$(prefix)/include/minkipc/SockQMSGQ.h
	install -D -m 755 sock/CloseNotifier.h $(DESTDIR)$(prefix)/include/minkipc/CloseNotifier.h
	install -D -m 755 sock/Utils.h $(DESTDIR)$(prefix)/include/minkipc/Utils.h
	install -D -m 755 sock/Profiling.h $(DESTDIR)$(prefix)/include/minkipc/Profiling.h
	install -D -m 755 sharemm/inc/MinkMem.h $(DESTDIR)$(prefix)/include/minkipc/MinkMem.h
