#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include "../libbcsproto/bcsproto.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

#define BCSBULLET_SPEED 3
#define CHECK_AREA_SIZE 5  // must be odd
#define RESPAWN_TIMER 3

extern bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, 
                                int steps);

extern bool bcsgameplay_respawn(BCSSERVER_FULL_STATE *state, size_t id);

extern void bcsgameplay_map_overlay_create(BCSSERVER_FULL_STATE *state);
