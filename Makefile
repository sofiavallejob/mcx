# Makefile for mcx  (pd-lib-builder: https://github.com/pure-data/pd-lib-builder)
lib.name = mcx

class.sources = \
	src/mcexpr~.c \
	src/mcsum~.c \
	src/mcnchs~.c \
	src/mcprint~.c

datafiles = \
	$(wildcard help/*.pd) \
	mcx-meta.pd \
	README.md \
	LICENSE

cflags = -std=c99 -O3

PDLIBBUILDER_DIR = .
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder
