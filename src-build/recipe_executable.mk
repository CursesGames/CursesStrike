# allowing to override this var from target Makefile
ifeq ($(LOCAL_OBJECTS),)
LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)
endif

LOCAL_OBJECTS := $(addprefix $(OBJDIR)/,$(LOCAL_OBJECTS))

LOCAL_STATIC_DEPENDENT_FILES := $(addprefix $(LIBDIR)/,$(LOCAL_STATIC_DEPENDENCIES))
LOCAL_SHARED_DEPENDENT_LIBS := $(addprefix -l,$(LOCAL_SHARED_DEPENDENCIES))

LOCAL_DESTINATION := $(BINDIR)/$(LOCAL_MODULE)

# check Makefile itself. If it was changed, rebuild
$(LOCAL_DESTINATION): Makefile
$(LOCAL_DESTINATION): $(LOCAL_OBJECTS) $(LOCAL_STATIC_DEPENDENT_FILES)
	$(LD) -L$(LIBDIR) -o $@ $(LOCAL_OBJECTS) $(LOCAL_STATIC_DEPENDENT_FILES) $(LDFLAGS) $(LDFLAGS_LOCAL) $(LOCAL_SHARED_DEPENDENT_LIBS)
	@(tput setaf 6 && echo -n " LD " && tput sgr 0 && echo " $(subst $(ROOTDIR)/,,$@)")

$(LOCAL_OBJECTS): $(LOCAL_HEADERS)
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LOCAL) -c -o $@ $<
	@(tput setaf 3 && echo -n " CC " && tput sgr 0 && echo " $(subst $(ROOTDIR)/,,$@)")

clean:
	rm -f "$(LOCAL_DESTINATION)"
	rm -f $(LOCAL_OBJECTS)
