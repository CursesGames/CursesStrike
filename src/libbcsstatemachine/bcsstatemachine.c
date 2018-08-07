#include <pthread.h>
#include <endian.h>
#include <alloca.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    //there is no client with such endpoint in array and this client did not send CONNECT
    if((search_client(state, src) == -1) && (be32toh(msg->action) != BCSACTION_CONNECT)){
        return false;
    }

    // TODO: добавить больше цветочков!
    uint16_t player_count;
    int i;
    BCSMSGREPLY reply; //reply to client
    reply.packet_no = msg->packet_no; // packet to send number
    int id = search_client(state, src); // client id in array

    pthread_mutex_lock(&state->mutex_self);
    int u_fd = state->sock_u; // copy descriptor from state
    pthread_mutex_unlock(&state->mutex_self);
    switch(be32toh(msg->action)){
        case BCSACTION_CONNECT: // client sent CONNECT;
            if(search_client(state, src) == -1){//ONLY IF THERE IS NO SUCH CLIENT IN ARRAY
                ALOGW("received CONNECT from client");
                // Add client to array
                switch(add_client(state, src)){
                    case -1: // clients limit is settled
                        reply.type = htobe32(BCSREPLT_NACK);
                        break;

                    default:
                        pthread_mutex_lock(&state->mutex_self);
                        state->player_count++;
                        state->client[id].public_info.state = BCSCLST_CONNECTING;
                        pthread_mutex_unlock(&state->mutex_self);
                        ALOGW("state to CONNECTING");
                        reply.type = htobe32(BCSREPLT_MAP);
                        log_print_cl_info(state);
                }
                //__syscall(gettimeofday(&reply.time_gen, NULL));
                __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                          0, (sockaddr*)src, sizeof(sockaddr_in)));
            }
            break;
            
        case BCSACTION_CONNECT2:// client sent CONNECT2
            pthread_mutex_lock(&state->mutex_self);
            if((state->client[id].public_info.state == BCSCLST_CONNECTING)
                || (state->client[id].public_info.state == BCSCLST_PLAYING)
                || (state->client[id].public_info.state == BCSCLST_RESPAWNING)){
                state->client[id].public_info.state = BCSCLST_CONNECTED;
                reply.type = htobe32(BCSREPLT_ACK);
            }
            else{
                reply.type = htobe32(BCSREPLT_NACK);
            }
            pthread_mutex_unlock(&state->mutex_self);
            //__syscall(gettimeofday(&reply.time_gen, NULL));
            __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                             0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;
                
        case BCSACTION_DISCONNECT:
            delete_client(state, src);
            reply.type = htobe32(BCSREPLT_ACK);
            //__syscall(gettimeofday(&reply.time_gen, NULL));
            __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                             0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;

        case BCSACTION_MOVE:
            pthread_mutex_lock(&state->mutex_self);
            if(state->client[id].public_info.state == BCSCLST_PLAYING){
                ALOGW("received MOVE from client");
                uint16_t x = state->client[id].public_info.position.x;
                ALOGW("init x pos = %d", x);
                uint16_t y = state->client[id].public_info.position.y;
                ALOGW("init x pos = %d", y);
                switch(be32toh(msg->un.ints.int_lo)) {
                    case BCSDIR_LEFT:
                        ALOGW("LEFT");
                        x--;
                        if((x != 0)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.x--;
                        }
                        break;
                        
                    case BCSDIR_RIGHT:
                        ALOGW("RIGHT");
                        x++;
                        if((x != state->map.width)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.x++;
                        }
                        break;

                    case BCSDIR_UP:
                        ALOGW("UP");
                        y--;
                        if((y != 0)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.y--;
                        }
                        break;

                    case BCSDIR_DOWN:
                        ALOGW("DOWN");
                        y++;
                        if((y != state->map.height)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.y++;
                        }
                        break;
                }
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;

        case BCSACTION_FIRE:
            pthread_mutex_lock(&state->mutex_self);
            if(state->client[id].public_info.state == BCSCLST_CONNECTED){
                state->client[id].public_info.state = BCSCLST_PLAYING;
            }
            else if(state->client[id].public_info.state == BCSCLST_PLAYING){
                BCSBULLET *bullet =  malloc(sizeof(BCSBULLET));
                bullet->creator_id = id;
                bullet->x = state->client[id].public_info.position.x;
                bullet->y = state->client[id].public_info.position.y;
                bullet->direction = state->client[id].public_info.direction;
                LIST_VALTYPE val = { .ptr = bullet };
                linkedlist_push_back(&state->bullets, val);

            }
            pthread_mutex_unlock(&state->mutex_self);
            break;
        
        case BCSACTION_ROTATE: //UNDEFINED
            pthread_mutex_lock(&state->mutex_self);
            switch(be32toh(msg->un.ints.int_lo)) {
                case BCSDIR_RIGHT:

                    state->client[id].public_info.direction = BCSDIR_RIGHT;
                    break;

                case BCSDIR_LEFT:
                    state->client[id].public_info.direction = BCSDIR_LEFT;
                    
                    break;
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;

        case BCSACTION_REQSTATS:
            player_count = return_clients_size(state); //htobe

            BCSMSGREPLY *stats_to_send = alloca(
              sizeof(BCSMSGREPLY)
            + sizeof(BCSCLIENT_PUBLIC_EXT) * player_count
            );

            stats_to_send->type = htobe32(BCSREPLT_STATS);
            //__syscall(gettimeofday(&(stats_to_send->time_gen), NULL));

            BCSCLIENT_PUBLIC_EXT *array = (BCSCLIENT_PUBLIC_EXT*)(stats_to_send + 1);
            pthread_mutex_lock(&state->mutex_self);
               for(i = 0; i < BCSSERVER_MAXCLIENTS; i++){
                   if(state->client[i].public_info.state != BCSCLST_FREESLOT){
                        array->frags = htobe16(state->client[i].public_ext_info.frags);
                        array->frags = htobe16(state->client[i].public_ext_info.deaths);
                        strncpy(array->nickname, state->client[i].public_ext_info.nickname, BCSPLAYER_NICKLEN + 1);
                        array++;
                    }
                }
            pthread_mutex_unlock(&state->mutex_self);
                __syscall(sendto(u_fd, stats_to_send, 
                          sizeof(BCSMSGREPLY) + sizeof(BCSCLIENT_PUBLIC_EXT) * player_count,
                          0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;
        //default:
            
    }

    return true;
}