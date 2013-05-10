PG_CONFIG = pg_config
PKG_CONFIG = pkg-config

extension_version = 0

EXTENSION = pgpcre
MODULE_big = pgpcre
OBJS = pgpcre.o
DATA_built = pgpcre--$(extension_version).sql

EXTRA_CLEAN = $(DATA_built)

ifeq (no,$(shell $(PKG_CONFIG) libpcre || echo no))
$(warning libpcre not registed with pkg-config, build might fail)
endif

PG_CPPFLAGS += $(shell $(PKG_CONFIG) --cflags-only-I libpcre)
SHLIB_LINK += $(shell $(PKG_CONFIG) --libs libpcre)

REGRESS = init test unicode
REGRESS_OPTS = --inputdir=test

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

pgpcre--$(extension_version).sql: pgpcre.sql
	cp $< $@
