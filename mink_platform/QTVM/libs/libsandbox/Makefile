TARGET = libsandbox.a

ifeq ($(ONTARGET)_$(OEM_VM), _true)
TARGET = libsandbox-oemvm.a
endif

top = ../..
libcommonpath       = $(top)/libs/libcommon
libservicecenter    = $(top)/libs/libservicecenter

INC_DIRS =  -I inc                       \
            -I $(top)/daemons/mink/inc   \
            -I $(libservicecenter)/inc   \
            -I $(top)/libs/libcommon/inc

cflags = -g -fPIC

ifeq ($(OEM_VM), true)
cflags += -DOEM_VM
INC_DIRS += -I $(top)/configs/oemvm
else
INC_DIRS += -I $(top)/configs/qtvm
endif

ifdef ONTARGET
sdkpath = ${SDK_INT_DIR}
else
cflags += -DOFFTARGET
sdkpath = $(top)/../../SDK/QTVM/int
INC_DIRS += -I $(top)/../../Transport/MinkSocket/out/include/minkipc
INC_DIRS += -I $(top)/../../../../../../../vendor/qcom/proprietary/linuxsecurity/lib/libcontainer
INC_DIRS += -I $(top)/../../Hub/MinkHub/interface
endif

INC_DIRS += -I $(sdkpath)/inc

ifneq ($(STUB), false)
cflags += -DSTUB
endif

ifneq ($(QTVM_TEST), false)
cflags += -DQTVM_TEST
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

SRC_DIR = src
OBJ_DIR = out
ifeq ($(ONTARGET)_$(OEM_VM), _true)
OBJ_DIR = oemvm
DYN_LIBS += $(libservicecenter)/libservicecenter-oemvm.a
else
DYN_LIBS += $(libservicecenter)/libservicecenter.a
endif

SRCS = $(SRC_DIR)/libcontainer.c
OBJS = $(subst $(SRC_DIR),$(OBJ_DIR),$(SRCS:c=o))

PHONY := $(TARGET)

ME = $(shell basename $(CURDIR))

all $(ME) : $(DYN_LIBS) $(TARGET)

$(TARGET): $(DYN_LIBS) $(OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(INC_DIRS) $<

$(OBJ_DIR):
	mkdir -p $@

$(DYN_LIBS) : .force
	$(MAKE) -C $(dir $@)

PHONY += .force
.force :

PHONY += clean
clean:
	rm -rf $(TARGET) $(OBJ_DIR)

.PHONY = $(PHONY)
