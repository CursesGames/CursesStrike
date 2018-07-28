#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stddef.h>
#include <stdint.h>
//typedef unsigned char byte;

typedef struct __map_t {
	size_t width; // ������ ����� - �� ����������� - ���������� ��������
	size_t height; // ������ ����� - �� ��������� - ���������� �����
	uint8_t *map_primitives;
} BCSMAP;
