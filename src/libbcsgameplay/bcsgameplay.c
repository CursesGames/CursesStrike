#include <string.h>
#include <stdlib.h>

#include "bcsgameplay.h"
#include "../libbcsmap/bcsmap.h"

bool bcsgameplay_bullet_step(BCSSERVER_FULL_STATE *state, BCSBULLET *bullet, int steps) 
{
    int line_size = 0;

    size_t rifleman = bullet->creator_id;

    uint16_t player_count = state->player_count;  // number of players in server
    uint16_t height = state->map.height;
    uint16_t width = state->map.width;
    uint16_t bullet_x = bullet->x;
    uint16_t bullet_y = bullet->y;
    uint16_t client_x;
    uint16_t client_y;
    uint8_t tmp_primitive;

    // для оптимизации работы функции было принято решение перед определением 
    // состояния пули изначально расставить всех вражеских игроков по карте
    // чтобы предотвратить множественный проход по массиву клиентов.
    uint8_t* my_copy_primitives = malloc(height * width);

    //memset(my_copy_primitives, 0, height * width);
    memcpy(my_copy_primitives, state->map.map_primitives, height * width);

    // единожды расставляем на локальной копии карты всех клиентов
    for (int i = 0; i < BCSSERVER_MAXCLIENTS; ++i) {
        if (state->client[i].public_info.state == BCSCLST_PLAYING) {
            client_x = state->client[i].public_info.position.x;
            client_y = state->client[i].public_info.position.y;
            my_copy_primitives[width*client_y + client_x] = i;  // размещаем номер "врага"
        }
    }

    switch (bullet->direction) {
        case BCSDIR_LEFT:   // y - constant, x can move at negative direction 
            line_size = width * bullet_y;
            for (int i = 0; i < steps; ++i) {
                if (bullet_x == 0) {
                    return false;
                } else {
                    --bullet_x;
                }
                tmp_primitive = my_copy_primitives[line_size + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
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
                tmp_primitive = my_copy_primitives[line_size + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
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
                tmp_primitive = my_copy_primitives[width * bullet_y + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
                        ++(state->client[rifleman].public_ext_info.frags);
                        return false;
                }
            }
            break;
        case BCSDIR_DOWN:   // x - cinstant, y can move at positive direction
            for (int i = 0; i < steps; ++i) {
               if (bullet_y == height) {
                    return false;
                } else {
                    ++bullet_y;
                }
                tmp_primitive = my_copy_primitives[width * bullet_y + bullet_x];
                switch (tmp_primitive) {
                    case PUNIT_OPEN_SPACE:
                    case PUNIT_WATER:
                        break;
                    case PUNIT_ROCK:
                    case PUNIT_BOX:
                        return false;
                    default:
                        ++(state->client[tmp_primitive].public_ext_info.deaths);
                        state->client[tmp_primitive].public_info.state 
                                                        = BCSCLST_RESPAWNING;
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
