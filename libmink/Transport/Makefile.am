# Makefile.am - Automake script for Mink Transport

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =

if TARGET_ENABLE_MINKSOCKET
SUBDIRS += MinkSocket
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mink-transport.pc
EXTRA_DIST = $(pkgconfig_DATA)
