.PHONY: clean

include $(BUILD_INITIALIZE)

CFLAGS_LOCAL := -std=c99

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_DEPENDENCIES := 
LOCAL_MODULE := liblinux_util.a

include $(BUILD_STATIC_LIBRARY)

clean:
	rm -f $(LIBDIR)/$(LOCAL_MODULE)
