TARGET = libcommon.a

top = ../..

INC_DIRS = -I inc \
           -I $(top)/daemons/mink/inc \
           -I $(top)/libs/libcommon/inc

cflags = -g -fPIC

ifdef ONTARGET
sdkpath = ${SDK_INT_DIR}
else
cflags += -DOFFTARGET
sdkpath = $(top)/../../SDK/QTVM/int
INC_DIRS += -I $(top)/../../Transport/MinkSocket/out/include/minkipc
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

ifeq ($(OEM_VM), true)
INC_DIRS += -I $(top)/configs/oemvm
cflags += -DOEM_VM
else
INC_DIRS += -I $(top)/configs/qtvm
endif

INCS = $(wildcard inc/*.h)
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:c=o)

# Define PLATFORM inputs
IDL_SRC_DIR    = idl
IDL_SRC        = $(wildcard $(IDL_SRC_DIR)/*.idl)
IDL_INVOKE_SRC = $(wildcard $(IDL_SRC_DIR)/I*.idl)

# Define PLATFORM targets
IDL_OUT_DIR    = inc
IDL_H          = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%.h,          $(IDL_SRC))
IDL_HPP        = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%.hpp,        $(IDL_SRC))
IDL_INVOKE_H   = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%_invoke.h,   $(IDL_INVOKE_SRC))
IDL_INVOKE_HPP = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%_invoke.hpp, $(IDL_INVOKE_SRC))

HEADERS := $(IDL_H) $(IDL_HPP) $(IDL_INVOKE_H) $(IDL_INVOKE_HPP)

PHONY := $(TARGET)

ME = $(shell basename $(CURDIR))

all $(ME) : $(HEADERS) $(TARGET)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c $(INCS)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(INC_DIRS) $<

# Define PLATFORM recipes
$(IDL_OUT_DIR)/%.h : $(IDL_SRC_DIR)/%.idl
	$(sdkpath)/minkidl -I $(sdkpath)/idl -o $@ $<
$(IDL_OUT_DIR)/%.hpp : $(IDL_SRC_DIR)/%.idl
	$(sdkpath)/minkidl -I $(sdkpath)/idl -o $@ $< --cpp
$(IDL_OUT_DIR)/%_invoke.h: $(IDL_SRC_DIR)/%.idl
	$(sdkpath)/minkidl -I $(sdkpath)/idl -o $@ $< --skel
$(IDL_OUT_DIR)/%_invoke.hpp: $(IDL_SRC_DIR)/%.idl
	$(sdkpath)/minkidl -I $(sdkpath)/idl -o $@ $< --skel --cpp

PHONY += clean
clean:
	rm -rf $(TARGET) $(OBJS)
	rm -f $(HEADERS)

install:

.PHONY = $(PHONY)
