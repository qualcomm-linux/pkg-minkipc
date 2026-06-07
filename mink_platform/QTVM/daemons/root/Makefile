top                 = ../..
libsandboxpath      = $(top)/libs/libsandbox
libcommonpath       = $(top)/libs/libcommon
libservicecenter    = $(top)/libs/libservicecenter
libminksocketpath   = $(top)/../../Transport/MinkSocket

includes = -I inc

cflags = -Wall

USE_MINIJAIL = true

ifdef ONTARGET
sdkpath = $(SDK_INT_DIR)
includes += -I $(WORKSPACE)/security/securemsm/smcinvoke/inc
linkflags += -lminksocket_vendor -lfdwrapper_vendor -lselinux -lqcbor -lyaml
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
sdkpath = $(top)/../../SDK/QTVM/int
includes += -I $(libminksocketpath)/out/include/minkipc \
            -I $(top)/../../../../../../../vendor/qcom/proprietary/linuxsecurity/lib/libcontainer \
            -I $(top)/../../Hub/MinkHub/interface
endif

ifeq ($(OEM_VM), true)
configpath += $(top)/configs/oemvm
cflags += -DOEM_VM
else
configpath += $(top)/configs/qtvm
endif

includes += -I $(sdkpath)/inc \
            -I $(libcommonpath)/inc \
            -I $(configpath)

DYN_LIBS += $(libcommonpath)/libcommon.a

ifdef ONTARGET
PREBUILT_LIBS = $(wildcard $(sdkpath)/libs/*.a)
else
# offtarget must link libminksocket_vendor.a first
PREBUILT_LIBS_ALL = $(wildcard $(sdkpath)/libs/*.a)
PREBUILT_LIBS = $(filter-out \
	$(sdkpath)/libs/libdmabufheap.a $(sdkpath)/libs/libminksocket_vendor.a $(sdkpath)/libs/libfdwrapper_vendor.a, \
	$(PREBUILT_LIBS_ALL))
PREBUILT_LIBS += \
	$(sdkpath)/libs/libminksocket_vendor.a \
	$(sdkpath)/libs/libfdwrapper_vendor.a \
	$(sdkpath)/libs/libdmabufheap.a
endif

ifeq ($(STUB), false)
linkflags += -lvmmem -ldmabufheap -lqrtr
else
cflags += -DSTUB
USE_MINIJAIL = false
endif

ifeq ($(USE_MINIJAIL), false)
# Stubbed, OEM_VM variant needs special library; else normal library
ifeq ($(ONTARGET)_$(OEM_VM), _true)
DYN_LIBS += $(libsandboxpath)/libsandbox-oemvm.a
DYN_LIBS += $(libservicecenter)/libservicecenter-oemvm.a
else
DYN_LIBS += $(libsandboxpath)/libsandbox.a
DYN_LIBS += $(libservicecenter)/libservicecenter.a
endif
else
linkflags += -lcontainer -lminijail
endif

USE_SW_ENCRYPT ?= false

ifneq ($(USE_SW_ENCRYPT), false)
cflags += -DUSE_SW_ENCRYPT
linkflags += -lminkdescriptor -lcap
endif

linkflags += -ldl

ifneq ($(QTVM_TEST), false)
cflags += -DQTVM_TEST
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

################    DEFINE TARGETS/DEPENDENCIES    ################
TVM_OUT_DIR = out
ifeq ($(ONTARGET)_$(OEM_VM), _true)
TVM_OUT_DIR = oemvm
endif

TVM_SRC_C = $(wildcard src/*.c)
TVM_OBJ_C = $(patsubst src/%.c, $(TVM_OUT_DIR)/%.o, $(TVM_SRC_C))

ME = $(shell basename $(CURDIR))

################    BUILD RULES    ################
all $(ME): $(DYN_LIBS) $(TVM_OUT_DIR)/TVMRoot

$(TVM_OUT_DIR)/TVMRoot: $(TVM_OBJ_C) $(PREBUILT_LIBS) $(DYN_LIBS)
	$(CXX) -o $@ $^ -pthread $(sanl) -lssl -lcrypto -lrt $(linkflags)

ifneq ($(STUB), false)
$(TVM_OUT_DIR)/TPrivilegedProcessManager.o : $(libsandboxpath)/libsandbox.a
endif

$(TVM_OUT_DIR)/%.o: src/%.c $(wildcard inc/*) $(wildcard $(configpath)/* ) | $(TVM_OUT_DIR)
	$(CC) -o $@ -c $< $(CFLAGS) $(cflags) -g $(includes) $(sanc)

$(TVM_OUT_DIR):
	mkdir -p $(TVM_OUT_DIR)

$(DYN_LIBS) : .force
	$(MAKE) -C $(dir $@)

PHONY += .force
.force :

clean_libs :
	for d in $(dir $(DYN_LIBS));               \
	do                                         \
		$(MAKE) -C $$d clean;                  \
	done

clean:
	rm -rf $(TVM_OUT_DIR)

ifndef ONTARGET
clean : clean_libs
endif

.PHONY = $(PHONY)
