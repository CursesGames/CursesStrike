#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include "../libbcsproto/bcsproto.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

#define BCSBULLET_SPEED 3

bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, 
                                int steps);
