#include <pthread.h>
#include <endian.h>
#include <alloca.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bcsstatemachine.h"
#include "clientarray.h"
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include "../libbcsproto/bcsproto.h"
#include "../liblinkedlist/linkedlist.h"
#include "../libbcsgameplay/bcsgameplay.h"

bool bcsstatemachine_process_request(
    BCSSERVER_FULL_STATE *state
  , sockaddr_in *src
  , BCSMSG *msg
  , ssize_t msglen
) {

    // TODO: 
    // Str1ker, 09.08.2018: don't send even NACKS when in-game
    // (иногда мне в клиент в игровом режиме прилетают наки, проверить на это нужно)

    int id = search_client(state, src); // return -1 if it's new client
    //there is no client with such endpoint in array and this client did not send CONNECT or REQSTATS
    if ((id == -1)
        && (be32toh(msg->action) != BCSACTION_CONNECT)
        && (be32toh(msg->action) != BCSACTION_REQSTATS)) {
        return false;
    }

    // TODO: добавить больше цветочков!
    int i;
    timeval128_t val_time, res;
    timeval128_t now;
    uint16_t x, y;
    bool flag = true;

    __syswrap(gettimeofday(&now, NULL));

    BCSMSGREPLY reply = {
        .packet_no = msg->packet_no // packet to send number
      , .time_gen = now
    }; //reply to client

    pthread_mutex_lock(&state->mutex_self);
    int u_fd = state->sock_u; // copy descriptor from state 
    if (id != -1) {
        state->client[id].private_info.time_last_dgram = now;
    }
    pthread_mutex_unlock(&state->mutex_self);

    switch (be32toh(msg->action)) {
        case BCSACTION_CONNECT: { // client sent CONNECT; 
            switch (id) {
                    //ONLY IF THERE IS NO SUCH CLIENT IN ARRAY
                case -1: {
                    ALOGD("received CONNECT from client\n");
                    // Add client to array
                    //pthread_mutex_lock(&state->mutex_self);
                    id = add_client(state, src, msg);
                    switch (id) {
                        case -1: // clients limit is settled
                            reply.type = htobe32(BCSREPLT_NACK);
                            flag = false;
                            break;

                        default:
                            state->player_count++;
                            reply.type = htobe32(BCSREPLT_MAP);
                            log_print_cl_info(state);
                            break;
                    }
                    //pthread_mutex_unlock(&state->mutex_self);
                    __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                        0, (sockaddr*)src, sizeof(sockaddr_in)));
                    break;
                }

                default: {
                    // игрок может вернуться на то же место, если совпадает порт
                    pthread_mutex_lock(&state->mutex_self);
                    reply.type = htobe32(BCSREPLT_MAP);
                    __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                        0, (sockaddr*)src, sizeof(sockaddr_in)));
                    pthread_mutex_unlock(&state->mutex_self);
                    break;
                }
            }
            break;
        }

        case BCSACTION_CONNECT2: { // client sent CONNECT2, obviously
            ALOGD("received CONNECT2 from client\n");
            pthread_mutex_lock(&state->mutex_self);
            // сначала мне не понравился этот иф, но потом я подумал:
            // может, игрок будет переходить в спектаторы как раз отправив CONNECT2?
            if ((state->client[id].public_info.state == BCSCLST_CONNECTING)
                || (state->client[id].public_info.state == BCSCLST_PLAYING)
                || (state->client[id].public_info.state == BCSCLST_RESPAWNING)) {
                state->client[id].public_info.state = BCSCLST_CONNECTED;
                reply.type = htobe32(BCSREPLT_ACK);
            }
            else {
                reply.type = htobe32(BCSREPLT_NACK);
                flag = false;
            }
            pthread_mutex_unlock(&state->mutex_self);
            __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;
        }

        case BCSACTION_DISCONNECT: {
            ALOGD("received DISCONNECT from client\n");
            //pthread_mutex_lock(&state->mutex_self);
            delete_client(state, src);
            //pthread_mutex_unlock(&state->mutex_self);
            reply.type = htobe32(BCSREPLT_ACK);
            __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;
        }

        case BCSACTION_MOVE: {
            if (id == -1) {
                flag = false;
                break;
            }
            ALOGD("received MOVE from client\n");
            pthread_mutex_lock(&state->mutex_self);
            switch (state->client[id].public_info.state) {
                case BCSCLST_PLAYING: {
                    state->client[id].private_info.time_last_activity = now;
                    // decline move request if too fast
                    timersub(&now, &state->client[id].private_info.time_last_move, &res);
                    val_time.tv_sec = 0;
                    val_time.tv_usec = BCSGP_MOVE_TIMEBOUND;
                    if(timercmp(&res, &val_time, <)) {
                        // пока ещё нельзя двигаться
                        ALOGV("MOVE declined\n");
                        flag = false;
	                    break;
                    }
                    x = state->client[id].public_info.position.x;
                    y = state->client[id].public_info.position.y;
                    switch (be32toh(msg->un.ints.int_lo)) {
                        case BCSDIR_LEFT: {
                            if ((x != 0)
                                && ((state->map.map_primitives[y * state->map.width + (x - 1)]) ==
                                    PUNIT_OPEN_SPACE)
                                && (isFree(state, x - 1, y) == true)) {
                                state->client[id].public_info.position.x--;
                            }
                            else {
                                flag = false;
                            }
                            break;
                        }

                        case BCSDIR_RIGHT: {
                            if ((x != (state->map.width - 1))
                                && ((state->map.map_primitives[y * state->map.width + (x + 1)]) ==
                                    PUNIT_OPEN_SPACE)
                                && (isFree(state, x + 1, y) == true)) {
                                state->client[id].public_info.position.x++;
                            }
                            else {
                                flag = false;
                            }
                            break;
                        }

                        case BCSDIR_UP: {
                            if ((y != 0)
                                && ((state->map.map_primitives[(y - 1) * state->map.width + x]) ==
                                    PUNIT_OPEN_SPACE)
                                && (isFree(state, x, y - 1) == true)) {
                                state->client[id].public_info.position.y--;
                            }
                            else {
                                flag = false;
                            }
                            break;
                        }

                        case BCSDIR_DOWN: {
                            if ((y != (state->map.height - 1))
                                && ((state->map.map_primitives[(y + 1) * state->map.width + x]) ==
                                    PUNIT_OPEN_SPACE)
                                && (isFree(state, x, y + 1) == true)) {
                                state->client[id].public_info.position.y++;
                            }
                            else {
                                flag = false;
                            }
                            break;
                        }

                        default: {
                            flag = false;
                            break;
                        }
                    }
                    if(flag) { // успешное перемещение
                        state->client[id].private_info.time_last_move = now;
                    }
                    break;
                }

                case BCSCLST_CONNECTED: { // спектатор
                    // хотя их-то можно кикнуть только за разрыв соединения, лол
                    break;
                }

                case BCSCLST_CONNECTING: {
                    reply.type = htobe32(BCSREPLT_NACK);
                    __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                        0, (sockaddr*)src, sizeof(sockaddr_in)));
                    flag = false;
                    break;
                }

                default: {
                    flag = false;
                    break;
                }
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;
        }

        case BCSACTION_FIRE: {
            if (id == -1) {
                flag = false;
                break;
            }
            ALOGD("received FIRE from client\n");
            pthread_mutex_lock(&state->mutex_self);
            switch (state->client[id].public_info.state) {
                case BCSCLST_CONNECTED: {
                    // более логично сначала установить время, а только после изменить статус
                    // и вообще, время выстрела изначально 0, так что можно обойтись
                    //__syscall(gettimeofday(&(state->client[id].private_info.time_last_fire), NULL));
                    state->client[id].private_info.time_last_activity = now;
                    state->client[id].public_info.state = BCSCLST_RESPAWNING;
                    //bcsgameplay_respawn(state, id);
                    break;
                }

                case BCSCLST_PLAYING: {
                    state->client[id].private_info.time_last_activity = now;
                    timersub(&now, &state->client[id].private_info.time_last_fire, &res);
                    //ALOGI("res time: %ld.%06ld\n", res.tv_sec, res.tv_usec);
                    val_time.tv_sec = 0;
                    val_time.tv_usec = BCSGP_FIRE_CALMDOWN;
                    if (timercmp(&res, &val_time, >=)) {
                        //ALOGI("time norm");
                        BCSBULLET *bullet = malloc(sizeof(BCSBULLET));
                        bullet->creator_id = id;
                        bullet->x = state->client[id].public_info.position.x;
                        bullet->y = state->client[id].public_info.position.y;
                        bullet->direction = state->client[id].public_info.direction;
                        LIST_VALTYPE val = { .ptr = bullet };
                        linkedlist_push_back(&state->bullets, val);
                        state->client[id].private_info.time_last_fire = now;
                    }
                    else {
                        ALOGV("FIRE declined\n");
                        flag = false;
                    }
                    break;
                }

                case BCSCLST_CONNECTING: {
                    reply.type = htobe32(BCSREPLT_NACK);
                    __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                        0, (sockaddr*)src, sizeof(sockaddr_in)));
                    flag = false;
                    break;
                }

                default: {
                    flag = false;
                    break;
                }
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;
        }

        case BCSACTION_ROTATE: {
            if (id == -1) {
                flag = false;
                break;
            }
            ALOGD("received ROTATE from client\n");
            pthread_mutex_lock(&state->mutex_self);
            switch (state->client[id].public_info.state) {
                case BCSCLST_PLAYING: {
                    switch (be32toh(msg->un.ints.int_lo)) {
                        case BCSDIR_RIGHT: {
                            state->client[id].public_info.direction = 
		                        (state->client[id].public_info.direction + 1) % 4;
                            break;
                        }

                        case BCSDIR_LEFT: {
                            state->client[id].public_info.direction = 
		                        (state->client[id].public_info.direction + 3) % 4;
                            break;
                        }

                        default: {
                            flag = false;
                            break;
                        }
                    }
                    break;
                }

                case BCSCLST_CONNECTING: {
                    reply.type = htobe32(BCSREPLT_NACK);
                    __syswrap(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                        0, (sockaddr*)src, sizeof(sockaddr_in)));
                    flag = false;
                    break;
                }

                default: {
                    flag = false;
                    break;
                }
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;
        }

        case BCSACTION_REQSTATS: {
            ALOGD("received REQSTATS from client\n");
            uint16_t player_count = return_clients_size(state);
            // доступ к state->bullets.count, ставим мьютекс...
            int index_self = search_client(state, src);
            if(index_self == -1) { // если статистику запросил аноним
                index_self = UINT16_MAX;
            }
            pthread_mutex_lock(&state->mutex_self);
            uint16_t bullets_count = (uint16_t)state->bullets.count;

            BCSMSGREPLY *stats_to_send = alloca(
                  sizeof(BCSMSGREPLY)
                + sizeof(BCSMSGANNOUNCE)
                + sizeof(BCSCLIENT_PUBLIC_EXT) * player_count
            );

            stats_to_send->type = htobe32(BCSREPLT_STATS);
            stats_to_send->packet_no = msg->packet_no;
            __syswrap(gettimeofday(&(stats_to_send->time_gen), NULL));

            BCSMSGANNOUNCE *ann = (BCSMSGANNOUNCE*)(stats_to_send + 1);
            ann->count = htobe16(player_count);
            ann->count_bullets = htobe16(bullets_count);
            ann->_zero = 0; // valgrind
            ann->index_self = index_self;

            size_t annlen = 
		          sizeof(BCSMSGREPLY)
                + sizeof(BCSMSGANNOUNCE)
                + sizeof(BCSCLIENT_PUBLIC_EXT) * player_count;

            BCSCLIENT_PUBLIC_EXT *array = (BCSCLIENT_PUBLIC_EXT*)(ann + 1);
            //pthread_mutex_lock(&state->mutex_self);
            for (i = 0; i < BCSSERVER_MAXCLIENTS; i++) {
                if (state->client[i].public_info.state != BCSCLST_FREESLOT) {
                    array->frags = htobe16(state->client[i].public_ext_info.frags);
                    array->deaths = htobe16(state->client[i].public_ext_info.deaths);
                    strncpy(array->nickname, state->client[i].public_ext_info.nickname
                          , BCSPLAYER_NICKLEN);
                    array++;
                }
            }
            pthread_mutex_unlock(&state->mutex_self);
            __syswrap(sendto(u_fd, stats_to_send, annlen,
                0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;
        }

        case BCSACTION_KEEPALIVE: {
            state->client[id].private_info.time_last_activity = now;
            break;
        }

        default: {
            flag = false;
            break;
        }
    }

    return flag;
}
