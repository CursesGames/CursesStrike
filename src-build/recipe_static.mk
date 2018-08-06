LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)
LOCAL_OBJECTS := $(addprefix $(OBJDIR)/,$(LOCAL_OBJECTS))

LOCAL_STATIC_DEPENDENT_FILES := $(addprefix $(LIBDIR),$(LOCAL_STATIC_DEPENDENCIES))

LOCAL_DESTINATION := $(LIBDIR)/$(LOCAL_MODULE)

# check Makefile itself. If it was changed, rebuild
$(LOCAL_DESTINATION): Makefile
$(LOCAL_DESTINATION): $(LOCAL_OBJECTS) $(LOCAL_STATIC_DEPENDENT_FILES)
	ar rcs $@ $(LOCAL_OBJECTS) $(LOCAL_STATIC_DEPENDENT_FILES)
	@(tput setaf 2 && echo -n " AR " && tput sgr 0 && echo " $(subst $(ROOTDIR)/,,$@)")

$(LOCAL_OBJECTS): $(LOCAL_HEADERS)
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LOCAL) -c -o $@ $<
	@(tput setaf 3 && echo -n " CC " && tput sgr 0 && echo " $(subst $(ROOTDIR)/,,$@)")

clean:
	rm -f "$(LOCAL_DESTINATION)"
	rm -f $(LOCAL_OBJECTS)
