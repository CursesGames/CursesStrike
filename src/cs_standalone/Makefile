include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=gnu99
LDFLAGS_LOCAL := $(DEPDIR)/ncurses/lib/libncursesw.a

LOCAL_OBJECTS := cs.o
LOCAL_STATIC_DEPENDENCIES := liblinux_util.a libncurses_util.a libbcsmap.a libbcsproto.a libvector.a
LOCAL_SHARED_DEPENDENCIES := pthread rt

ifneq ($(strip $(NCURSES_FROM_SOURCE)),1)
# link statically with libtinfo
ifeq ($(strip $(CROSS_COMPILE)),)
LDFLAGS_LOCAL += $(DEPDIR)/ncurses/lib/libtinfo.a
LOCAL_SHARED_DEPENDENCIES += dl
endif

endif

LOCAL_MODULE := cs_standalone.exe

include $(BUILD_EXECUTABLE)
