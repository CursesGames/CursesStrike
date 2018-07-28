LOCAL_OBJECTS := $(LOCAL_SOURCES:.c=.o)

$(BINDIR)$(LOCAL_MODULE): $(LOCAL_OBJECTS)
	cd $(OBJDIR) && $(LD) -o $@ $^ $(LOCAL_STATIC_DEPENDENCIES) $(LDFLAGS) $(LDFLAGS_LOCAL) $(LOCAL_SHARED_DEPENDENCIES)
	@echo -e "\e[36m LD \e[0m $(shell realpath --relative-to $(ROOTDIR) $@)"

$(LOCAL_OBJECTS): $(LOCAL_HEADERS)
%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_LOCAL) -c -o $(OBJDIR)$@ $<
	@echo -e "\e[33m CC \e[0m $(shell realpath --relative-to $(ROOTDIR) $(OBJDIR)$@)"
