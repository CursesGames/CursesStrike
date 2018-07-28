ifeq ($(MAKELEVEL),0)
CC := gcc
LD := $(CC)
endif

ifeq ($(strip $(OBJDIR)),)
OBJDIR := ./
endif

CFLAGS_LOCAL :=
LDFLAGS_LOCAL :=

LOCAL_HEADERS :=
LOCAL_SOURCES :=
LOCAL_DEPENDENCIES :=
LOCAL_MODULE :=
