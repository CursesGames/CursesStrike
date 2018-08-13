#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include "../libbcsproto/bcsproto.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

// some magic const for respawn function
#define CHECK_AREA_SIZE 5  // must be odd

// speed of the bullet: cells per frame
// Илья Коротецкий, [13.08.18 21:40]
// а еще, я бы увеличил скорость пуль
// Илья Коротецкий, [13.08.18 21:40]
// на 4 клеточки
// Илья Коротецкий, [13.08.18 21:40]
// потому что все равно танчики какие-то
#define BCSBULLET_SPEED 4

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

// Create "squashed" layer of the map, containing all the objects on map,
// including active players and bullets.
// This should be done every time in frame once,
// before bullet_step() or respawn() can be used.
extern void bcsgameplay_map_overlay_create(BCSSERVER_FULL_STATE *state);

// Try to move bullet forward to `steps' cells using current overlay.
// Return false if the bullet under pointer should be destroyed
// (e.g. it hit wall, crate, map bounds or another player)
// and true otherwise (if the bullet still in flight).
extern bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, int steps);

// Do a respawn procedure for player in slot `id':
// select any position on the map, attempting to select it safe
// (it means far from other players and not under direct fire)
// and return player to the game by changing player's state to "PLAYING".
// Return false only if the given slot is NOT in "RESPAWNING" state.
extern bool bcsgameplay_respawn(BCSSERVER_FULL_STATE *state, size_t id);
