# DEFAULT COMPILER
ifeq ($(strip $(CC)),)
CC := gcc
endif

# avoid usage of `ld' itself
LD := $(CC)

# пути до проекта
ifeq ($(MAKELEVEL),0)
BINDIR := $(ROOTDIR)/bin/
LIBDIR := $(ROOTDIR)/lib/
OBJDIR := $(ROOTDIR)/obj/
SRCDIR := $(ROOTDIR)/src/
endif

BUILD_SUBSYSTEM_PATH := $(ROOTDIR)/src-build

BUILD_INITIALIZE     := $(BUILD_SUBSYSTEM_PATH)/initialize.mk

BUILD_STATIC_LIBRARY := $(BUILD_SUBSYSTEM_PATH)/recipe_static.mk
#BUILD_SHARED_LIBRARY := $(BUILD_SUBSYSTEM_PATH)/recipe_shared.mk
BUILD_EXECUTABLE     := $(BUILD_SUBSYSTEM_PATH)/recipe_executable.mk

ifneq ($(strip $(shell realpath --version > /dev/null 2> /dev/null ; echo $$? )),0)
	echo You have no `realpath' utility! Exiting. ; exit 1
endif

export
