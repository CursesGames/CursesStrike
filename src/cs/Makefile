.PHONY: clean

CFLAGS_LOCAL := -std=gnu99
LDFLAGS_LOCAL :=

LOCAL_HEADERS := $(wildcard *.h)
LOCAL_SOURCES := $(wildcard *.c)
LOCAL_STATIC_DEPENDENCIES := $(LIBDIR)liblinux_util.a $(LIBDIR)libncurses_util.a $(LIBDIR)libbcsmap.a $(LIBDIR)libbcsproto.a
LOCAL_SHARED_DEPENDENCIES := -lncursesw -lpthread -lrt
LOCAL_MODULE := cs.exe

include $(BUILD_EXECUTABLE)

clean:
	rm -f $(BINDIR)/$(LOCAL_MODULE)
