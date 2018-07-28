.PHONY: all debug release clean distclean

ROOTDIR := $(CURDIR)
include $(ROOTDIR)/src-build/default-config.mk

# whole solution flags
CFLAGS := -Wno-unused-parameter -ansi
LDFLAGS :=

# -r: save time by omitting `default' targets
# -R: avoid auto-setting of CC, LD and some other variables
# -s: silence is golden, enjoy short compilation messages
MKFLG := rRs

# export ALL variables
export

CFLAGS := -Wextra -pedantic -O3 -g -march=native -mtune=native -D NDEBUG=1 -D RELEASE=1 $(CFLAGS)
override CONFIGURATION := release

# BUILD CONFIGURATIONS
release: all

debug: CFLAGS := -Wextra -pedantic -O0 -ggdb -ffunction-sections -D DEBUG=1 $(CFLAGS)
debug: CFLAGS += -D VALGRIND_SUCKS
debug: override CONFIGURATION := debug
debug: all

# TARGETS
all:
	$(MAKE) -$(MKFLG)C $(SRCDIR) projects

clean:
	$(MAKE) -$(MKFLG)C $(SRCDIR) clean

distclean:
	$(MAKE) -$(MKFLG)C $(SRCDIR) distclean

# last resort target, to redirect all lower level targets
# https://www.gnu.org/software/make/manual/html_node/Last-Resort.html
%::
	$(MAKE) -$(MKFLG)C $(SRCDIR) $@
