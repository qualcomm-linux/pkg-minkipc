ifndef ONTARGET
WORKSPACE = ../../../../../..
endif

shipPath = $(WORKSPACE)/security/securemsm
internalPath = $(WORKSPACE)/security/securemsm-internal
minkPath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink
minkInternalPath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink-internal
examplehubPath = $(minkPath)/Hub/ExampleHub
credentialsPath = $(minkPath)/Credentials
minksocketPath = $(minkPath)/Transport/MinkSocket
fdwrapperPath = $(minkPath)/Transport/FdWrapper
minktransportTestPath = $(minkInternalPath)/minktransport_test
gtestPath = $(internalPath)/gtest-1.10.0

cflag = -fPIC -Wall -Wno-unused-parameter -Wno-unused-function -Werror

includes = -I $(shipPath)/mink/inc/interface \
           -I $(shipPath)/mink/inc/uid \
           -I $(shipPath)/smcinvoke/inc \
           -I $(minksocketPath) \
           -I $(minksocketPath)/sock \
           -I $(minksocketPath)/sharemm/inc \
           -I $(minksocketPath)/sock/sim_port \
           -I $(minksocketPath)/include \
           -I $(minksocketPath)/kernel \
           -I $(examplehubPath)/inc \
           -I $(credentialsPath)/inc/ \
           -I service/inc \
           -I scenarios/inc \
           -I main

MAIN_SRC = $(wildcard main/*.cpp)
MAIN_OBJ = $(MAIN_SRC:cpp=o)
SCENARIOS_SRC = $(wildcard scenarios/src/*.cpp)
SCENARIOS_OBJ = $(SCENARIOS_SRC:cpp=o)
SERVICE_SRC = $(wildcard service/src/*.c)
SERVICE_OBJ = $(SERVICE_SRC:c=o)
EXT_SRC = $(wildcard ext/*.cpp)
EXT_OBJ = $(EXT_SRC:cpp=o)

ifdef ONTARGET
cflags += -DVNDR_SMCINVOKE_QRTR -DVNDR_VSOCK -D_XOPEN_SOURCE=600
includes_gtest = -I $(gtestPath)/googletest/include/gtest
linkflags = -lgtest -lgtest_main -ldmabufheap -lvmmem -lqrtr -lminksocket_vendor -lfdwrapper_vendor
TARGET = minktransport_test
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
includes_gtest = -I $(gtestPath)/googletest/include
GTEST_LIBS = $(gtestPath)/libgtest.a $(gtestPath)/libgtest_main.a
MINK_LIB = $(minksocketPath)/libminksocket_vendor.so
HUB = exampleHub exampleHubLegacy
TARGET = unit_test
endif

all: $(TARGET) $(HUB)

.PHONY: unit_test

$(TARGET): $(MAIN_OBJ) $(SCENARIOS_OBJ) $(SERVICE_OBJ) $(MINK_LIB) $(GTEST_LIBS)
	$(CXX) -o $@ $^ $(sanl) -pthread -lrt $(linkflags)

%.o: %.cpp
	$(CXX) -o $@ -c -std=c++11 $(cflags) -g $(includes) $(includes_gtest) $(CXXFLAGS) $<

%.o: %.c
	$(CC) -o $@ -c $(cflags) -g $(includes) $(CFLAGS) $<

$(MINK_LIB):
	$(MAKE) -C $(minksocketPath)

$(GTEST_LIBS):
	$(MAKE) -C $(gtestPath)

$(HUB):
	$(MAKE) -C $(examplehubPath)
	cp $(examplehubPath)/exampleHub .
	cp $(examplehubPath)/exampleHubLegacy .

clean: clean_libs clean_sockfd
	rm -rf $(TARGET) $(HUB)
	rm -rf $(MAIN_OBJ) $(SCENARIOS_OBJ) $(SERVICE_OBJ) $(EXT_OBJ)

clean_libs:
ifndef ONTARGET
# Clean up for offtarget only
	$(MAKE) -C $(examplehubPath) clean
	$(MAKE) -C $(minksocketPath) clean
	$(MAKE) -C $(fdwrapperPath) clean
	$(MAKE) -C $(credentialsPath) clean
	$(MAKE) -C $(gtestPath) clean
endif

clean_sockfd:
	rm -f exampleHub_PVM_sockUnix exampleHub_PVM_sockQRTR
	rm -f exampleHub_QTVM_sockUnix exampleHub_QTVM_sockQRTR exampleHub_QTVM_sockVSOCK
	rm -f exampleHub_OEMVM_sockUnix exampleHub_OEMVM_sockVsock

#do installation in minktransport-test.bb
install:
