include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=gnu99
LDFLAGS_LOCAL :=

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_STATIC_DEPENDENCIES := liblinux_util.a libbcsmap.a libbcsproto.a
LOCAL_SHARED_DEPENDENCIES := pthread
LOCAL_MODULE := csds.exe

include $(BUILD_EXECUTABLE)
