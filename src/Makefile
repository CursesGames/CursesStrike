.PHONY: subdirs projects clean distclean last-config

# твою ж дивизию! определение переменных не требует таба!
ROOTDIR := $(realpath $(CURDIR)/../)
include $(ROOTDIR)/src-build/default-config.mk

ifeq ($(strip $(CONFIGURATION)),)
CONFIGURATION := null
endif

# не работает
#vpath %.h .
#vpath %.c .
#vpath %.o $(OBJDIR)
#vpath %.a $(LIBDIR)
#vpath %.exe $(BINDIR)

# работает
VPATH := $(LIBDIR) $(OBJDIR) $(BINDIR)

export

# https://superuser.com/a/593404
SUBDIRS := $(shell find . -mindepth 1 -maxdepth 1 -type d  \( ! -iname ".*" \) | sed 's|^\./||g')
#.PHONY: $(SUBDIRS)

projects: subdirs $(SUBDIRS)
# это позволяет собрать только одну подпапку, например: make libncurses_util
# также здесь прописываем зависимости проектов
include Makefile.projdep.mk

$(SUBDIRS): subdirs
	$(MAKE) -$(MKFLAGS)C $@

last-config:
	echo [i] Compiler is $(CC), linker is $(LD), configuration is $(CONFIGURATION) | tee "$(ROOTDIR)/make.config.now"
	test -f "$(ROOTDIR)/make.config.prev" && diff -N "$(ROOTDIR)/make.config.prev" "$(ROOTDIR)/make.config.now" > /dev/null 2> /dev/null \
	|| ( echo -e "\e[33mConfiguration changed (code $$?), running clean\e[0m" ; $(MAKE) -rR clean )
	cp -f "$(ROOTDIR)/make.config.now" "$(ROOTDIR)/make.config.prev"

subdirs: last-config
	-mkdir -p "$(BINDIR)" 2> /dev/null
	-mkdir -p "$(LIBDIR)" 2> /dev/null
	-mkdir -p "$(OBJDIR)" 2> /dev/null

clean:
	-rm -rf "$(BINDIR)"
	-rm -rf "$(LIBDIR)"
	-rm -rf "$(OBJDIR)"
	-rm -f "$(ROOTDIR)/make.config.prev"
	
distclean:
	@$(foreach dir,$(SUBDIRS),$(MAKE) -$(MKFLAGS)C "$(dir)" clean;)
