PG_CONFIG = pg_config
PKG_CONFIG = pkg-config

EXTENSION = pgpcre
MODULE_big = pgpcre
OBJS = pgpcre.o
DATA = pgpcre--0.sql pgpcre--1.sql pgpcre--0--1.sql

ifeq (no,$(shell $(PKG_CONFIG) libpcre2-8 || echo no))
$(warning libpcre2-8 not registed with pkg-config, build might fail)
endif

PG_CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I libpcre2-8)
SHLIB_LINK += $(shell $(PKG_CONFIG) --libs libpcre2-8)

REGRESS = init test unicode
REGRESS_OPTS = --inputdir=test

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
