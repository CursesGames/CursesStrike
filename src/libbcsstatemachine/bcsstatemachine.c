#include <pthread.h>
#include <endian.h>

#include "bcsstatemachine.h"
#include "clientarray.h"
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include "../libbcsproto/bcsproto.h"

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
    BCSMSGREPLY reply; //reply to client
    int id = search_client(state, src); // client id in array
    reply.packet_no = msg->packet_no; // packet to send number

    pthread_mutex_lock(&state->mutex_self);
    int u_fd = state->sock_u; // copy descriptor from state
    pthread_mutex_unlock(&state->mutex_self);
    int x, y; //coordinates for moving

    switch(be32toh(msg->action)){
        case BCSACTION_CONNECT: // client sent CONNECT
            if(search_client(state, src) == -1){//ONLY IF THERE IS NO SUCH CLIENT IN ARRAY
                printf("received CONNECT from client\n");
                // Add client to array
                switch(add_client(state, src)){
                    case -1: // clients limit is settled
                        reply.type = htobe32(BCSREPLT_NACK);
                        break;

                    default:
                        pthread_mutex_lock(&state->mutex_self);
                        state->client[id].public_info.state = BCSCLST_CONNECTING;
                        pthread_mutex_unlock(&state->mutex_self);
                        reply.type = htobe32(BCSREPLT_MAP);
                        log_print_cl_info(state);
                }
                __syscall(gettimeofday(&(reply.time_gen), NULL));
                __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 0, (sockaddr*)src, sizeof(sockaddr_in)));
            }
            break;
            
        case BCSACTION_CONNECT2:// client sent CONNECT2
            pthread_mutex_lock(&state->mutex_self);
            if((state->client[id].public_info.state == BCSCLST_CONNECTING)
                || (state->client[id].public_info.state == BCSCLST_PLAYING)
                || (state->client[id].public_info.state == BCSCLST_RESPAWNING)){
                state->client[id].public_info.state = BCSCLST_CONNECTED;
            }
            pthread_mutex_unlock(&state->mutex_self);
            break;
                
        case BCSACTION_DISCONNECT:
            pthread_mutex_lock(&state->mutex_self);
            delete_client(state, src);
            pthread_mutex_unlock(&state->mutex_self);
            reply.type = htobe32(BCSREPLT_ACK);
            __syscall(gettimeofday(&(reply.time_gen), NULL));
            __syscall(sendto(u_fd, &reply, sizeof(BCSMSGREPLY), 0, (sockaddr*)src, sizeof(sockaddr_in)));
            break;

        case BCSACTION_MOVE:
            pthread_mutex_lock(&state->mutex_self);
            if(state->client[id].public_info.state == BCSCLST_PLAYING){
                x = state->client[id].public_info.position.x;
                y = state->client[id].public_info.position.y;
                switch(be32toh(msg->un.ints.int_lo)) {
                    case BCSDIR_LEFT:
                        x--;
                        if((x != 0)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.x--;
                        }
                        break;
                        
                    case BCSDIR_RIGHT:
                        x++;
                        if((x != state->map.width)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.x++;
                        }
                        break;

                    case BCSDIR_UP:
                        y--;
                        if((y != 0)
                            && ((state->map.map_primitives[y * state->map.width + x]) == PUNIT_OPEN_SPACE)
                            && (isFree(state, x, y) == true)){
                            state->client[id].public_info.position.y--;
                        }
                        break;

                    case BCSDIR_DOWN:
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
            pthread_mutex_unlock(&state->mutex_self);
            break;

        //case BCSACTION_STRAFE: //UNDEFINED
        //    reply.type = htobe32(BCSREPLT_NACK);
        //    break;
        
        //case BCSACTION_ROTATE: //UNDEFINED
        //    reply->type = htobe32(BCSREPLT_NACK);
        //    break;

        //case BCSACTION_REQSTATS: //UNDEFINED
        //    reply->type = htobe32(BCSREPLT_NACK);
        //    break;

        //default:
            
    }

    return true;
}