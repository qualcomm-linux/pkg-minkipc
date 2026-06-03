ifdef ONTARGET
# $(WORKSPACE) points to <LE Workspace>/src in yocto env
else
WORKSPACE = ../../../../../../..
endif

libminksocketpath = ../../Transport/MinkSocket
libfdwrapperpath  = ../../Transport/FdWrapper
libbacktracepath  = libs/libbacktrace
libmodulepath     = libs/libmodule
libdmabufpath     = libs/libdmabufheap
libvmmempath      = libs/libvmmem
minkheaderpath    = $(WORKSPACE)/security/securemsm/mink
interfacepath     = $(minkheaderpath)/inc/interface
sectoolspath      = $(WORKSPACE)/vendor/qcom/proprietary/sectools/Linux

ME = $(shell basename $(CURDIR))
INT_DIR = int
EXT_DIR = ext

##############################  IDL Headers  ###################################
# Define inputs
IDL_SRC_DIR    = idl
IDL_SRC        = $(wildcard $(IDL_SRC_DIR)/*.idl)
IDL_INVOKE_SRC = $(wildcard $(IDL_SRC_DIR)/I*.idl)

INT_BLK_LIST =

# Define INTERNAL targets
INT_IDL          = $(filter-out $(INT_BLK_LIST),$(IDL_SRC))
INT_INVOKE_IDL   = $(filter-out $(INT_BLK_LIST),$(IDL_INVOKE_SRC))
INT_IDL_OUT      = $(INT_DIR)/inc
INT_IDL_H        = $(patsubst $(IDL_SRC_DIR)/%.idl, $(INT_IDL_OUT)/%.h,          $(INT_IDL))
INT_IDL_HPP      = $(patsubst $(IDL_SRC_DIR)/%.idl, $(INT_IDL_OUT)/%.hpp,        $(INT_IDL))
INT_IDL_INVK_H   = $(patsubst $(IDL_SRC_DIR)/%.idl, $(INT_IDL_OUT)/%_invoke.h,   $(INT_INVOKE_IDL))
INT_IDL_INVK_HPP = $(patsubst $(IDL_SRC_DIR)/%.idl, $(INT_IDL_OUT)/%_invoke.hpp, $(INT_INVOKE_IDL))

INT_IDL_HEADERS := $(INT_IDL_H) $(INT_IDL_HPP) $(INT_IDL_INVK_H) $(INT_IDL_INVK_HPP)

# Exclude private Platform services and duplicates of QTEE-provided IDLs
EXT_BLK_LIST = %TRegisterModule.idl %rModule.idl %IO.idl
# Exclude test-related IDLs
EXT_BLK_LIST += %TestService.idl %EmbeddedMissingNeverUnload.idl

# Define EXTERNAL targets
EXT_BLK_LIST_IDL = $(filter $(EXT_BLK_LIST),$(IDL_SRC))
EXT_IDL          = $(patsubst $(IDL_SRC_DIR)/%.idl, $(EXT_DIR)/idl/%.idl,        $(EXT_BLK_LIST_IDL))
EXT_IDL_OUT      = $(EXT_DIR)/inc
EXT_IDL_H        = $(patsubst $(IDL_SRC_DIR)/%.idl, $(EXT_IDL_OUT)/%.h,          $(EXT_BLK_LIST_IDL))
EXT_IDL_HPP      = $(patsubst $(IDL_SRC_DIR)/%.idl, $(EXT_IDL_OUT)/%.hpp,        $(EXT_BLK_LIST_IDL))
EXT_IDL_INVK_H   = $(patsubst $(IDL_SRC_DIR)/%.idl, $(EXT_IDL_OUT)/%_invoke.h,   $(EXT_BLK_LIST_IDL))
EXT_IDL_INVK_HPP = $(patsubst $(IDL_SRC_DIR)/%.idl, $(EXT_IDL_OUT)/%_invoke.hpp, $(EXT_BLK_LIST_IDL))

# Define INTERNAL recipes
$(INT_IDL_OUT)/%.h : $(IDL_SRC_DIR)/%.idl | $(INT_IDL_OUT)
	./minkidl -o $@ $<
$(INT_IDL_OUT)/%.hpp : $(IDL_SRC_DIR)/%.idl | $(INT_IDL_OUT)
	./minkidl -o $@ $< --cpp
$(INT_IDL_OUT)/%_invoke.h: $(IDL_SRC_DIR)/%.idl | $(INT_IDL_OUT)
	./minkidl -o $@ $< --skel
$(INT_IDL_OUT)/%_invoke.hpp: $(IDL_SRC_DIR)/%.idl | $(INT_IDL_OUT)
	./minkidl -o $@ $< --skel --cpp

# Define recipes
$(EXT_IDL_OUT)/%.h : $(IDL_SRC_DIR)/%.idl | $(EXT_IDL_OUT)
	./minkidl -o $@ $<
$(EXT_IDL_OUT)/%.hpp : $(IDL_SRC_DIR)/%.idl | $(EXT_IDL_OUT)
	./minkidl -o $@ $< --cpp
$(EXT_IDL_OUT)/%_invoke.h: $(IDL_SRC_DIR)/%.idl | $(EXT_IDL_OUT)
	./minkidl -o $@ $< --skel
$(EXT_IDL_OUT)/%_invoke.hpp: $(IDL_SRC_DIR)/%.idl | $(EXT_IDL_OUT)
	./minkidl -o $@ $< --skel --cpp

############################  Exported libraries  ##############################
includes = -I libmodulepath/inc \
           -I $(INT_DIR)/inc

cflags = -g -fPIC

LIBMODULE_SRC_LIB = $(libbacktracepath)/libbacktrace.a

ifndef ONTARGET
cflags += -DOFFTARGET
offtarget_flags = -o
includes += -I $(libminksocketpath)/sock
LIBMODULE_SRC_LIB  += $(libminksocketpath)/libminksocket_vendor.a
LIBMODULE_SRC_LIB  += $(libfdwrapperpath)/libfdwrapper_vendor.a
endif

ifneq ($(QTVM_TEST), false)
cflags += -DQTVM_TEST
endif

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

ifneq ($(STUB), false)
cflags += -DSTUB
endif

LIBMODULE_INCS = $(wildcard $(libmodulepath)/inc/*.h)
LIBMODULE_SRCS = $(wildcard $(libmodulepath)/src/*.c)
LIBMODULE_OBJS = $(LIBMODULE_SRCS:c=o)

# TODO review headers which truly need to be exported to SDK
HEADERS_SRCS = $(libbacktracepath)/inc/backtrace.h

ifneq ($(STUB), false)
HEADERS_SRCS += $(libdmabufpath)/include/BufferAllocatorWrapper.h \
                $(libdmabufpath)/include/dmabufheap-defs.h        \
                $(libvmmempath)/include/vmmem.h                   \
                $(libvmmempath)/include/vmmem_wrapper.h
endif

INT_HEADERS_DSTS = $(addprefix $(INT_DIR)/inc/, $(notdir $(HEADERS_SRCS)))
HEADERS_DIRS = $(dir $(HEADERS_SRCS))

INT_IDL_DIR = $(INT_DIR)/idl
INT_IDL_FILES = $(addprefix $(INT_IDL_DIR)/, $(notdir $(IDL_SRC)))

# Setup variables for building/copying static libraries
TMP_DIR             = ./tmp
INT_STATIC_LIBS_DIR = $(INT_DIR)/libs

INT_STATIC_LIB_SRC  = $(LIBMODULE_SRC_LIB)

ifneq ($(STUB), false)
INT_STATIC_LIB_SRC  += $(libdmabufpath)/libdmabufheap.a
endif

LIBMODULE_TMP_LIB       = $(addprefix $(TMP_DIR)/, $(notdir $(LIBMODULE_SRC_LIB)))
INT_STATIC_LIBS_LIST    = $(addprefix $(INT_STATIC_LIBS_DIR)/, $(notdir $(INT_STATIC_LIB_SRC)) libmodule.lib)

all $(ME) : INT_SDK EXT_SDK sampleapps

install :
.FORCE :

%.o: %.c $(INCS)
	$(CC) -o $@ -c $(CFLAGS) $(cflags) -g $(includes) $<

%.o: %.cpp $(INCS)
	$(CXX) -o $@ -c -std=c++11 $(CXXFLAGS) $(cflags) $(INC_DIRS) $<

# Recipes for building static libraries
$(INT_STATIC_LIB_SRC) : .FORCE
	$(MAKE) -C $(dir $@)
	$(MAKE) -C $(dir $@) install

vpath %.a $(dir $(INT_STATIC_LIB_SRC))

$(INT_STATIC_LIBS_DIR)/%.a : %.a | $(INT_STATIC_LIBS_DIR)
	cp $< $@

$(EXT_STATIC_LIBS_DIR)/%.a : %.a | $(EXT_STATIC_LIBS_DIR)
	cp $< $@

$(INT_STATIC_LIBS_DIR)/libmodule.lib : $(TMP_DIR)/libmodule.lib | $(INT_DIR)/inc
	cp $(libmodulepath)/inc/*.h $(INT_DIR)/inc/
	cp $< $@

$(EXT_STATIC_LIBS_DIR)/libmodule.lib : $(TMP_DIR)/libmodule.lib | $(EXT_DIR)/inc $(EXT_STATIC_LIBS_DIR)
	cp $(libmodulepath)/inc/*.h $(EXT_DIR)/inc/
	cp $< $@

# Recipes for creating libmodule.lib
$(TMP_DIR)/%.a : %.a | $(TMP_DIR)
	cp $< $@
	cd $(TMP_DIR) && $(AR) x $(notdir $@)

$(TMP_DIR)/libmodule.lib : $(LIBMODULE_OBJS) $(LIBMODULE_TMP_LIB)
	$(AR) crs $@ $(LIBMODULE_OBJS) $(TMP_DIR)/*.o

# applib.o dependencies
$(LIBMODULE_OBJS) : $(INT_IDL_HEADERS) $(INT_HEADERS_DSTS)

# Recipe for copying header files from various places
$(INT_DIR)/inc/%.h : %.h | $(INT_DIR)/inc
	cp $< $@

vpath %.h $(HEADERS_DIRS)

$(INT_IDL_DIR)/%.idl : %.idl | $(INT_IDL_DIR)
	cp $< $@

vpath %.idl $(dir $(IDL_SRC))

################################################################################
$(INT_DIR) $(INT_DIR)/inc $(EXT_DIR) $(TMP_DIR) $(EXT_STATIC_LIBS_DIR) $(INT_STATIC_LIBS_DIR) $(INT_IDL_DIR) :
	mkdir -p $@
	cp $(interfacepath)/object.h $@

$(EXT_DIR)/inc :
	mkdir -p $@

# All files that are simply copied from source to internal/external SDK
COPY_FILES  = $(wildcard *.py) minkidl $(wildcard inc/*) marking.txt
INT_COPY_FILES  = $(addprefix $(INT_DIR)/, $(COPY_FILES))
#EXT_COPY_FILES = $(addprefix $(EXT_DIR)/, $(COPY_FILES))

$(INT_DIR)/% : % | $(INT_DIR)/inc
	cp -f $< $@

$(INT_DIR)/sectools :
	cp -f $(sectoolspath)/sectools $(INT_DIR)

PHONY += EXT_SDK
EXT_SDK : $(EXT_DIR)
	cp -R $(INT_DIR)/* $(EXT_DIR)/
	rm -rf $(EXT_IDL) $(EXT_IDL_H) $(EXT_IDL_HPP) $(EXT_IDL_INVK_H) $(EXT_IDL_INVK_HPP)

$(INT_DIR)/sampleapp/ $(INT_DIR)/sampleapp_cpp/ $(INT_DIR)/sampleCA/ :
	cp -rf $(notdir $(patsubst %/,%,$@)) $(INT_DIR)

# TODO: review minksocket-related headers that are being shipped
PHONY += INT_SDK
INT_SDK : $(INT_COPY_FILES) $(INT_IDL_HEADERS) $(INT_IDL_FILES) $(INT_HEADERS_DSTS) $(INT_STATIC_LIBS_LIST) $(INT_DIR)/sectools $(INT_DIR)/sampleapp/ $(INT_DIR)/sampleapp_cpp/ $(INT_DIR)/sampleCA/

clean :
	rm -rf $(INT_DIR) $(EXT_DIR) $(TMP_DIR) $(LIBMODULE_OBJS)

################################################################################
####  For internal testing only - Source copied into SDK separately        #####
################################################################################
TEST_DIR = apps

$(TEST_DIR)/ :
	mkdir -p $@

ifndef ONTARGET
build_flags = -o -d
endif

# Depend on INT_DIR otherwise it will trigger a re-build always
$(TEST_DIR)/sampleapp/bin/sampleapp : $(wildcard sampleapp/src/*) $(wildcard $(INT_DIR)/*/*) | $(TEST_DIR)/
	cp -r sampleapp/ $(TEST_DIR)/
	python3 $(EXT_DIR)/Build.py $(build_flags) --i $(TEST_DIR)/sampleapp/build.json || (echo "mycommand failed $$?"; exit 1)

$(TEST_DIR)/sampleapp_cpp/bin/sampleapp_cpp : $(wildcard sampleapp_cpp/src/*) $(wildcard $(INT_DIR)/*/*) | $(TEST_DIR)/
	cp -r sampleapp_cpp/ $(TEST_DIR)/
	python3 $(EXT_DIR)/Build.py $(build_flags) --i $(TEST_DIR)/sampleapp_cpp/build.json || (echo "mycommand failed $$?"; exit 1)

$(TEST_DIR)/sampleCA/QTVMSampleCA : $(wildcard sampleCA/src/*) | $(TEST_DIR)/
	cp -r sampleCA/ $(TEST_DIR)/
	$(MAKE) -C $(TEST_DIR)/sampleCA

sampleapps : $(TEST_DIR)/sampleapp/bin/sampleapp $(TEST_DIR)/sampleapp_cpp/bin/sampleapp_cpp

ifndef ONTARGET
sampleapps : $(TEST_DIR)/sampleCA/QTVMSampleCA
endif

PHONY += sampleapps

ifndef ONTARGET
clean : clean_libs

clean_libs :
	for d in $(dir $(INT_STATIC_LIB_SRC)); \
	do                                     \
		$(MAKE) -C $$d clean;              \
	done
	rm -rf apps
endif


.PHONY = $(PHONY)
