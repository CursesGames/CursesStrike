#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "clientarray.h"
#include "../libbcsstatemachine/bcsstatemachine.h"
#include "../liblinux_util/linux_util.h"

// Player coordinates assignment
void init_start_xy (BCSMAP *map, uint16_t start_x, uint16_t start_y) {
    int i, j;

    for (i = 0; i < map->height; i++) {
        for (j = 0; j < map->width; j++) {
            if (map->map_primitives[i * map->width + j] == 0) {
                start_y = i; //y-coordinate
                start_x = j; //x-coordinate
                map->map_primitives[i * map->width + j] = 1;
                return;
            }
        }
    }
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
int add_client (BCSMAP *map, BCSCLIENT *clients, struct sockaddr_in addr_client) {
    struct timeval tv;
    int i;

    for(i = 0; i < CLIENTS_NUM; i++){
        if(clients[i].private_info.endpoint.sin_addr.s_addr == 0){
            clients[i].private_info.endpoint = addr_client; //client endpoint
            __syscall(gettimeofday(&(clients[i].private_info.time_last_dgram), NULL)); //set current time as the last response time
            init_start_xy(map, clients[i].public_info.position.x, clients[i].public_info.position.y); //init coordinates
            clients[i].public_info.state = BCSCLST_CONNECTING; //init state = wait for map
            clients[i].public_info.direction = BCSDIR_UP; //init direction
            return i;
        }
    }

    return -1;
}

// Search client data in array, returns index of element 
int search_client(BCSCLIENT *clients, struct sockaddr_in addr_client) {
    int i;
    int num;

    // If endpoint is the same as addr_client
    for (i = 0; i < CLIENTS_NUM; i++) {
        if ((clients[i].private_info.endpoint.sin_addr.s_addr == 0) && 
            (clients[i].private_info.endpoint.sin_port == addr_client.sin_port) && 
            (clients[i].private_info.endpoint.sin_family == addr_client.sin_family)) {
            num = i;
            return num;
        }
    }

    return -1;
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
void delete_client (BCSCLIENT *clients, struct sockaddr_in addr_client) {
    int num;

    num = search_client(clients, addr_client);
    memset(clients + num*sizeof(BCSCLIENT), 0, sizeof(BCSCLIENT));
}

void log_print_cl_info(BCSCLIENT *clients) {
    int i;

    for(i = 0; i < CLIENTS_NUM; i++){
        if(clients[i].private_info.endpoint.sin_addr.s_addr != 0){
            ALOGD("CLIENT %d\n", i);
            //public info 
            ALOGD("state: %d\n", clients[i].public_info.state);
            ALOGD("position: x = %u, y = %u\n", (unsigned int)clients[i].public_info.position.x, (unsigned int)clients[i].public_info.position.y);
            ALOGI("direction: %d\n", clients[i].public_info.direction);
            //public ext info
            ALOGI("frags: %u\n", (unsigned int)(clients[i].public_ext_info.frags));
            ALOGI("deaths: %u\n", (unsigned int)(clients[i].public_ext_info.deaths));
            ALOGI("deaths: %s\n", clients[i].public_ext_info.nickname);
            //private info
            ALOGI("endpoint : family = %d, port = %d, address = %s\n", clients[i].private_info.endpoint.sin_family, clients[i].private_info.endpoint.sin_port, inet_ntoa(clients[i].private_info.endpoint.sin_addr));
            ALOGI("last fire time: %ld.%06ld\n", clients[i].private_info.time_last_fire.tv_sec, clients[i].private_info.time_last_fire.tv_usec);
            ALOGI("last dgram time: %ld.%06ld\n", clients[i].private_info.time_last_dgram.tv_sec, clients[i].private_info.time_last_dgram.tv_usec);
        }

    }
}
