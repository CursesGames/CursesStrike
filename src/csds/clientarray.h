#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>
#include "../libbcsmap/bcsmap.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

void init_start_xy (BCSMAP *map, uint16_t start_x, uint16_t start_y);
uint16_t return_clients_size(BCSSERVER_FULL_STATE *state);
int add_client (BCSMAP *map, BCSCLIENT *clients, struct sockaddr_in addr_client);
int search_client(BCSCLIENT *clients, struct sockaddr_in addr_client);
void delete_client (BCSCLIENT *clients, struct sockaddr_in addr_client);
void log_print_cl_info(BCSCLIENT *clients);
