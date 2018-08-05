#ifndef __BCSMAP_H_
#define __BCSMAP_H_ 

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>
#include <stdbool.h>

// количество каналов в RGB
#define RGB_NUM 3

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define _BUF_SIZE 4096

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t  LONG;

// header of bmp file
typedef struct {
    WORD  bfType;
    DWORD bfSize;
    WORD  bfReserved1;
    WORD  bfReserved2;
    DWORD bfOffBits;
} __attribute__((__packed__)) bmp_file_header_t;

typedef struct {
    DWORD  biSize; 
    LONG   biWidth; 
    LONG   biHeight; 
    WORD   biPlanes; 
    WORD   biBitCount; 
    DWORD  biCompression; 
    DWORD  biSizeImage; 
    LONG   biXPelsPerMeter; 
    LONG   biYPelsPerMeter; 
    DWORD  biClrUsed; 
    DWORD  biClrImportant; 
} __attribute__((__packed__)) bmp_info_header_t;

// color channels
typedef struct {
    BYTE  rgbtBlue;
    BYTE  rgbtGreen;
    BYTE  rgbtRed;
} __attribute__((__packed__)) rgb_triple_t;

// Bionicle Counter-Strike Map
typedef struct __map_t {
    // horizontal size - count of columns
    // ширина карты - по горизонтали - количество столбцов
    uint16_t width;
    // vertical size - count of lines
    // высота карты - по вертикали - количество строк
    uint16_t height;
    // pointer to single-dimension array of 8-bit primitives
    uint8_t *map_primitives;
} BCSMAP;

// source (.bmp) colors of item on the map
enum {
    COLOR_ROCK = 0x000000,       // black color
    COLOR_WATER = 0xFF,          // blue color
    COLOR_BOX = 0x804000,        // brown color
    COLOR_OPEN_SPACE = 0xFFFFFF  // white color
};

// list of map primitives
typedef enum {
    PUNIT_ROCK = 'r',       // black color
    PUNIT_WATER = 'w',      // blue color
    PUNIT_BOX = 'b',        // brown color
    PUNIT_OPEN_SPACE = 'o'  // white color
} __attribute__((packed)) BCSMAPPRIMITIVE;

// Сonverts a file <filename> of the bmp format to a set of primitives for 
// further work. The function allocates memory for storing an array of 
// primitives (map->map_primitives), which the caller must clear after use.
// The function supports work only on machines with the LE order of bytes.
bool bcsmap_get_from_bmp(const char *filename, BCSMAP *map); 

// Convert a map file to a structure in the program memory for further work.
// The function allocates memory for storing an array of primitives 
// (map->map_primitives), which the caller must clear after use.
// Can useable in any bytes order (BE or LE)
bool bcsmap_load(const char *filename, BCSMAP *map); 

// Saving a map to .bcsmap format on local disk
// Can useable in any bytes order (BE or LE)
bool bcsmap_save(const char *filename, BCSMAP *map); 

#endif
