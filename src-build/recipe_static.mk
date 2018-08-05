LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)
LOCAL_OBJECTS := $(addprefix $(OBJDIR)/,$(LOCAL_OBJECTS))

LOCAL_STATIC_DEPENDENT_FILES := $(addprefix $(LIBDIR),$(LOCAL_STATIC_DEPENDENCIES))

LOCAL_DESTINATION := $(LIBDIR)/$(LOCAL_MODULE)

# check Makefile itself. If it was changed, rebuild
$(LOCAL_DESTINATION): Makefile
$(LOCAL_DESTINATION): $(LOCAL_OBJECTS) $(LOCAL_STATIC_DEPENDENT_FILES)
	ar rcs $@ $(LOCAL_OBJECTS) $(LOCAL_STATIC_DEPENDENT_FILES)
	@echo -e "\e[32m AR \e[0m $(subst $(ROOTDIR)/,,$@)"

$(LOCAL_OBJECTS): $(LOCAL_HEADERS)
$(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LOCAL) -c -o $@ $<
	@echo -e "\e[33m CC \e[0m $(subst $(ROOTDIR)/,,$@)"

clean:
	rm -f "$(LOCAL_DESTINATION)"
	rm -f $(LOCAL_OBJECTS)
