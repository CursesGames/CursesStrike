#pragma once
#include "../liblinux_util/mscfix.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>
#include <string.h>

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <ifaddrs.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <linux/if_link.h>

#include "../libncurses_util/ncurses_util.h"
#include "../liblinux_util/linux_util.h"
#include "../libbcsmap/bcsmap.h"
#include "../libbcsproto/bcsproto.h"
#include "../libvector/vector.h"
#include "../libbcsstatemachine/bcsstatemachine.h"

BCSMSG all_possible[7];
BCSMSG msg_disconnect;
BCSMSG msg_connect2;
BCSMSG msg_reqstat;
BCSMSG msg_move;
BCSMSG msg_fire;
BCSMSG msg_rotate;
BCSMSG msg_connect;

BCSMSGREPLY* reply;
BCSBEACON* try_beacon;
struct sockaddr_in serv;
socklen_t size_serv = sizeof(serv);

char* names_reply[7];
char* names_resp[7];

char buf[BCSDGRAM_MAX];