include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=gnu99

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_STATIC_DEPENDENCIES := 
LOCAL_SHARED_DEPENDENCIES := pthread
LOCAL_MODULE := libbcsstatemachine.a

include $(BUILD_STATIC_LIBRARY)
