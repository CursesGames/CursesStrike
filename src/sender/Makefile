include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=gnu99
LDFLAGS_LOCAL :=

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_STATIC_DEPENDENCIES := liblinux_util.a libbcsmap.a libbcsproto.a libvector.a
LOCAL_SHARED_DEPENCIES :=
LOCAL_MODULE := sender.exe

include $(BUILD_EXECUTABLE)
