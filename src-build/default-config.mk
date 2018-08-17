# DEFAULT COMPILER
ifeq ($(strip $(CC)),)
CC := gcc
endif

# avoid usage of `ld' itself
LD := $(CC)

# -r: save time by omitting `default' targets
# -R: avoid auto-setting of CC, LD and some other variables
# -s: silence is golden, enjoy short compilation messages
ifneq ($(strip $(SILENCE_IS_GOLDEN)),)
MKFLG := rR
else
MKFLG := rRs
endif

# пути до проекта
ifeq ($(MAKELEVEL),0)
BINDIR := $(ROOTDIR)/bin
LIBDIR := $(ROOTDIR)/lib
OBJDIR := $(ROOTDIR)/obj
SRCDIR := $(ROOTDIR)/src
#DEPDIR := $(ROOTDIR)/src-deps
DEPDIR := $(ROOTDIR)/src-build/arch/x64
endif

BUILD_SUBSYSTEM_PATH := $(ROOTDIR)/src-build

BUILD_INITIALIZE     := $(BUILD_SUBSYSTEM_PATH)/initialize.mk

BUILD_STATIC_LIBRARY := $(BUILD_SUBSYSTEM_PATH)/recipe_static.mk
#BUILD_SHARED_LIBRARY := $(BUILD_SUBSYSTEM_PATH)/recipe_shared.mk
BUILD_EXECUTABLE     := $(BUILD_SUBSYSTEM_PATH)/recipe_executable.mk

#include $(BUILD_SUBSYSTEM_PATH)/help.mk
