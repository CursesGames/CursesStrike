#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include "../libbcsproto/bcsproto.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

// some magic const for respawn function
#define CHECK_AREA_SIZE 5  // must be odd

// speed of the bullet: cells per frame
#define BCSBULLET_SPEED 3

///////////////////////////////////
// CONFIGURABLE SETTINGS
///////////////////////////////////
// seconds before respawn after death
#define RESPAWN_TIMER 2

// seconds before kick if no datagram received from client
// allow 30 sec of connection interruption
#define BCSGP_KICK_TIMEOUT 30

// seconds before moving to spectators for inactivity
// allow 60 sec of AFK
#define BCSGP_SPEC_TIMEOUT 60

// if moving to spectators is faster, bad things would happen
STATIC_ASSERT(BCSGP_KICK_TIMEOUT < BCSGP_SPEC_TIMEOUT);

// firing speed: one shot in these microseconds
// default is no more than 3 bullets per second.
#define BCSGP_FIRE_CALMDOWN 333333

// moving speed: one move in these microseconds.
// default is no more than 30 cells per second.
#define BCSGP_MOVE_TIMEBOUND 33333

extern bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, int steps);

extern bool bcsgameplay_respawn(BCSSERVER_FULL_STATE *state, size_t id);

extern void bcsgameplay_map_overlay_create(BCSSERVER_FULL_STATE *state);
