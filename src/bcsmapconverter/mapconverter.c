// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"

int main(int argc, char **argv) {
    if(argc < 2) {
        ALOGE("Specify a .bmp file to convert from\n");
        exit(EXIT_FAILURE);
    }
    
    BCSMAP map;
    if(!bcsmap_get_from_bmp(argv[1], &map)) {
        ALOGE("Could not convert '%s' to BCSMAP\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    
    char fname[NAME_MAX];
    sprintf(fname, "%s.bcsmap", argv[1]);
    if(!bcsmap_save(fname, &map)) {
        ALOGE("Could not save BCSMAP to '%s'\n", fname);
        exit(EXIT_FAILURE);
    }
    
    ALOGI("Converted successfully!\n");
    return EXIT_SUCCESS;
}
