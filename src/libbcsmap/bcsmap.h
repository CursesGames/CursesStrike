#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>

typedef struct __map_t {
	uint16_t width; // ширина карты - по горизонтали - количество столбцов
	uint16_t height; // высота карты - по вертикали - количество строк
	uint8_t *map_primitives;
} __attribute__((packed)) BCSMAP;
