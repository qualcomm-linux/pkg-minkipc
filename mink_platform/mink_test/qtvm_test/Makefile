# This Makefile is for LE only. There is an Android.mk for LA build instructions.
top      = ../..

includes =  -I inc

cflags = -Wall

ifneq ($(QTVM_TEST), false)
cflags += -DQTVM_TEST
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

ifdef ONTARGET
sdkpath = $(SDK_INT_DIR)
libspath = $(sdkpath)/libs
linkflags = -lgtest -lgtest_main
minkpath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink
libminkcredentialspath = $(minkpath)/Credentials
minkdaemonpath = $(minkpath)/Platform/QTVM/daemons/mink
includes += -I $(sdkpath)/inc \
            -I $(libminkcredentialspath)/inc \
            -I $(minkdaemonpath)/inc \
            -I $(WORKSPACE)/security/securemsm/mink/inc/uid
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
qtvmpath            = $(top)/mink/Platform/QTVM
hlospath            = $(top)/mink-noship/Platform/HLOS
sdkpath             = $(top)/mink/SDK/QTVM
minkhub             = $(top)/mink/Hub/MinkHub
minkdaemonpath      = $(qtvmpath)/daemons/mink
prelauncherpath     = $(qtvmpath)/daemons/prelauncher
rootdaemonpath      = $(qtvmpath)/daemons/root
hlosminkdaemonpath  = $(hlospath)/minkdaemon
libspath            = $(sdkpath)/int/libs
gtestpath           = $(top)/../../../../security/securemsm-internal/gtest-1.10.0
libcommonpath       = $(qtvmpath)/libs/libcommon
libminksocketpath   = $(top)/mink/Transport/MinkSocket
libminkcredentialspath   = ${top}/mink/Credentials

includes += -I $(sdkpath)/int/inc \
            -I $(minkdaemonpath)/inc \
            -I $(libcommonpath)/inc \
            -I $(libminksocketpath)/include \
            -I $(libminksocketpath)/out/include/minkipc \
            -I $(top)/../../../../security/securemsm/mink/inc/uid \
            -I $(libminkcredentialspath)/inc \
            -I $(minkhub)/interface

includes_gtest = -I $(gtestpath)/googletest/include

GTEST_LIBS = $(gtestpath)/libgtest.a \
             $(gtestpath)/libgtest_main.a
endif

SRC      = $(wildcard src/*.cpp)
OBJ      = $(SRC:cpp=o)

ifneq ($(STUB), false)
QTVM_LIBS = $(libspath)/libminksocket_vendor.a  $(libspath)/libdmabufheap.a  $(libspath)/libfdwrapper_vendor.a
cflags += -DSTUB
CRED_LIB = $(libminkcredentialspath)/libminkcredentials.a
else
linkflags += -ldmabufheap -lvmmem -lqrtr -lminksocket_vendor -lfdwrapper_vendor -lminkcredentials
linkflags += -lminkdescriptor -lqcbor
endif

includes += -I mink_daemon_unit_test/inc

QTVM_DIR  = out
OEMVM_DIR = oemvm
HLOS_DIR  = hlos

ME = $(shell basename $(CURDIR))

# Always build the testapps
all $(ME): testapps

ifndef ONTARGET
# Build everything else offtarget only
all $(ME): qtvmd oemvmd hlosd mink_test copy_binaries
endif

mink_test: $(OBJ) $(QTVM_LIBS) $(GTEST_LIBS) $(CRED_LIB) | idl_headers
	$(CXX) -o $@ $^ -pthread $(sanl) -lssl -lcrypto -Wall -Werror -lrt $(linkflags)

%.o: %.cpp | idl_headers
	$(CXX) -o $@ -c -std=c++11 $(CXXFLAGS) $(cflags) -g $(includes) $(includes_gtest) $<

%.o: %.c | idl_headers
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(includes) $<

testapps : sdk
	$(MAKE) -C apps

idl_headers : sdk
	$(MAKE) -C idl

sdk:
ifndef ONTARGET
	$(MAKE) -C  $(sdkpath)
endif

$(CRED_LIB):
ifndef ONTARGET
	$(MAKE) -C  $(libminkcredentialspath)
endif

qtvmd:
	$(MAKE) -C  $(qtvmpath)

oemvmd:
	$(MAKE) -C  $(qtvmpath) OEM_VM=true

hlosd:
	$(MAKE) -C $(hlosminkdaemonpath)

$(GTEST_LIBS) :
	$(MAKE) -C $(gtestpath)

copy_binaries: | $(QTVM_DIR) $(OEMVM_DIR) $(HLOS_DIR)
	cp $(rootdaemonpath)/$(QTVM_DIR)/TVMRoot $(QTVM_DIR)/
	cp $(prelauncherpath)/$(QTVM_DIR)/TVMPrelauncher $(QTVM_DIR)/
	cp $(minkdaemonpath)/$(QTVM_DIR)/TVMMink $(QTVM_DIR)/
	cp $(rootdaemonpath)/$(OEMVM_DIR)/TVMRoot $(OEMVM_DIR)/XVMRoot
	cp $(prelauncherpath)/$(OEMVM_DIR)/TVMPrelauncher $(OEMVM_DIR)/XVMPrelauncher
	cp $(minkdaemonpath)/$(OEMVM_DIR)/TVMMink $(OEMVM_DIR)/XVMMink
	cp $(hlosminkdaemonpath)/$(HLOS_DIR)/minkdaemon $(HLOS_DIR)/minkdaemon

# QTVM has all of the embedded processes and none of the TPs
$(QTVM_DIR): testapps
	mkdir -p $@
	cp apps/out/Embedded* $@

# OEMVM has none of the embedded processes and all of the TPs
$(OEMVM_DIR): testapps
	mkdir -p $@
	cp apps/out/* $@
	rm -f $@/Embedded*

$(HLOS_DIR):
	mkdir -p $@

TEST_SRC = $(wildcard mink_daemon_unit_test/src/*.cpp)
TEST_OBJ = $(TEST_SRC:cpp=o)
TVM_OUT_DIR = $(minkdaemonpath)/$(QTVM_DIR)
COMMON_OBJ_DIR = $(libcommonpath)/src

MINK_DAEMON_OBJS = $(COMMON_OBJ_DIR)/MetadataInfo.o \
                   $(COMMON_OBJ_DIR)/MDScan.o       \
                   $(COMMON_OBJ_DIR)/MemSCpy.o      \
                   $(TVM_OUT_DIR)/TModule.o      \
                   $(TVM_OUT_DIR)/CTRegisterModule.o

unit_test : $(TEST_OBJ) $(MINK_DAEMON_OBJS) $(QTVM_LIBS) $(GTEST_LIBS)
	$(CXX) -o $@ $^ -pthread $(sanl) -lssl -lcrypto -Wall -Werror -lrt $(linkflags)

clean:
	$(MAKE) -C idl clean
ifndef ONTARGET
	$(MAKE) -C apps clean
	$(MAKE) -C  $(sdkpath) clean
	$(MAKE) -C  $(qtvmpath) clean
	$(MAKE) -C  $(qtvmpath) clean OEM_VM=true
	$(MAKE) -C  $(hlosminkdaemonpath) clean
	$(MAKE) -C $(gtestpath) clean
	$(MAKE) -C $(libminkcredentialspath) clean
	rm -f $(TEST_OBJ) $(MINK_DAEMON_OBJS) unit_test
	rm -f log_*
	rm -rf $(QTVM_DIR) $(OEMVM_DIR) $(HLOS_DIR)
	rm -f wake_*
endif
	rm -f $(OBJ) mink_test
	find -type s -exec rm {} \;

install:

.PHONY: $(PHONY)
