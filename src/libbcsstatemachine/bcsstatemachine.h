#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdbool.h>
#include <pthread.h>
#include <ncursesw/ncurses.h> // just to export window type, nothing personal
#include "../libbcsproto/bcsproto.h"
#include "../libbcsmap/bcsmap.h"
#include "../libvector/vector.h"
#include "../liblinkedlist/linkedlist.h"

// тупой C требует указания типа объекта, с**а
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr sockaddr;

// I don't bother with packing of this structure now
// структура состояния клиента на клиенте
typedef struct {
	BCSCLIENT_PUBLIC self;
	BCSCLIENT_PUBLIC_EXT self_ext;
	struct {
		uint16_t count;
		uint16_t index_self;
		BCSCLIENT_PUBLIC array[BCSSERVER_MAXCLIENTS];
	} others;
	pthread_mutex_t mutex_self; // this struct data exclusive access
	pthread_mutex_t mutex_frame; // ncurses view exclusive access
	pthread_mutex_t mutex_sock; // udp socket exclusive access
	struct sockaddr_in endpoint; // Server IP:Port
	int sockfd; // UDP socket
	BCSMAP map;
	BCSMAP map_overlay;
	WINDOW *mappad;
	WINDOW *below;
	struct {
		uint16_t count;
		BCSBULLET *array;

	} bullets;
	size_t frames;
} BCSPLAYER_FULL_STATE;

// структура состояния сервера = состояние клиентов на сервере
typedef struct {
// тупой статический массив на столько слотов, на сколько можем
// при инициализации нужно закатать нулями, 
// тогда state и endpoint будут как у свободного слота
	BCSCLIENT client[BCSSERVER_MAXCLIENTS];
	pthread_mutex_t mutex_self; // this struct data exclusive access
	pthread_mutex_t mutex_sock; // main udp socket exclusive access
	int sock_u; // main UDP socket descriptor
	int sock_t; // TCP socket
	VECTOR sock_b; // vector of broadcast sockets
	BCSMAP map;
// объекты изменившие вид карты: разрушенные ящики, пули могут храниться здесь
	BCSMAP map_overlay;
	LINKED_LIST bullets;
// количество фреймов сервера, для статистики какой-нибудь
	size_t frames;
// текущее количество подключенных клиентов, чтобы каждый раз не гонять массив
	uint16_t player_count;
} BCSSERVER_FULL_STATE;

// что должна делать эта функция?
// 1. читать сообщение msg (это всегда именно сообщение от клиента)
// 2. перед любым доступом (r/w) к state блокировать в нём же находящийся мьютекс:
//    pthread_mutex_lock(&state->mutex_self);
// 3. проверить возможность запрошенного действия и изменить state если да
// 4. разблокировать мьютекс
// 5. если запрос подразумевает обязательный ответ (на спам/флуд от анонимов не отвечаем):
//    - pthread_mutex_lock(&state->mutex_sock);
//    - sendto() тому кто прислал msg
//    - pthread_mutex_unlock(&state->mutex_sock);
//    (P.S.: Str1ker: не знаю на самом деле требуется ли синхронизация доступа к сокету,
//      но прошу сделать так)
// Критическая секция с заблокированным mutex_self должна работать максимально быстро.
// Вернуть true если запрос возможен и выполнен, иначе false.
extern bool bcsstatemachine_process_request(
	  BCSSERVER_FULL_STATE *state
	, sockaddr_in *src
	, BCSMSG *msg
	, ssize_t msglen
);
