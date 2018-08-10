help:
	@echo "Batch targets:"
	@echo "all - build all projects, except for tests"
	@echo "projects - alias for 'all'"
	@echo "tests - build test projects"
	@echo
	@echo "Build configurations:"
	@echo "debug - all + CONFIG=debug (this is the default)"
	@echo "release - all + CONFIG=release. Result binaries are stripped (no debug info)"
	@echo "wild - all + CONFIG=wild. Result binaries are stripped even more than release. No source code exposed in macro"
	@echo
	@echo "Active projects, can be built in chosen configuration using CONFIG=<debug,release,wild> make [-f Makefile.*arch*] <project>:"
	@cd $(SRCDIR) && (find . -mindepth 1 -maxdepth 1 -type d ! -iname ".*" | sed 's|^./||g' 2> /dev/null) | sort
