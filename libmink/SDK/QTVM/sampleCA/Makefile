# This makefile is for OFFTARGET only!
#
# To run the PositiveSampleCode test for OFFTARGET, we need a sample client. We
# can use the same client for On and offtarget, but the client needs to stub out
# any code that might interact with QTEE when running OFFTARGET.

SDK_PATH = ../..
MINK_PATH = $(SDK_PATH)/../..
MINK_SOCKET_PATH = $(MINK_PATH)/Transport/MinkSocket
FD_WRAPPER_PATH = $(MINK_PATH)/Transport/FdWrapper
MINK_CREDENTIALS_PATH = $(MINK_PATH)/Credentials
SDK_EXT = $(SDK_PATH)/ext

sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address

cflags += -DSTUB -DDEBUG
STATIC_LIBS += $(MINK_SOCKET_PATH)/libminksocket_vendor.a
# needed by libminksocket
DYN_LIBS += -lpthread
STATIC_LIBS += $(SDK_PATH)/libs/libdmabufheap/libdmabufheap.a
STATIC_LIBS += $(FD_WRAPPER_PATH)/libfdwrapper_vendor.a
# needed by libdmabufheap
DYN_LIBS += -lrt
STATIC_LIBS += $(MINK_CREDENTIALS_PATH)/libminkcredentials.a

includes += -I $(SDK_EXT)/inc
includes += -I $(MINK_SOCKET_PATH)/out/include/minkipc
includes += -I ./inc

SRC_C = src/main.c \
	src/profiling.c
OBJ_C = $(SRC_C:.c=.o)

QTVMSampleCA : $(OBJ_C)  $(STATIC_LIBS) | $(SDK_EXT)
	$(CXX) -o $@ $^ $(DYN_LIBS) $(sanl)

%.o : %.c
	$(CC) -o $@ -c $< $(cflags) -g $(includes) $(sanc)

$(SDK_EXT) $(STATIC_LIBS) :
	$(MAKE) -C $(dir $@)

clean :
	rm -f QTVMSampleCA $(OBJ_C)
