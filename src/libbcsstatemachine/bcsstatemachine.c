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

    int id = search_client(state, src); // return -1 if it's new client
    //there is no client with such endpoint in array and this client did not send CONNECT or REQSTATS
    if((id == -1) 
        && (be32toh(msg->action) != BCSACTION_CONNECT)
        && (be32toh(msg->action) != BCSACTION_REQSTATS)){
        return false;
    }

    // TODO: добавить больше цветочков!
    int i;
    BCSMSGREPLY reply; //reply to client
    BCSBULLET *bullet;
    reply.packet_no = msg->packet_no; // packet to send number
    uint16_t x, y;
    bool flag = true;

    pthread_mutex_lock(&state->mutex_self);
    int u_fd = state->sock_u; // copy descriptor from state 
    if(id != -1){
        __syscall(gettimeofday(&state->client[id].private_info.time_last_dgram, NULL));
    }
    pthread_mutex_unlock(&state->mutex_self);

    switch(be32toh(msg->action)){
        case BCSACTION_CONNECT: // client sent CONNECT; 
            switch(search_client(state, src)){ //ONLY IF THERE IS NO SUCH CLIENT IN ARRAY
                case -1:
                    ALOGW("received CONNECT from client\n");
                    // Add client to array
                    switch(add_client(state, src, msg)){
                        case -1: // clients limit is settled
                            reply.type = htobe32(BCSREPLT_NACK);
                            flag = false;
                            break;

                        default:
                            pthread_mutex_lock(&state->mutex_self);
                            state->player_count++;
                            pthread_mutex_unlock(&state->mutex_self);
                            reply.type = htobe32(BCSREPLT_MAP);
                            log_print_cl_info(state);
                            break;
                    }
                    __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                                     0, (sockaddr*)src, sizeof(sockaddr_in)));
                    break;

                default:
                    flag = false;
                    break;
            }
            break;
            
        case BCSACTION_CONNECT2:// client sent CONNECT2
            ALOGW("received CONNECT2 from client\n");
            pthread_mutex_lock(&state->mutex_self);
            if((state->client[id].public_info.state == BCSCLST_CONNECTING)
                || (state->client[id].public_info.state == BCSCLST_PLAYING)
                || (state->client[id].public_info.state == BCSCLST_RESPAWNING)){
                state->client[id].public_info.state = BCSCLST_CONNECTED;
                reply.type = htobe32(BCSREPLT_ACK);
            }
            else{
                reply.type = htobe32(BCSREPLT_NACK);
                flag = false;
            }
            pthread_mutex_unlock(&state->mutex_self);
            __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY),
                             0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;
                
        case BCSACTION_DISCONNECT:
            ALOGW("received DISCONNECT from client\n");
            delete_client(state, src);
            reply.type = htobe32(BCSREPLT_ACK);
            __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                             0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;

        case BCSACTION_MOVE:
            ALOGW("received MOVE from client\n");
            pthread_mutex_lock(&state->mutex_self);
            switch (state->client[id].public_info.state) {
                case BCSCLST_PLAYING:
                    x = state->client[id].public_info.position.x;
                    y = state->client[id].public_info.position.y;
                    switch (be32toh(msg->un.ints.int_lo)) {
                        case BCSDIR_LEFT:
                            if ((x != 0)
                                && ((state->map.map_primitives[y * state->map.width + (x - 1)]) == PUNIT_OPEN_SPACE)
                                && (isFree(state, x - 1, y) == true)) {
                                state->client[id].public_info.position.x--;
                            }
                            else {
                                flag = false;
                            }
                            break;
                        
                        case BCSDIR_RIGHT:
                            if ((x != (state->map.width - 1))
                                && ((state->map.map_primitives[y * state->map.width + (x + 1)]) == PUNIT_OPEN_SPACE)
                                && (isFree(state, x + 1, y) == true)) {
                                state->client[id].public_info.position.x++;
                            }
                            else {
                                flag = false;
                            }
                            break;

                        case BCSDIR_UP:
                            if ((y != 0)
                                && ((state->map.map_primitives[(y - 1) * state->map.width + x]) == PUNIT_OPEN_SPACE)
                                && (isFree(state, x, y - 1) == true)) {
                                state->client[id].public_info.position.y--;
                            }
                            else {
                                flag = false;
                            }
                            break;

                        case BCSDIR_DOWN:
                            if ((y != (state->map.height - 1))
                                && ((state->map.map_primitives[(y + 1)* state->map.width + x]) == PUNIT_OPEN_SPACE)
                                && (isFree(state, x, y + 1) == true)) {
                                state->client[id].public_info.position.y++;
                            }
                            else {
                                flag = false;
                            }
                            break;

                        default:
                            flag = false;
                            break;
                    }

                default:
                    reply.type = htobe32(BCSREPLT_NACK);
                    __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                                     0, (sockaddr*)src, sizeof(sockaddr_in)));
                    flag = false;
                    break;
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;

        case BCSACTION_FIRE:
            ALOGW("received FIRE from client\n");
            pthread_mutex_lock(&state->mutex_self);
            switch(state->client[id].public_info.state){
                case BCSCLST_CONNECTED:
                    state->client[id].public_info.state = BCSCLST_RESPAWNING;
                    //__syscall(gettimeofday(&state->client[id].private_info.time_last_fire, NULL));
                    bcsgameplay_respawn(state, id);
                    break;

                case BCSCLST_PLAYING:
                    bullet =  malloc(sizeof(BCSBULLET));
                    bullet->creator_id = id;
                    bullet->x = state->client[id].public_info.position.x;
                    bullet->y = state->client[id].public_info.position.y;
                    bullet->direction = state->client[id].public_info.direction;
                    LIST_VALTYPE val = { .ptr = bullet };
                    linkedlist_push_back(&state->bullets, val);
                    break;

                default:
                    reply.type = htobe32(BCSREPLT_NACK);
                    __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                                     0, (sockaddr*)src, sizeof(sockaddr_in)));
                    flag = false;
                    break;
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;
        
        case BCSACTION_ROTATE:
            ALOGW("received ROTATE from client\n");
            pthread_mutex_lock(&state->mutex_self);
            switch(state->client[id].public_info.state){
                case BCSCLST_PLAYING:
                    switch(be32toh(msg->un.ints.int_lo)) {
                        case BCSDIR_RIGHT:
                            state->client[id].public_info.direction = (state->client[id].public_info.direction + 1) % 4;
                            break;

                        case BCSDIR_LEFT:
                            state->client[id].public_info.direction = (state->client[id].public_info.direction + 3) % 4;
                            break;

                        default:
                            flag = false;
                            break;
                    }
                    break;

                default:
                    reply.type = htobe32(BCSREPLT_NACK);
                    __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 
                                     0, (sockaddr*)src, sizeof(sockaddr_in)));
                    flag = false;
                    break;
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;

        case BCSACTION_REQSTATS:
            ALOGW("received REQSTATS from client\n");
            uint16_t player_count = return_clients_size(state);
            uint16_t bullets_count = state->bullets.count;

            BCSMSGREPLY *stats_to_send = alloca(
              sizeof(BCSMSGREPLY) 
            + sizeof(BCSMSGANNOUNCE) 
            + sizeof(BCSCLIENT_PUBLIC_EXT) * player_count
            );

            stats_to_send->type = htobe32(BCSREPLT_STATS);
            //__syscall(gettimeofday(&(stats_to_send->time_gen), NULL));

            BCSMSGANNOUNCE *ann = (BCSMSGANNOUNCE*)(stats_to_send + 1);
            ann->count = htobe16(player_count);
            ann->count_bullets = htobe16(bullets_count);

            size_t annlen =   sizeof(BCSMSGREPLY)
                            + sizeof(BCSMSGANNOUNCE) 
                            + sizeof(BCSCLIENT_PUBLIC_EXT) * player_count;

            BCSCLIENT_PUBLIC_EXT *array = (BCSCLIENT_PUBLIC_EXT*)(ann + 1);
            pthread_mutex_lock(&state->mutex_self);
               for(i = 0; i < BCSSERVER_MAXCLIENTS; i++){
                   if(state->client[i].public_info.state != BCSCLST_FREESLOT){
                        array->frags = htobe16(state->client[i].public_ext_info.frags);
                        array->deaths = htobe16(state->client[i].public_ext_info.deaths);
                        strncpy(array->nickname, state->client[i].public_ext_info.nickname, BCSPLAYER_NICKLEN + 1);
                        array++;
                    }
                }
            pthread_mutex_unlock(&state->mutex_self);
            __syscall(sendto(u_fd, stats_to_send, annlen, 
                             0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;

        default:
            flag = false;
            break;
            
    }

    return flag;
}
