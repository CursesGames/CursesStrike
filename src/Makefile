.PHONY: subdirs projects clean distclean last-config

# твою ж дивизию! определение переменных не требует таба!
ROOTDIR := $(realpath $(CURDIR)/..)
include $(ROOTDIR)/src-build/default-config.mk

ifeq ($(strip $(CONFIG)),)
CONFIG := null
endif

export

# https://superuser.com/a/593404
SUBDIRS := $(shell find . -mindepth 1 -maxdepth 1 -type d  \( ! -iname ".*" \) | sed 's|^\./||g')
.PHONY: $(SUBDIRS)

projects: subdirs $(SUBDIRS)
# это позволяет собрать только одну подпапку, например: make libncurses_util
# также здесь прописываем зависимости проектов
# пока что не удалось от них избавиться, но это меньшее из зол
include Makefile.projdep.mk

# if you change MKFLG to MAKEFLAGS, horrible things will behave and -j could not be used!
$(SUBDIRS): subdirs
	$(MAKE) -$(MKFLG)C $@

# warning: bash-specific script...
last-config:
	@(echo [i] Compiler is $(CC), linker is $(LD), configuration is $(CONFIG) && \
	echo [i] CFLAGS are: $(CFLAGS) && \
	echo [i] LDFLAGS are: $(LDFLAGS)) > "$(ROOTDIR)/make.config.now"
	@cat "$(ROOTDIR)/make.config.now"
	@(! test -f "$(ROOTDIR)/make.config.prev") || diff -N "$(ROOTDIR)/make.config.prev" "$(ROOTDIR)/make.config.now" > /dev/null 2> /dev/null \
	|| (tput setaf 3 && echo "Configuration changed (code $$?), running clean" && tput sgr 0 && echo "$(MAKE) -$(MKFLG)" clean )
	@cp -f "$(ROOTDIR)/make.config.now" "$(ROOTDIR)/make.config.prev"

subdirs: last-config
	-mkdir -p "$(BINDIR)"
	-mkdir -p "$(LIBDIR)"
	-mkdir -p "$(OBJDIR)"

clean:
	-rm -rf "$(BINDIR)"
	-rm -rf "$(LIBDIR)"
	-rm -rf "$(OBJDIR)"
	-rm -f "$(ROOTDIR)/make.config.prev"
	
targets := $(addprefix clean-, $(SUBDIRS))

$(targets):
	$(MAKE) -$(MKFLG)C $(subst clean-,,$@) clean

distclean:
	@$(foreach dir,$(SUBDIRS),$(MAKE) -$(MKFLG)C "$(dir)" clean;)
