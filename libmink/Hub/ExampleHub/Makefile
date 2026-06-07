ifndef ONTARGET
WORKSPACE = ../../../../../../..
endif

minkPath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink
shipPath = $(WORKSPACE)/security/securemsm
examplehubPath = $(minkPath)/Hub/ExampleHub
credentialsPath = $(minkPath)/Credentials
minksocketPath = $(minkPath)/Transport/MinkSocket

cflags = -fPIC -Wno-unused-parameter -Wno-unused-function -D_FORTIFY_SOURCE=2 -O3

includes = -I $(shipPath)/mink/inc/interface \
           -I $(shipPath)/mink/inc/uid \
           -I $(shipPath)/mink/inc/qtvm \
           -I $(shipPath)/smcinvoke/QCBOR/inc \
           -I $(minksocketPath)/sock \
           -I $(minksocketPath)/sock/sim_port \
           -I $(minksocketPath)/include \
           -I $(minksocketPath)/kernel \
           -I $(minksocketPath)/sharemm/inc \
           -I inc

MANAGER_SRC = src/ServiceManager.cpp \
              src/ServiceOpener.cpp \
              src/CRegisterModule.cpp \
              src/VMCredentials.cpp \
              src/MinkHub.cpp \
              src/ServiceModuleOpener.cpp
MANAGER_OBJ = $(MANAGER_SRC:cpp=o)
HUB_SRC = src/ExampleHub.cpp
HUB_OBJ = $(HUB_SRC:cpp=o)
HUBLEGACY_SRC = src/ExampleHubLegacy.cpp
HUBLEGACY_OBJ = $(HUBLEGACY_SRC:cpp=o)

ifdef ONTARGET
cflags += -DVNDR_SMCINVOKE_QRTR -DVNDR_VSOCK -D_XOPEN_SOURCE=600
linkflags = -ldmabufheap -lvmmem -lqrtr -lminksocket_vendor -lfdwrapper_vendor -lminkcredentials -lqcbor
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
TRANSPORT_LIB = $(minksocketPath)/libminksocket_vendor.so
CRED_LIB = $(credentialsPath)/libminkcredentials.a
includes += -I $(credentialsPath)/inc
endif

TARGET = exampleHub
TARGET_LEGACY = exampleHubLegacy

all: $(TARGET) $(TARGET_LEGACY)

.PHONY: $(TARGET) $(TARGET_LEGACY)

$(TARGET): $(HUB_OBJ) $(MANAGER_OBJ) $(TRANSPORT_LIB) $(CRED_LIB)
	$(CXX) -o $@ $^ $(sanl) $(linkflags) -pthread -lcrypto -lssl -lrt

$(TARGET_LEGACY): $(HUBLEGACY_OBJ) $(MANAGER_OBJ) $(TRANSPORT_LIB) $(CRED_LIB)
	$(CXX) -o $@ $^ $(sanl) $(linkflags) -pthread -lcrypto -lssl -lrt

%.o: %.cpp
	$(CXX) -o $@ -c $(CFLAGS) $(cflags) $(includes) $(includes_gtest) $<

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $(cflags) $(includes) $<

$(TRANSPORT_LIB):
	$(MAKE) -C $(minksocketPath)

$(CRED_LIB):
	$(MAKE) -C $(credentialsPath)

clean_lib:
ifndef ONTARGET
	$(MAKE) -C $(minksocketPath) clean
	$(MAKE) -C $(credentialsPath) clean
endif

clean: clean_lib
	rm -f exampleHub_PVM_sockUnix exampleHub_PVM_sockQRTR
	rm -f exampleHub_QTVM_sockUnix exampleHub_QTVM_sockQRTR exampleHub_QTVM_sockVSOCK
	rm -f exampleHub_OEMVM_sockUnix exampleHub_OEMVM_sockVsock
	rm -f $(TARGET) $(TARGET_LEGACY)
	rm -f $(HUB_OBJ) $(HUBLEGACY_OBJ) $(MANAGER_OBJ)

#do installation in  mink-hub.bb and  mink-hub-oemvm.bb
install:
