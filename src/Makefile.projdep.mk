# DEPENDENCIES
cs: libncurses_util liblinux_util libbcsmap libbcsproto
csds: liblinux_util libbcsmap
libncurses_util: liblinux_util
liblinux_util: 
libbcsmap: liblinux_util
libbcsproto: liblinux_util
