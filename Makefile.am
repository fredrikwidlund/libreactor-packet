AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4
AM_CFLAGS = -std=gnu11 -g -O3 -flto -fuse-linker-plugin -Isrc
AM_LDFLAGS = -all-static

EXTRA_DIST = \
CHANGES \
LICENSE \
README.rst

SOURCE_FILES = \
src/reactor_packet.c

HEADER_FILES = \
src/reactor_packet.h

lib_LTLIBRARIES = libreactor_packet.la
libreactor_packet_la_SOURCES = $(SOURCE_FILES) $(HEADER_FILES)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libreactor_packet.pc

## examples ###

bin_PROGRAMS = \
example/ebp

example_ebp_LDFLAGS = -lreactor_packet -lreactor -ldynamic -all-static

MAINTAINERCLEANFILES = aclocal.m4 config.h.in configure Makefile.in docs/Makefile.in libreactor_packet-?.?.?.tar.gz
maintainer-clean-local:; rm -rf autotools m4 libreactor_packet-?.?.?

CLEANFILES = {.,src,test}/*.{gcno,gcda,gcov}
