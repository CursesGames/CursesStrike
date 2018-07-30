#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>

typedef struct __map_t {
	uint16_t width; // ������ ����� - �� ����������� - ���������� ��������
	uint16_t height; // ������ ����� - �� ��������� - ���������� �����
	uint8_t *map_primitives;
} BCSMAP;
