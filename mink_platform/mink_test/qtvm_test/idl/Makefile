ifdef ONTARGET
idlpath = $(SDK_EXT_DIR)
else
top     = ../..
sdkpath = $(top)/../mink/SDK/QTVM/ext
idlpath = $(sdkpath)
endif

# Define inc directory
INC_DIR        = ../inc

# Define inputs
IDL_SRC_DIR    = .
IDL_SRC        = $(wildcard $(IDL_SRC_DIR)/*.idl)
IDL_INVOKE_SRC = $(wildcard $(IDL_SRC_DIR)/I*.idl)

# Define targets
IDL_OUT_DIR    = $(INC_DIR)
IDL_H          = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%.h,          $(IDL_SRC))
IDL_HPP        = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%.hpp,        $(IDL_SRC))
IDL_INVOKE_H   = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%_invoke.h,   $(IDL_SRC))
IDL_INVOKE_HPP = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%_invoke.hpp, $(IDL_SRC))

HEADERS := $(IDL_H) $(IDL_HPP) $(IDL_INVOKE_H) $(IDL_INVOKE_HPP)

$(IDL_OUT_DIR)/%.h : %.idl
	$(idlpath)/minkidl -o $@ $<
$(IDL_OUT_DIR)/%.hpp : %.idl
	$(idlpath)/minkidl -o $@ $< --cpp
$(IDL_OUT_DIR)/%_invoke.h: %.idl
	$(idlpath)/minkidl -o $@ $< --skel
$(IDL_OUT_DIR)/%_invoke.hpp: %.idl
	$(idlpath)/minkidl -o $@ $< --skel --cpp

all : $(HEADERS)

clean:
	rm -rf $(HEADERS)
