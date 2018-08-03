#ifndef __BCSMAP_H_
#define __BCSMAP_H_ 

#include <stdint.h>
#include <stdbool.h>

#define RGB_NUM 3

typedef uint8_t  BYTE;
typedef uint32_t DWORD;
typedef int32_t  LONG;
typedef uint16_t WORD;

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

typedef struct {
    BYTE  rgbtBlue;
    BYTE  rgbtGreen;
    BYTE  rgbtRed;
} __attribute__((__packed__)) rgb_triple_t;

typedef struct __map_t {
    uint16_t width;                 // ширина карты - по горизонтали - количество столбцов
    uint16_t height;                // высота карты - по вертикали - количество строк
    uint8_t *map_primitives;
} __attribute__((packed)) BCSMAP;

enum {
    COLOR_ROCK = 0x000000,       // black color
    COLOR_WATER = 0xFF,          // blue color
    COLOR_BOX = 0x804000,        // brown color
    COLOR_OPEN_SPACE = 0xFFFFFF  // white color
};

enum {
    PUNIT_ROCK = 'r',       // black color
    PUNIT_WATER = 'w',      // blue color
    PUNIT_BOX = 'b',        // brown color
    PUNIT_OPEN_SPACE = 'o'  // white color
};

// загрузить из bmp, результат конвертации в памяти
bool bcsmap_get_from_bmp(const char *filename, BCSMAP *map); 

// загрузить из bcsmap в память
bool bcsmap_load(const char *filename, BCSMAP *map); 

// сохранить из памяти в bcsmap
bool bcsmap_save(const char *filename, BCSMAP *map); 

#endif
