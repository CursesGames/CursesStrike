#ifndef __BCSMAP_H_
#define __BCSMAP_H_ 

#include <stdint.h>
#include <stdbool.h>

//количество каналов в rgb
#define RGB_NUM 3

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t  LONG;

//header of bmp file
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

//color channels
typedef struct {
    BYTE  rgbtBlue;
    BYTE  rgbtGreen;
    BYTE  rgbtRed;
} __attribute__((__packed__)) rgb_triple_t;

//map structure
typedef struct __map_t {
    uint16_t width;                 // horisontal size - count of columns
    uint16_t height;                // vertical size - count of lines
    uint8_t *map_primitives;        // 1D array of primitives width*height
} __attribute__((packed)) BCSMAP;

//colors of items on the map
enum {
    COLOR_ROCK = 0x000000,       // black color
    COLOR_WATER = 0xFF,          // blue color
    COLOR_BOX = 0x804000,        // brown color
    COLOR_OPEN_SPACE = 0xFFFFFF  // white color
};

//list of map primitives
typedef enum {
    PUNIT_ROCK = 'r',       // black color
    PUNIT_WATER = 'w',      // blue color
    PUNIT_BOX = 'b',        // brown color
    PUNIT_OPEN_SPACE = 'o'  // white color
} __attribute__((packed)) BCSMAPPRIMITIVE;

// загрузить из bmp, результат конвертации в памяти
//load from bmp
bool bcsmap_get_from_bmp(const char *filename, BCSMAP *map); 

// загрузить из bcsmap в память
//load from bcsmap to memory
bool bcsmap_load(const char *filename, BCSMAP *map); 

// сохранить из памяти в bcsmap
//save to bcsmap from memory
bool bcsmap_save(const char *filename, BCSMAP *map); 

#endif
