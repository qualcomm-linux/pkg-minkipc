TOPTARGETS := all clean

TVMD_DIR = daemons

DAEMONS = root prelauncher mink

SUBDIRS = $(addprefix $(TVMD_DIR)/, $(DAEMONS))

$(TOPTARGETS) : $(SUBDIRS)
$(SUBDIRS) :
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)

# Remove all socket type files
clean: $(SUBDIRS)
	find -type s -exec rm {} \;

install:
