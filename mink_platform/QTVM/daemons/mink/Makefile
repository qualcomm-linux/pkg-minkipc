top                 = ../..
libtunnelinvokepath = $(top)/libs/libtunnelinvoke
libcommonpath       = $(top)/libs/libcommon
libservicecenter    = $(top)/libs/libservicecenter
libminksocketpath   = $(top)/../../Transport/MinkSocket
libminkhubpath      = $(top)/../../Hub/MinkHub

includes = -I inc

cflags = -Wall

ifdef ONTARGET
sdkpath = $(SDK_INT_DIR)
includes += -I $(WORKSPACE)/security/securemsm/smcinvoke/inc
libminkcredentialspath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink/Credentials
includes += -I $(libminkcredentialspath)/inc
# Workaround: must pass minksocket_vendor first before minkdesciptor to linker
# to address crash issues caused by duplicate fdwrapper implementation
linkflags += -lminksocket_vendor -lminkdescriptor -lfdwrapper_vendor -lqcbor -lyaml -lminkhub -lminkcredentials
includes += -I $(libtunnelinvokepath)/inc
DYN_LIBS += $(libtunnelinvokepath)/libtunnelinvoke.a
else
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags += -DOFFTARGET
sdkpath = $(top)/../../SDK/QTVM/int
libminkcredentialspath = $(top)/../../Credentials
includes += -I $(libminksocketpath)/out/include/minkipc \
            -I $(libminkcredentialspath)/inc \
            -I $(top)/../../../../../../../vendor/qcom/proprietary/linuxsecurity/lib/libcontainer \
            -I $(libminkhubpath)/interface
DYN_LIBS += $(libminkhubpath)/libminkhub.a
DYN_LIBS += $(libminkcredentialspath)/libminkcredentials.a
endif

ifeq ($(OEM_VM), true)
configpath += $(top)/configs/oemvm
cflags += -DOEM_VM
else
configpath += $(top)/configs/qtvm
endif

includes += -I $(sdkpath)/inc \
            -I $(libcommonpath)/inc \
            -I $(libservicecenter)/inc \
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
endif

linkflags += -ldl

ifneq ($(QTVM_TEST), false)
cflags += -DQTVM_TEST
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

ifeq ($(ONTARGET)_$(OEM_VM), _true)
DYN_LIBS += $(libservicecenter)/libservicecenter-oemvm.a
else
DYN_LIBS += $(libservicecenter)/libservicecenter.a
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
all $(ME): $(PREBUILT_LIBS) $(DYN_LIBS) $(TVM_OUT_DIR)/TVMMink

$(TVM_OUT_DIR)/TVMMink: $(TVM_OBJ_C)
	$(CXX) -o $@ $^ $(PREBUILT_LIBS) $(DYN_LIBS) -pthread $(sanl) -lssl -lcrypto -lrt $(linkflags)

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
	rm -rf $(TVM_OUT_DIR) $(IDL_H) $(IDL_HPP) $(IDL_INVOKE_H) $(IDL_INVOKE_HPP)

ifndef ONTARGET
clean : clean_libs
endif

install:

.PHONY = $(PHONY)
