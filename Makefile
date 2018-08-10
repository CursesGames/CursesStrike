.PHONY: all config debug release wild help

ROOTDIR := $(CURDIR)
include $(ROOTDIR)/src-build/default-config.mk

# whole solution flags
CFLAGS := -ansi -Wextra -pedantic -Wno-unused-parameter
LDFLAGS :=

# native compilation
CFLAGS += -march=native -mtune=native

# export ALL variables
export

# TARGETS
all: config
	@CONFIG=$(CONFIG) $(MAKE) -$(MKFLG)C $(SRCDIR) projects

# BUILD CONFIGURATIONS
# To build everything, type `make` or `make debug`, which are equal, or `make release`, or `make wild`
# To build one project, for ex. `cs` in wild configuration, type `CONFIG=wild make cs`
config:
ifeq ($(strip $(CONFIG)),release)
CFLAGS += -O3 -D NDEBUG -D RELEASE
LDFLAGS += -s

else ifeq ($(strip $(CONFIG)),wild)
CFLAGS += -O3 -D NDEBUG -D RELEASE -D WILDRELEASE
LDFLAGS += -s

else
CONFIG := debug
CFLAGS += -O0 -ggdb -ffunction-sections -D DEBUG
CFLAGS += -D VALGRIND_SUCKS
endif

include $(ROOTDIR)/src-build/help.mk

# last resort target, to redirect all lower level targets
# https://www.gnu.org/software/make/manual/html_node/Last-Resort.html
%::
	@CONFIG=$(CONFIG) $(MAKE) -$(MKFLG)C $(SRCDIR) $@
