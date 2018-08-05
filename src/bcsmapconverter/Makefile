include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=c99
LDFLAGS_LOCAL :=

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_STATIC_DEPENDENCIES := libbcsmap.a liblinux_util.a
LOCAL_SHARED_DEPENDENCIES :=
LOCAL_MODULE := bcsmapconverter.exe

include $(BUILD_EXECUTABLE)
