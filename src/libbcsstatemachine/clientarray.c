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

    for (i = 0; i < BCSSERVER_MAXCLIENTS; i++){
        if((state->client[i].public_info.state != BCSCLST_FREESLOT)
            && (state->client[i].public_info.position.y == y)
            && (state->client[i].public_info.position.x == x)){
            return false;
        }
    }

    return true;
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
    int i, j, k;
    int x, y;
    int num = -1, flag = -1;

    pthread_mutex_lock(&state->mutex_self);
    // init coordinates
    //return из вложенного цикла не сделаешь, поэтому flag
    for (j = 0; j < state->map.height; j++) {
        for (k = 0; k < state->map.width; k++) {
            if((flag == -1)
                &&(state->map.map_primitives[j * state->map.width + k] == PUNIT_OPEN_SPACE)
                && (isFree(state, k, j) == true)) {
                x = k;
                y = j;
                flag = 0;
            }
        }
    }
    if(flag == 0){
        for(i = 0; i < BCSSERVER_MAXCLIENTS; i++){
            if((state->client[i].public_info.state) == BCSCLST_FREESLOT){
                state->client[i].private_info.endpoint = *addr_client; // client endpoint
                __syscall(gettimeofday(&(state->client[i].private_info.time_last_dgram), NULL)); 
                state->client[i].public_info.state = BCSCLST_CONNECTING; // init state = wait for map
                state->client[i].public_info.direction = BCSDIR_UP; // init direction
                state->client[i].public_info.position.y = y; // y-coordinate
                state->client[i].public_info.position.x = x; // x-coordinate
                num = i;
                break;
            }
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
    for (i = 0; i < BCSSERVER_MAXCLIENTS; i++) {
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
    memset(&state->client[num], 0, sizeof(BCSCLIENT));
    pthread_mutex_unlock(&state->mutex_self);

    return 0;
}

void log_print_cl_info(BCSSERVER_FULL_STATE *state) {
    int i;

    pthread_mutex_lock(&state->mutex_self);
    for(i = 0; i < BCSSERVER_MAXCLIENTS; i++){
        if(state->client[i].public_info.state != BCSCLST_FREESLOT){
            ALOGD("CLIENT %d\n", i);
            //public info 
            ALOGD("state: %d\n", state->client[i].public_info.state);
            ALOGD("position: x = %u, y = %u\n", 
                (unsigned int)state->client[i].public_info.position.x, 
                (unsigned int)state->client[i].public_info.position.y);
            ALOGI("direction: %d\n", state->client[i].public_info.direction);
            //public ext info
            ALOGI("frags: %u\n", (unsigned int)(state->client[i].public_ext_info.frags));
            ALOGI("deaths: %u\n", (unsigned int)(state->client[i].public_ext_info.deaths));
            ALOGI("nickname: %s\n", state->client[i].public_ext_info.nickname);
            //private info
            ALOGI("endpoint : family = %d, port = %d, address = %s\n", 
                state->client[i].private_info.endpoint.sin_family, 
                state->client[i].private_info.endpoint.sin_port, 
                inet_ntoa(state->client[i].private_info.endpoint.sin_addr));
            ALOGI("last fire time: %ld.%06ld\n", 
                state->client[i].private_info.time_last_fire.tv_sec, 
                state->client[i].private_info.time_last_fire.tv_usec);
            ALOGI("last dgram time: %ld.%06ld\n", 
                state->client[i].private_info.time_last_dgram.tv_sec, 
                state->client[i].private_info.time_last_dgram.tv_usec);
        }
    }
    pthread_mutex_unlock(&state->mutex_self);
}

