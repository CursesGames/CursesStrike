# DEPENDENCIES
cs: libncurses_util liblinux_util libbcsmap libbcsproto libvector libbcsstatemachine
csds: liblinux_util libbcsmap libbcsproto libbcsstatemachine libvector liblinkedlist
bcsmapconverter: libbcsmap liblinux_util
libncurses_util: liblinux_util
liblinux_util: 
libbcsmap: liblinux_util
libbcsproto: liblinux_util
libbcsstatemachine: liblinux_util libbcsproto libbcsmap libbcsgameplay
libvector: 
