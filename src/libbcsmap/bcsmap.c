#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <endian.h>
#include <errno.h>

#include "bcsmap.h"
#include "../liblinux_util/linux_util.h"

bool bcsmap_get_from_bmp(const char *filename, BCSMAP *map) 
{
    bmp_file_header_t bmp_header;
    bmp_file_header_t* temp_iterator_header;
    bmp_info_header_t bmp_info;
    bmp_info_header_t* temp_iterator_info;    
    
    rgb_triple_t* raster_data = NULL;
    BYTE* primitives_arr = NULL;
    char* temp_raster_ptr = NULL;

    int source_bmp_fd = 0;                      // fd for source .bmp file
    int temp_RGB_unit = 0;
    int padding = 0;
    
    ssize_t now_read = 0;

    size_t bmp_header_size = sizeof(bmp_header);
    size_t bmp_info_size = sizeof(bmp_info);
    size_t useful_raster_size = 0;
    size_t useful_width = 0;
    size_t useful_height = 0;
    size_t primitives_arr_size = 0;

    memset(&bmp_header, 0, sizeof(bmp_header));
    memset(&bmp_info, 0, sizeof(bmp_info));

    __syscall(source_bmp_fd = open(filename, O_RDONLY));

    // reading bmp file header
    // If reading process has been interruped of some signal
    // read() return -1 and install errno = EINTR, what mean 
    // what need rereading without changes. If now_read
    // less than bmp_header_size then need to read the missing 
    // bytes and add them to the buffer (bmp_headr in our case)
    // This situation could occur if the signal interrupted the 
    // reading process
    temp_iterator_header = &bmp_header;
    while (bmp_header_size != 0 && 
        (now_read = read(source_bmp_fd, temp_iterator_header, bmp_header_size)) != 0) {
        if (now_read == -1) {
            if (errno == EINTR) {
                continue;
            }
            __syscall(now_read);
        }

        bmp_header_size -= now_read;
        temp_iterator_header += now_read;
    }

    // check BMP format
    if (bmp_header.bfType != 0x4D42) {
        ALOGE("Unsupported file format\n");
        __syscall(close(source_bmp_fd));
        exit(EXIT_FAILURE);
    }

    // reading bmp information header
    temp_iterator_info = &bmp_info;
    now_read = 0;
    while (bmp_info_size != 0 && 
        (now_read = read(source_bmp_fd, temp_iterator_info, bmp_info_size)) != 0) {
        if (now_read == -1) {
            if (errno == EINTR) {
                continue;
            }
            __syscall(now_read);
        }

        bmp_info_size -= now_read;
        temp_iterator_header += now_read;
    }

    // jumping to raster data;
    __syscall(lseek(source_bmp_fd, bmp_header.bfOffBits, SEEK_SET));

    // calculate padding of bmp file
    padding = (4 - (RGB_NUM*bmp_info.biWidth % 4)) % 4;

    // calculate some information about raster from bmp
    useful_height = bmp_info.biHeight;
    useful_width = bmp_info.biWidth * RGB_NUM;
    useful_raster_size = useful_height*useful_width;

    // formation a storage for raster information from bmp
    raster_data = malloc(useful_raster_size);

    // ever unit in primitive array is "pixel" 
    primitives_arr_size = bmp_info.biHeight * bmp_info.biWidth;  // for itaration at array

    // formation array for storage primitives
    primitives_arr = malloc(sizeof(temp_RGB_unit) * primitives_arr_size);
    // note
    // for storage a colors usage integer. 
    // In ever byte keeping RED, GREEN and BLUE colors.
    // High byte is zero

    // create pointer for indexing in raster_data
    // Becouse BMP format stores raster data 
    // reflected horizontally, it is necessary to select 
    // data in right order. To do this, set the pointer
    // to the beginning of last line and read raster data
    // from the end
    temp_raster_ptr = (char*) raster_data + (useful_raster_size - useful_width);

    for (int i = 0; i <= bmp_info.biHeight; ++i) {
        temp_raster_ptr -= read(source_bmp_fd, temp_raster_ptr, useful_width);
        lseek(source_bmp_fd, padding, SEEK_CUR);  // jump next line
    }

    for (size_t i = 0; i < primitives_arr_size; ++i) {
        temp_RGB_unit = raster_data[i].rgbtRed << 16;
        temp_RGB_unit += raster_data[i].rgbtGreen << 8;
        temp_RGB_unit += raster_data[i].rgbtBlue;

        switch (temp_RGB_unit) {
            case COLOR_OPEN_SPACE:
                primitives_arr[i] = PUNIT_OPEN_SPACE;
                break;
            case COLOR_WATER:
                primitives_arr[i] = (BYTE)PUNIT_WATER;
                break;
            case COLOR_BOX:
                primitives_arr[i] = (BYTE)PUNIT_BOX;
                break;
            case COLOR_ROCK:
                primitives_arr[i] = (BYTE)PUNIT_ROCK;
                break;
            default:
                primitives_arr[i] = PUNIT_OPEN_SPACE;
                break;
        }
        temp_RGB_unit = 0;
    }

    map->height = bmp_info.biHeight;
    map->width = bmp_info.biWidth;
    map->map_primitives = primitives_arr;

    free(raster_data);  // freeing memory after getting primitives

    VERBOSE {
        ALOGI("BCSMAP success located in memory\n");
    }
    return true;
}

bool bcsmap_save(const char *filename, BCSMAP *map)
{
    int destination_fd = 0;
    ssize_t now_write = 0;
    ssize_t left_bytes = map->height*map->width;

    uint8_t* iterator = NULL;
    char bcsmap_id[] = "csm";

    // translation into BE
    map->height = htobe16(map->height);
    map->width = htobe16(map->width);

    __syscall(destination_fd = creat(filename, 0644));

    __syscall(write(destination_fd, bcsmap_id, sizeof(bcsmap_id)));

    __syscall(write(destination_fd, map, sizeof(map->height)*2));

    // writing primitives on chunks
    iterator = map->map_primitives;
    while (true) {
        __syscall(now_write = write(destination_fd, iterator, 
                    min(left_bytes,_BUF_SIZE)));
        if (now_write == left_bytes) {
            break;
        }
        left_bytes -= now_write;
        iterator += now_write;
    }

    __syscall(close(destination_fd));

    // translation into LE
    map->height = htobe16(map->height);
    map->width = htobe16(map->width);

    VERBOSE {
        ALOGI("BCSMAP successfully saved in local disk\n");
    }
    return true;
}

bool bcsmap_load(const char *filename, BCSMAP *map) 
{
    BYTE* buf = malloc(_BUF_SIZE);    
    char bcsmap_check[4];

    BYTE* primitives_arr = NULL;
    BYTE* temp_primitive = NULL;
    
    int bcsmap_fd = 0;

    ssize_t now_read = 0;
    
    size_t map_size = 0;
    
    if((bcsmap_fd = open(filename, O_RDONLY)) == -1)
	    return false;

    // reading contorl string
    __syscall(read(bcsmap_fd, bcsmap_check, sizeof(bcsmap_check)));

    if (strcmp(bcsmap_check, "csm") != 0) {
        ALOGE("This format is not a map format");
        exit(EXIT_FAILURE);
    }

    // reading height and width of map
    __syscall(read(bcsmap_fd, map, sizeof(map->height)*2));

    // accounting for the possibility BE
    map->height = be16toh(map->height);
    map->width = be16toh(map->width);

    map_size = (map->height) * (map->width);
    primitives_arr = malloc(map_size);
    temp_primitive = primitives_arr;


    // reading primitives from map
    while ((now_read = read(bcsmap_fd, buf, _BUF_SIZE)) != 0) {
        if (now_read == -1) {
            if (errno == EINTR) {
                continue;
            }
            __syscall(now_read);
        }

        temp_primitive = mempcpy(temp_primitive, buf, now_read);
    }

    map->map_primitives = primitives_arr;

    VERBOSE {
        ALOGI("Map loading finished success\n");
    }

    free(buf);

    return true;
}
