#include "bcsproto.h"

void foo() {
	_Static_assert(sizeof(BCSDIRECTION) == sizeof(int), "govno");
}
