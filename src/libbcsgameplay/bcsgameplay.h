#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"
#include "../libbcsproto/bcsproto.h"
#include <pthread.h>
#include "../libbcsstatemachine/bcsstatemachine.h"

#define BCSBULLET_SPEED 3

typedef struct __bcs_bullet {
    size_t creator_id;
    int16_t x;
    int16_t y;
    BCSDIRECTION direction;
} BCSBULLET;

bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, 
                                int steps);