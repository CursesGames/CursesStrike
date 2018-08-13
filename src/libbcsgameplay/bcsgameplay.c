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

void bcsgameplay_map_overlay_create(BCSSERVER_FULL_STATE *state) {
    uint16_t height = state->map.height;
    uint16_t width = state->map.width;
    uint16_t client_x;
    uint16_t client_y;
    uint16_t size = height * width;
    uint8_t *map_overlay_copy = state->map_overlay.map_primitives; // copy ptr to map_overlay

    memcpy(state->map_overlay.map_primitives, state->map.map_primitives,
           height * width);

    for (int i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
        if (state->client[i].public_info.state == BCSCLST_PLAYING) {
            client_x = state->client[i].public_info.position.x;
            client_y = state->client[i].public_info.position.y;
            map_overlay_copy[width * client_y + client_x] = i; // marking enemy
        }
    }
}

bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet,
                             int steps) {
    int line_size = 0;

    uint16_t rifleman = bullet->creator_id;

    uint16_t player_count = state->player_count; // number of players in server
    uint16_t height = state->map.height;
    uint16_t width = state->map.width;
    uint16_t bullet_x = bullet->x;
    uint16_t bullet_y = bullet->y;
    uint8_t tmp_primitive;
    uint8_t *map_overlay_copy = state->map_overlay.map_primitives;

    switch (bullet->direction) {
        case BCSDIR_LEFT: {
            // y - constant, x can move at negative direction 
            line_size = width * bullet_y;
            for (int i = 0; i < steps; ++i) {
                if (bullet_x == 0) {
                    return false;
                }
                else {
                    --bullet_x;
                }
                tmp_primitive = map_overlay_copy[line_size + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER: {
                        break;
                    }
                    case PUNIT_ROCK:
                    case PUNIT_BOX: {
                        return false;
                    }
                    default: {
                        if (tmp_primitive == rifleman) {
                            // don't kill yourself!!!
                            break;
                        }
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state
                            = BCSCLST_RESPAWNING;
                        if (gettimeofday(&state->client[tmp_primitive]
                                         .private_info
                                         .time_last_fire, NULL) == -1) {
                            perror("gettimeofday in LEFT");
                        }
                        state->client[tmp_primitive]
                           .private_info
                           .time_last_fire
                           .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);

                        // копипаст-программирование - говно!
                        ALOGD("KILL event at (%hu, %hu), hunter: %hu '%s', victim: %hhu '%s'\n"
		                    , bullet_x, bullet_y
		                    , rifleman, state->client[rifleman].public_ext_info.nickname
		                    , tmp_primitive, state->client[tmp_primitive].public_ext_info.nickname
		                );
                        lassert(state->client[tmp_primitive].public_info.position.x == bullet_x);
                        lassert(state->client[tmp_primitive].public_info.position.y == bullet_y);

                        return false;
                        break;
                    }
                }
            }
            break;
        }
        case BCSDIR_RIGHT: {
            // y - constant, x can move at positive direction
            line_size = width * bullet_y;
            for (int i = 0; i < steps; ++i) {
                if (bullet_x == width) {
                    return false;
                }
                else {
                    ++bullet_x;
                }
                if (bullet_x > width) {
                    return false;
                }
                tmp_primitive = map_overlay_copy[line_size + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER: {
                        break;
                    }
                    case PUNIT_ROCK:
                    case PUNIT_BOX: {
                        return false;
                    }
                    default: {
                        if (tmp_primitive == rifleman) {
                            // don't kill yourself!!!
                            break;
                        }
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state
                            = BCSCLST_RESPAWNING;
                        if (gettimeofday(&state->client[tmp_primitive]
                                         .private_info
                                         .time_last_fire, NULL) == -1) {
                            perror("gettimeofday in RIGHT");
                        }
                        state->client[tmp_primitive]
                           .private_info
                           .time_last_fire
                           .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);

                        // копипаст-программирование - говно!
                        ALOGD("KILL event at (%hu, %hu), hunter: %hu '%s', victim: %hhu '%s'\n"
		                    , bullet_x, bullet_y
		                    , rifleman, state->client[rifleman].public_ext_info.nickname
		                    , tmp_primitive, state->client[tmp_primitive].public_ext_info.nickname
		                );
                        lassert(state->client[tmp_primitive].public_info.position.x == bullet_x);
                        lassert(state->client[tmp_primitive].public_info.position.y == bullet_y);

                        return false;
                        break;
                    }
                }
            }
            break;
        }
        case BCSDIR_UP: {
            // x - constant, y can move at negative direction
            for (int i = 0; i < steps; ++i) {
                if (bullet_y == 0) {
                    return false;
                }
                else {
                    --bullet_y;
                }
                tmp_primitive = map_overlay_copy[width * bullet_y + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER: {
                        break;
                    }
                    case PUNIT_ROCK:
                    case PUNIT_BOX: {
                        return false;
                    }
                    default: {
                        if (tmp_primitive == rifleman) {
                            // don't kill yourself!!!
                            break;
                        }
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state
                            = BCSCLST_RESPAWNING;
                        if (gettimeofday(&state->client[tmp_primitive]
                                         .private_info
                                         .time_last_fire, NULL) == -1) {
                            perror("gettimeofday in RIGHT");
                        }
                        state->client[tmp_primitive]
                           .private_info
                           .time_last_fire
                           .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);

                        // копипаст-программирование - говно!
                        ALOGD("KILL event at (%hu, %hu), hunter: %hu '%s', victim: %hhu '%s'\n"
		                    , bullet_x, bullet_y
		                    , rifleman, state->client[rifleman].public_ext_info.nickname
		                    , tmp_primitive, state->client[tmp_primitive].public_ext_info.nickname
		                );
                        lassert(state->client[tmp_primitive].public_info.position.x == bullet_x);
                        lassert(state->client[tmp_primitive].public_info.position.y == bullet_y);

                        return false;
                        break;
                    }
                }
            }
            break;
        }
        case BCSDIR_DOWN: {
            // x - constant, y can move at positive direction
            for (int i = 0; i < steps; ++i) {
                if (bullet_y == height - 1) {
                    return false;
                }
                else {
                    ++bullet_y;
                }
                tmp_primitive = map_overlay_copy[width * bullet_y + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER: {
                        break;
                    }
                    case PUNIT_ROCK:
                    case PUNIT_BOX: {
                        return false;
                    }
                    default: {
                        if (tmp_primitive == rifleman) {
                            // don't kill yourself!!!
                            break;
                        }

                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state
                            = BCSCLST_RESPAWNING;
                        if (gettimeofday(&state->client[tmp_primitive]
                                         .private_info
                                         .time_last_fire, NULL) == -1) {
                            perror("gettimeofday in RIGHT");
                        }
                        // (TODO) fixed respawn time
                        // ALOGI("Death time %ld\n", state->client[tmp_primitive]
                        //       .private_info
                        //       .time_last_fire
                        //       .tv_sec);

                        state->client[tmp_primitive]
                           .private_info
                           .time_last_fire
                           .tv_sec += RESPAWN_TIMER;
                        ++(state->client[rifleman].public_ext_info.frags);
                        // ALOGI("Respawn after %ld\n", state->client[tmp_primitive]
                        //       .private_info
                        //       .time_last_fire
                        //       .tv_sec);

                        // копипаст-программирование - говно!
                        ALOGD("KILL event at (%hu, %hu), hunter: %hu '%s', victim: %hhu '%s'\n"
		                    , bullet_x, bullet_y
		                    , rifleman, state->client[rifleman].public_ext_info.nickname
		                    , tmp_primitive, state->client[tmp_primitive].public_ext_info.nickname
		                );
                        lassert(state->client[tmp_primitive].public_info.position.x == bullet_x);
                        lassert(state->client[tmp_primitive].public_info.position.y == bullet_y);

                        return false;
                        break;
                    }
                }
            }
            break;
        }
        default: {
            break;
        }
    }
    // rewrite x and y coordinates bullet
    bullet->x = bullet_x;
    bullet->y = bullet_y;

    return true;
}

bool bcsgameplay_respawn(BCSSERVER_FULL_STATE *state, size_t id) {

    if ((state->client[id].public_info.state) != BCSCLST_RESPAWNING) {
        return false;
    }

    timeval128_t seed;

    if (gettimeofday(&seed, NULL) == -1) {
        perror("random failed");
    }

    srand(seed.tv_usec);

    LIST_VALTYPE *lv;
    LINKED_LIST_ENTRY *lle = NULL;
    uint16_t spawn_coordinate_y;
    uint16_t spawn_coordinate_x;
    uint16_t height = state->map.height;
    uint16_t width = state->map.width;
    uint16_t offset_y = 0;
    uint16_t offset_x = 0;
    uint16_t useful_area_width = CHECK_AREA_SIZE;
    uint16_t useful_area_hight = CHECK_AREA_SIZE;

    uint16_t start_area;
    uint16_t end_area;

    uint8_t danger_lvl = 0;

    size_t map_size = height * width;
    uint8_t *map_overlay_copy = state->map_overlay.map_primitives;

    while (true) {
        spawn_coordinate_y = rand() % height;                                   // choose random spawn place
        spawn_coordinate_x = rand() % width;

        if (spawn_coordinate_x < CHECK_AREA_SIZE) {
            offset_x = spawn_coordinate_x + 1;
            useful_area_width = CHECK_AREA_SIZE - offset_x;
        }
        if (spawn_coordinate_y < CHECK_AREA_SIZE) {
            offset_y = spawn_coordinate_y + 1;
            useful_area_hight = CHECK_AREA_SIZE - offset_y;
        }
        if (spawn_coordinate_x > (width - CHECK_AREA_SIZE)) {
            offset_x = (spawn_coordinate_x % CHECK_AREA_SIZE);
            useful_area_width = CHECK_AREA_SIZE - offset_x;
        }
        if (spawn_coordinate_y > (height - CHECK_AREA_SIZE)) {
            offset_y = (spawn_coordinate_y % CHECK_AREA_SIZE);
            useful_area_hight = CHECK_AREA_SIZE - offset_y;
        }

        start_area = (spawn_coordinate_y - useful_area_hight) *
            width + (spawn_coordinate_x - useful_area_width); // start check area

        end_area = (spawn_coordinate_y + useful_area_hight) *
            width + (spawn_coordinate_x + useful_area_width); // end check area

        // Str1ker, 13.08.2018: попытка пофиксить выход за границы массива.
        //end_area = min(end_area, width * height - 1);
        lassert(end_area <= width * height - 1);
        // Илья Коротецкий, [13.08.18 23:47]
        // ну тогда и для start тоже
        // Илья Коротецкий, [13.08.18 23:47]
        // потому что старт тоже может выйти
        lassert(start_area <= width * height - 1);

        if (map_overlay_copy[spawn_coordinate_y * width + spawn_coordinate_x]
            != PUNIT_OPEN_SPACE) {
            // check
            continue; // can respawn possibility
        } // at this coordinate

        for (uint16_t i = start_area; i < end_area - width; i += width) {
            for (uint16_t j = 0; j < useful_area_width; ++j) {
                if (map_overlay_copy[i + j] < BCSSERVER_MAXCLIENTS) {
                    // potential danger
                    ++danger_lvl;
                }
            }
        }

        while ((lv = linkedlist_next_r(&state->bullets, &lle)) != NULL) {
            BCSBULLET *bullet = (BCSBULLET*)(lv->ptr);

            if (bullet->y == spawn_coordinate_y) {
                // bullet flying on line of player
                if (bullet->x < spawn_coordinate_x &&
                    bullet->direction == BCSDIR_RIGHT) {
                    // bullet flying to player from leftside
                    ++danger_lvl;
                }
                else if (bullet->x > spawn_coordinate_x &&
                    bullet->direction == BCSDIR_LEFT) {
                    // bullet flying to player from rightside
                    ++danger_lvl;
                }
            }
            else if (bullet->x == spawn_coordinate_x) {
                // bullet flying on line of player
                if (bullet->y > spawn_coordinate_y &&
                    bullet->direction == BCSDIR_UP) {
                    // bullet flying to player from upper
                    ++danger_lvl;
                }

                else if (bullet->y < spawn_coordinate_y &&
                    bullet->direction == BCSDIR_DOWN) {
                    // bullet flying to player from down
                    ++danger_lvl;
                }
            }
        }

        if (danger_lvl > 1) {
            // At least one threat nearby
            continue; // look up new      
        }

        break;
    }

    state->client[id].public_info.state = BCSCLST_PLAYING;
    state->client[id].public_info.position.x = spawn_coordinate_x;
    state->client[id].public_info.position.y = spawn_coordinate_y;

    return true;
}
