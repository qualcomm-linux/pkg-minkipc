TARGET = libminkcredentials.a

ifndef ONTARGET
WORKSPACE = ../../../../../..
DESTDIR := out
shipPath = $(WORKSPACE)/security/securemsm
endif

minkPath = $(WORKSPACE)/mink/vendor/qcom/proprietary/mink

SRC = $(wildcard src/*.c)
cflags = -Wall -Werror -fPIC -O2 -Wno-deprecated-declarations -Wno-unused-result

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

ifndef ONTARGET
cflags += -DOFFTARGET
endif

INCLUDES = -I./inc \
           -I$(minkPath)/Platform/QTVM/daemons/mink/inc \
           -I$(minkPath)/SDK/QTVM/inc \
           -I$(minkPath)/Transport/MinkSocket/kernel \
           -I$(minkPath)/Transport/MinkSocket/include \

ifndef ONTARGET
INCLUDES += -I$(shipPath)/mink/inc/interface \
            -I$(shipPath)/smcinvoke/QCBOR/inc
endif

OBJ = $(SRC:c=o)

ME = $(shell basename $(CURDIR))

all $(ME) : $(TARGET)

$(TARGET): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $(cflags) $(INCLUDES) $<

PHONY := $(TARGET)

clean:
	rm -f $(OBJ)
	rm -f $(TARGET)
	rm -rf $(DESTDIR)

install:
ifndef ONTARGET
	mkdir -p $(DESTDIR)$(prefix)/include/minkcredentials/
endif
	install -D -m 755 $(TARGET) $(DESTDIR)$(prefix)/lib/$(TARGET)
	install -D -m 755 inc/MinkTypes.h $(DESTDIR)$(prefix)/include/minkcredentials/MinkTypes.h
	install -D -m 755 inc/osIndCredentials.h $(DESTDIR)$(prefix)/include/minkcredentials/osIndCredentials.h
