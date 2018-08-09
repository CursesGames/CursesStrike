.PHONY: all config debug release clean distclean

ROOTDIR := $(CURDIR)
include $(ROOTDIR)/src-build/default-config.mk

# whole solution flags
CFLAGS := -ansi -Wextra -pedantic -Wno-unused-parameter
LDFLAGS :=

# native compilation
CFLAGS += -march=native -mtune=native

# -r: save time by omitting `default' targets
# -R: avoid auto-setting of CC, LD and some other variables
# -s: silence is golden, enjoy short compilation messages
ifneq ($(strip $(SILENCE_IS_GOLDEN)),)
MKFLG := rR
else
MKFLG := rRs
endif

# export ALL variables
export

# TARGETS
all: config
	@CONFIG=$(CONFIG) $(MAKE) -$(MKFLG)C $(SRCDIR) projects

clean:
	@$(MAKE) -$(MKFLG)C $(SRCDIR) clean

clean-%:
	@$(MAKE) -$(MKFLG)C $(SRCDIR) $@

distclean:
	@$(MAKE) -$(MKFLG)C $(SRCDIR) distclean

# BUILD CONFIGURATIONS
# To build everything, type `make` or `make release`, which are equal, or `make debug`
# To build one project, for ex. `cs` in debug configuration, type `CONFIG=debug make cs`
# TODO: apply these changes to cross-platform Makefiles
config:
ifeq ($(strip $(CONFIG)),)
CONFIG := debug
endif

ifeq ($(strip $(CONFIG)),release)
CFLAGS += -O3 -D NDEBUG=1 -D RELEASE=1
LDFLAGS += -s
endif

# dirty fix for visual studio
ifeq ($(strip $(CONFIG)),Debug)
CONFIG := debug
endif

ifeq ($(strip $(CONFIG)),debug)
CFLAGS += -O0 -ggdb -ffunction-sections -D DEBUG=1
CFLAGS += -D VALGRIND_SUCKS
endif

debug: CONFIG := debug
debug: all

release: CONFIG := release
release: all

# last resort target, to redirect all lower level targets
# https://www.gnu.org/software/make/manual/html_node/Last-Resort.html
%::
	@CONFIG=$(CONFIG) $(MAKE) -$(MKFLG)C $(SRCDIR) $@
