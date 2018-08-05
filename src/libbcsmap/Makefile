include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=gnu99 -D _GNU_SOURCE

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_STATIC_DEPENDENCIES :=
LOCAL_MODULE := libbcsmap.a

include $(BUILD_STATIC_LIBRARY)
