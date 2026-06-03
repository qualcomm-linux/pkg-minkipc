ifeq ($(ONTARGET)_$(OEM_VM), _true)
TARGET = libservicecenter-oemvm.a
else
TARGET = libservicecenter.a
endif

top = ../..

INC_DIRS = -I inc \
           -I $(top)/daemons/mink/inc \
           -I $(top)/libs/libcommon/inc \
           -I $(top)/../../../../../../../vendor/qcom/proprietary/linuxsecurity/lib/libcontainer
cflags = -g -fPIC

ifdef ONTARGET
sdkpath = ${SDK_INT_DIR}
else
cflags += -DOFFTARGET
sdkpath = $(top)/../../SDK/QTVM/int
INC_DIRS += -I $(top)/../../Transport/MinkSocket/out/include/minkipc
endif

INC_DIRS += -I $(sdkpath)/inc

ifeq ($(OEM_VM), true)
cflags += -DOEM_VM
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
endif

INCS = $(wildcard inc/*.h)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(subst $(SRC_DIR),$(OBJ_DIR),$(SRCS:c=o))

PHONY := $(TARGET)

ME = $(shell basename $(CURDIR))

all $(ME) : $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(INC_DIRS) $<

$(OBJ_DIR):
	mkdir -p $@

install:

PHONY += clean
clean:
	rm -rf $(TARGET) $(OBJ_DIR)

.PHONY = $(PHONY)
