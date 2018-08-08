#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <alloca.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bcsgameplay.h"
#include "../libbcsmap/bcsmap.h"
#include "../liblinux_util/linux_util.h"

void bcsgameplay_map_overlay_create(BCSSERVER_FULL_STATE *state)
{   
    uint16_t height = state->map.height;
    uint16_t width = state->map.width;
    uint16_t client_x;
    uint16_t client_y;
    uint16_t size = height * width;
    uint8_t* map_overlay_copy = state->map_overlay.map_primitives;  // copy ptr to map_overlay

    memcpy(state->map_overlay.map_primitives, state->map.map_primitives, 
            height * width);

    for (int i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
        if (state->client[i].public_info.state == BCSCLST_PLAYING) {
            client_x = state->client[i].public_info.position.x;
            client_y = state->client[i].public_info.position.y;
            map_overlay_copy[width*client_y + client_x] = i;  // размещаем номер "врага"
        }
    }    
}

bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, 
                                                          int steps) 
{
    int line_size = 0;

    uint16_t rifleman = bullet->creator_id;

    uint16_t player_count = state->player_count;  // number of players in server
    uint16_t height = state->map.height;
    uint16_t width = state->map.width;
    uint16_t bullet_x = bullet->x;
    uint16_t bullet_y = bullet->y;
    uint8_t tmp_primitive; 
    uint8_t* map_overlay_copy = state->map_overlay.map_primitives;

    memcpy(map_overlay_copy, state->map_overlay.map_primitives, height * width);

    switch (bullet->direction) {
        case BCSDIR_LEFT:   // y - constant, x can move at negative direction 
            line_size = width * bullet_y;
            for (int i = 0; i < steps; ++i) {
                if (bullet_x == 0) {
                    return false;
                } else {
                    --bullet_x;
                }
                tmp_primitive = map_overlay_copy[line_size + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        if (tmp_primitive == rifleman) {  // don't kill yourself!!!
                            break;
                        }
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
                        lassert(gettimeofday(&(state->client[tmp_primitive]
                                                 .private_info
                                                 .time_last_fire), NULL));
                        state->client[tmp_primitive]
                              .private_info
                              .time_last_fire
                              .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);
                        

                        return false;
                }
            }
            break;
        case BCSDIR_RIGHT:  // y - constant, x can move at positive direction
            line_size = width * bullet_y;
            for (int i = 0; i < steps; ++i) {
                if (bullet_x == width) {
                    return false;
                } else {
                    ++bullet_x;
                }
                if (bullet_x > width) {
                    return false;
                }
                tmp_primitive = map_overlay_copy[line_size + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        if (tmp_primitive == rifleman) {  // don't kill yourself!!!
                            break;
                        }
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
                        lassert(gettimeofday(&(state->client[tmp_primitive]
                                                 .private_info
                                                 .time_last_fire), NULL));
                        state->client[tmp_primitive]
                              .private_info
                              .time_last_fire
                              .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);
                        return false;
                }
            }
            break;
        case BCSDIR_UP:     // x - constant, y can move at negative direction
            for (int i = 0; i < steps; ++i) {
                if (bullet_y == 0) {
                    return false;
                } else {
                    --bullet_y;
                }
                tmp_primitive = map_overlay_copy[width * bullet_y + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        if (tmp_primitive == rifleman) {  // don't kill yourself!!!
                            break;
                        }
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
                        lassert(gettimeofday(&(state->client[tmp_primitive]
                                                 .private_info
                                                 .time_last_fire), NULL));
                        state->client[tmp_primitive]
                              .private_info
                              .time_last_fire
                              .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);
                        return false;
                }
            }
            break;
        case BCSDIR_DOWN:   // x - constant, y can move at positive direction
            for (int i = 0; i < steps; ++i) {
                if (bullet_y == height - 1) {
                    return false;
                } else {
                    ++bullet_y;
                }
                tmp_primitive = map_overlay_copy[width * bullet_y + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        if (tmp_primitive == rifleman) {  // don't kill yourself!!!
                            break;
                        }

                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
                        lassert(gettimeofday(&(state->client[tmp_primitive]
                                                 .private_info
                                                 .time_last_fire), NULL));
                        state->client[tmp_primitive]
                              .private_info
                              .time_last_fire
                              .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);
                        return false;
                }
            }
            break;
        default: 
            break;
    }

    // rewrite x and y coordinates bullet
    bullet->x = bullet_x;
    bullet->y = bullet_y;

    return true;

}
