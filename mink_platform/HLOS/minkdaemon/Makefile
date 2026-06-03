ifeq ($(OFFTARGET), false)
top = $(WORKSPACE)
else
top = ../../../../../../../..
endif

minkShipPath      = $(top)/mink/vendor/qcom/proprietary/mink
shipPath          = $(top)/security/securemsm
noshipPath        = $(top)/security/securemsm-noship
credentialsPath   = $(minkShipPath)/Credentials
minkDaemonPath    = $(minkShipPath)/Platform/HLOS/minkdaemon
minkHubPath       = $(minkShipPath)/Hub/MinkHub
minkTransportPath = $(minkShipPath)/Transport/MinkSocket
libdmaBufHeapPath = $(minkShipPath)/SDK/QTVM/libs/libdmabufheap

cflags = -fPIC -Wno-unused-parameter -Wno-unused-function -D_FORTIFY_SOURCE=2 -O3

includes = -I $(sdkPath)/inc \
           -I $(credentialsPath)/inc \
           -I $(shipPath)/mink/inc/interface \
           -I $(shipPath)/mink/inc/uid \
           -I $(shipPath)/mink/inc/qtvm \
           -I $(shipPath)/smcinvoke/QCBOR/inc \
           -I $(noshipPath)/qwes/secure_channel/inc \
           -I $(noshipPath)/qwes/daemon/common/inc \
           -I $(minkDaemonPath)/inc \
           -I $(minkHubPath)/interface \
           -I $(minkHubPath)/inc \
           -I $(minkTransportPath)/sock \
           -I $(minkTransportPath)/include

MINKDAEMON_SRC = $(wildcard src/*.cpp)

MINKDAEMON_OBJ = $(MINKDAEMON_SRC:cpp=o)

HLOS_DIR = hlos
$(shell mkdir -p $(HLOS_DIR))

DYN_LIBS:=

ifeq ($(OFFTARGET), false)
linkflags = -lminkcredentials -lminkhub -lminksocket_vendor
sdkPath = $(minkShipPath)/SDK/QTVM
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
DYN_LIBS += $(minkTransportPath)/libminksocket_vendor.a $(minkHubPath)/libminkhub.a $(credentialsPath)/libminkcredentials.a
sdkPath = $(minkShipPath)/SDK/QTVM/int
endif

ifeq ($(STUB), false)
linkflags += -ldmabufheap
else
DYN_LIBS += $(libdmaBufHeapPath)/libdmabufheap.a
cflags += -DSTUB
endif

TARGET = $(HLOS_DIR)/minkdaemon

all: $(TARGET)

.PHONY: $(TARGET)

$(TARGET): $(MINKDAEMON_OBJ) $(DYN_LIBS)
	$(CXX) -o $@ $^ $(sanl) $(linkflags) $(DYN_LIBS) -pthread -lcrypto -lssl -lrt -ldl

%.o: %.cpp
	$(CXX) -o $@ -c $(cflags) $(includes) $<

%.o: %.c
	$(CC) -o $@ -c $(cflags) $(includes) $<

$(DYN_LIBS) : .force
	$(MAKE) -C $(minkHubPath)
	$(MAKE) -C $(credentialsPath)
	$(MAKE) -C $(minkTransportPath)
	$(MAKE) -C $(libdmaBufHeapPath)

PHONY += .force
.force :

clean_libs:
	$(MAKE) -C $(minkHubPath) clean
	$(MAKE) -C $(credentialsPath) clean
	$(MAKE) -C $(minkTransportPath) clean
	$(MAKE) -C $(libdmaBufHeapPath) clean

clean: clean_libs
	rm -rf $(MINKDAEMON_OBJ) $(HLOS_DIR)

install:

.PHONY = $(PHONY)
