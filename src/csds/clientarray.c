#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "clientarray.h"
#include "../libbcsstatemachine/bcsstatemachine.h"
#include "../liblinux_util/linux_util.h"

//Не включено в мьютекс, потому что я вызываю эту функцию внутри мьютекса
bool isFree(BCSSERVER_FULL_STATE *state, uint16_t x, uint16_t y){
    int i;

    for (i = 0; i < CLIENTS_NUM; i++){
        if((state->client[i].public_info.state != BCSCLST_FREESLOT)
            && (state->client[i].public_info.position.y == y)
            && (state->client[i].public_info.position.x == x)){
            return false;
        }
    }

    return true;
}

// Player coordinates assignment
int init_start_xy (BCSSERVER_FULL_STATE *state, uint16_t start_x, uint16_t start_y) {
    int i, j;
    int flag = -1;

    pthread_mutex_lock(&state->mutex_self);
    if(flag == -1){
        for (i = 0; i < state->map.height; i++) {
            for (j = 0; j < state->map.width; j++) {
                if ((state->map.map_primitives[i * state->map.width + j] == PUNIT_OPEN_SPACE)
                    && (isFree(state, i, j) == true)) {
                    start_y = i; //y-coordinate
                    start_x = j; //x-coordinate
                    state->map.map_primitives[i * state->map.width + j] = 1;
                    flag = 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&state->mutex_self);

    return flag;
}

// Return current number of clients
// With the introduction of special `count' variable
// this function might become redundant
uint16_t return_clients_size(BCSSERVER_FULL_STATE *state) {
    pthread_mutex_lock(&state->mutex_self);
    uint16_t num = state->player_count;
    pthread_mutex_unlock(&state->mutex_self);

    return num;
}

// Put client data into array
int add_client (BCSSERVER_FULL_STATE *state, struct sockaddr_in *addr_client) {
    struct timeval tv;
    int i;
    int num = -1;

    pthread_mutex_lock(&state->mutex_self);
    for(i = 0; i < state->player_count; i++){
        if(state->client[i].public_info.state == BCSCLST_FREESLOT){
            state->client[i].private_info.endpoint = *addr_client; //client endpoint
            __syscall(gettimeofday(&(state->client[i].private_info.time_last_dgram), NULL)); //set current time as the last response time
            if(init_start_xy(state, state->client[i].public_info.position.x, state->client[i].public_info.position.y) == -1){//init coordinates
                break;
            }
            state->client[i].public_info.state = BCSCLST_CONNECTING; //init state = wait for map
            state->client[i].public_info.direction = BCSDIR_UP; //init direction
            num = i;
            break;
        }
    }
    pthread_mutex_unlock(&state->mutex_self);

    return num;
}

// Search client data in array, returns index of element 
int search_client(BCSSERVER_FULL_STATE *state, struct sockaddr_in *addr_client) {
    int i;
    int num = -1;

    // If endpoint is the same as addr_client
    pthread_mutex_lock(&state->mutex_self);
    for (i = 0; i < CLIENTS_NUM; i++) {
        if ((state->client[i].public_info.state != BCSCLST_FREESLOT)
            && (state->client[i].private_info.endpoint.sin_addr.s_addr == addr_client->sin_addr.s_addr) 
            && (state->client[i].private_info.endpoint.sin_port == addr_client->sin_port) 
            && (state->client[i].private_info.endpoint.sin_family == addr_client->sin_family)) {
            num = i;
            break;
        }
    }
    pthread_mutex_unlock(&state->mutex_self);

    return num;
}

// Remove client from array
// Str1ker, 06.08.2018:
// TODO:
// WARNING:
// ERROR:
// эта функция не производит никаких проверок (??)
// поэтому клиент, не зарегистрированный на сервере, может послать DISCONNECT
// и развалить сервер?!?!
// срочно починить!!!
int delete_client (BCSSERVER_FULL_STATE *state, struct sockaddr_in *addr_client) {
    int num;

    if((num = search_client(state, addr_client)) == -1){
        return -1;
    }
    pthread_mutex_lock(&state->mutex_self);
    state->client[num].public_info.state = BCSCLST_FREESLOT;
    memset(&(state->client[num]), 0, sizeof(BCSCLIENT));
    pthread_mutex_unlock(&state->mutex_self);

    return 0;
}

void log_print_cl_info(BCSSERVER_FULL_STATE *state) {
    int i;

    pthread_mutex_lock(&state->mutex_self);
    for(i = 0; i < CLIENTS_NUM; i++){
        if(state->client[i].public_info.state != BCSCLST_FREESLOT){
            ALOGD("CLIENT %d\n", i);
            //public info 
            ALOGD("state: %d\n", state->client[i].public_info.state);
            ALOGD("position: x = %u, y = %u\n", (unsigned int)state->client[i].public_info.position.x, (unsigned int)state->client[i].public_info.position.y);
            ALOGI("direction: %d\n", state->client[i].public_info.direction);
            //public ext info
            ALOGI("frags: %u\n", (unsigned int)(state->client[i].public_ext_info.frags));
            ALOGI("deaths: %u\n", (unsigned int)(state->client[i].public_ext_info.deaths));
            ALOGI("deaths: %s\n", state->client[i].public_ext_info.nickname);
            //private info
            ALOGI("endpoint : family = %d, port = %d, address = %s\n", state->client[i].private_info.endpoint.sin_family, state->client[i].private_info.endpoint.sin_port, inet_ntoa(state->client[i].private_info.endpoint.sin_addr));
            ALOGI("last fire time: %ld.%06ld\n", state->client[i].private_info.time_last_fire.tv_sec, state->client[i].private_info.time_last_fire.tv_usec);
            ALOGI("last dgram time: %ld.%06ld\n", state->client[i].private_info.time_last_dgram.tv_sec, state->client[i].private_info.time_last_dgram.tv_usec);
        }
    }
    pthread_mutex_unlock(&state->mutex_self);
}

