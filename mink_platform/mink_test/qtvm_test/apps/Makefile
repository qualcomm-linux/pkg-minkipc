top             = ..
apps_common     = $(top)/apps_common
DOWNLOADABLE    = $(filter-out Embed%, $(wildcard *Service))
EMBEDDED        = $(wildcard Embed*)
BIN_DIR         = out
DL_BINS         = $(foreach dir,$(DOWNLOADABLE), $(BIN_DIR)/$(dir))
EMBED_BINS      = $(foreach dir,$(EMBEDDED), $(BIN_DIR)/$(dir))

ifdef ONTARGET
sdkpath = $(SDK_INT_DIR)
build_flags = -d
else
sdkpath = $(top)/../../mink/SDK/QTVM/int
build_flags = -o -d
endif

all : $(DL_BINS) $(EMBED_BINS)

$(BIN_DIR) :
	mkdir -p $@

define copy_and_build =
cp -R $(apps_common)/src $(notdir $@)/
cp -R $(top)/idl $(notdir $@)/
python3 $(sdkpath)/Build.py $(build_flags) --i $(notdir $@)/build.json
cp $(notdir $@)/bin/$(notdir $@) $(BIN_DIR)/
endef

$(DL_BINS) : $(wildcard $(apps_common)/src/*) $(wildcard $(top)/idl/*) $(wildcard $(sdkpath)/*) $(sdkpath)/libs/libmodule.lib | $(BIN_DIR)
	cp $(apps_common)/build.json $(notdir $@)/build.json
	$(copy_and_build)

$(EMBED_BINS) : $(wildcard $(apps_common)/src/*) $(wildcard $(top)/idl/*) $(wildcard $(sdkpath)/*) $(sdkpath)/libs/libmodule.lib | $(BIN_DIR)
	cp -f $(apps_common)/build-embedded.json $(notdir $@)/build.json
	$(copy_and_build)

clean : clean_loop
	rm -rf $(BIN_DIR)

clean_loop:
	for dir in $(DOWNLOADABLE) $(EMBEDDED); \
	do                                                    \
		$(MAKE) -C $$dir clean;                           \
		rm -rf $$dir/build.json;                          \
		rm -rf $$dir/idl;                                 \
		rm -rf $$dir/src;                                 \
	done

.PHONY: all clean
