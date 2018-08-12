#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <stdint.h> // for known-sized types
#include <stdbool.h>
#include <netinet/in.h>

// this include is important on MIPS for `timeval128_t'
// ReSharper disable once CppUnusedIncludeDirective
#include <sys/time.h>
#include "../liblinux_util/linux_util.h"
#include "timeval16.h"

///////////////////////////////////////////////////
//          ДОГОВОРЁННОСТЬ ПО ПРОТОКОЛУ          //
// Все числа длиной более байта (2, 4, 8 байт)   //
// передавать по сети в формате Big-Endian!!!    //
// Для преобразования в Big-Endian использовать: //
//   htobe16(), htobe32(), htobe64()             //
// Для обратного преобразования числа,           //
// полученного из сети, в нативный вид:          //
//   be16toh(), be32toh(), be64toh()             //
///////////////////////////////////////////////////

#define BCSPROTO_VERSION 0x0003

// beacon packet signature
#define BCSBEACON_MAGIC 0x1324214277da7aff
// длина человекочитаемого имени сервера без '\0':
#define BCSBEACON_DESCRLEN 51

// default server port, what's unclear?
#define BCSSERVER_DEFAULT_PORT 2018

// port for broadcast messages
// do broadcast to the same port as the game defaults to
// there's nothing bad cuz we have to bind to broadcast addr to receive beacons
#define BCSSERVER_BCAST_PORT BCSSERVER_DEFAULT_PORT

// max size of data structures for client list
// this is define and not a variable for optimization
#define BCSSERVER_MAXCLIENTS 16

// how many times the same datagram will be sent
// values on the client and the server may vary
#define BCSPROTO_PACKETDUP 2

// from csds.c
// maximum size of the buffer to receive a datagram
#define BCSDGRAM_MAX 65535
// max length of player's nickname without '\0'
#define BCSPLAYER_NICKLEN 19

// from cs.c
// timeout in milliseconds to receive datagram
#define BCSRECV_TIMEO 1000

// deprecated
#define NICK_SIZE BCSPLAYER_NICKLEN
#define CLIENTS_NUM BCSSERVER_MAXCLIENTS

STATIC_ASSERT(sizeof(timeval128_t) == 16);
STATIC_ASSERT(sizeof(struct sockaddr_in) == 16);

// unions to simplify broadcasting
typedef union __bcast_un {
    struct {
        in_addr_t bcast;
        in_addr_t mask;
    } v4;
    uint64_t _vval;
} BCAST_UN;
STATIC_ASSERT(sizeof(BCAST_UN) == 8);

typedef union __bcast_srv_ep {
    struct __endpoint {
        in_addr_t addr;
        uint16_t port;
        uint16_t zero; // should be 0 after init
    } endpoint;
    uint64_t _vval;
} BCAST_SRV_UN;
STATIC_ASSERT(sizeof(BCAST_SRV_UN) == 8);

// structure for storing coordinates
typedef struct __point {
    uint16_t x;
    uint16_t y;
} __attribute__((packed)) POINT;
STATIC_ASSERT(sizeof(POINT) == 4);

// all possible actions that the client may request
typedef enum __bcsaction {
    // "connect" to server and reserve slot
    BCSACTION_CONNECT // params: version (4 bytes), nickname
    // confirm that client got the map and ready to play
  , BCSACTION_CONNECT2 // noparams
    // disconnect from server
  , BCSACTION_DISCONNECT // noparams
    // move without rotation
  , BCSACTION_MOVE // params: BCSDIRECTION
    // fire to the current direction
  , BCSACTION_FIRE // noparams
    // rotate around without move
  , BCSACTION_ROTATE // params: BCSDIRECTION (только LEFT или RIGHT)
    // request statistics
  , BCSACTION_REQSTATS // noparams
    // tell the server that client is alive
  , BCSACTION_KEEPALIVE
} BCSACTION;
STATIC_ASSERT(sizeof(BCSACTION) == 4);

// direction of the character's movement
typedef enum __bcsdir {
    // what about BCSDIR_UNDEF?
    // some actions doesn't have direction, but what if I
    // want to be strict and disallow all unexistent directions
    // except for L, R, U, D?
    // Solution: use `BCSDIR_LEFT' where direction does not matter.
    BCSDIR_LEFT
  , BCSDIR_UNDEF = BCSDIR_LEFT
  , BCSDIR_UP
  , BCSDIR_RIGHT
  , BCSDIR_DOWN
} BCSDIRECTION;
STATIC_ASSERT(sizeof(BCSDIRECTION) == 4);

// possible types of server replies
typedef enum __bcsreply_type {
    // на всякий случай
    BCSREPLT_NONE = 0
    // acknowledge - client request accepted
  , BCSREPLT_ACK
    // negative acknowledge - client request denied
  , BCSREPLT_NACK
    // message to client about necessity to download the map
  , BCSREPLT_MAP
    // stats about players (ex. kills/deaths)
  , BCSREPLT_STATS
    // send message, initiated by server, to client
    // messages of this type will be sent to each client with a server's "frame-rate" frequency
    // ANNOUNCE messages contain only a minimal set of public information (BCSCLIENT_PUBLIC)
    // we send state cause some player can be killed or be connecting
    // so, client won't draw this player
    // сообщения такого типа будут рассылаться каждому клиенту с частотой "фреймрейта" сервера
    // (скорее всего, это 30 раз в секунду)
    // сообщения ANNOUNCE содержат только минимальный набор публичной информации: 
    // расположение, направление, состояние.
    // состояние отправляем на тот хрен, что чувака могли убить, или он ещё не подключился
    // в таком случае его не нужно рисовать
    // TODO: ограничить анонс каждому клиенту его зоной видимости
  , BCSREPLT_ANNOUNCE
    // emergency (immediate) message
    // экстренное (немедленное) сообщение
  , BCSREPLT_EMERGENCY
    // выключение сервера
  , BCSREPLT_SHUTDOWN
} BCS_REPLY_TYPE;
STATIC_ASSERT(sizeof(BCS_REPLY_TYPE) == 4);

// possible states of client
typedef enum __bcsclient_state {
    // error? initial state?
    BCSCLST_FREESLOT
  , BCSCLST_STANDALONE = BCSCLST_FREESLOT
    // registered on the server but downloading map, for ex. and don't receive announces
  , BCSCLST_CONNECTING
    // receives announces but not playing, spectator mode
  , BCSCLST_CONNECTED
    // transition from the "connected" state to the "playing" state
    // is done by sending message "FIRE"
    // переход из состояния "подключен" в состояние "играет" 
    // осуществляется посылкой сообщения "FIRE"
    // UPD. 05.08.2018: игрок переходит в это состояние немедленно
    // а свои координаты узнает из ближайшего анонса
  , BCSCLST_PLAYING
    // dead
  , BCSCLST_RESPAWNING
} BCSCLST;
STATIC_ASSERT(sizeof(BCSCLST) == 4);

// public information about player, server send it to ever clients with ANNOUNCE
// публичная информация об игроке, которую сервер отсылает всем в ANNOUNCE:
typedef struct __bcsclient_info_public {
    BCSCLST state;
    POINT position;
    BCSDIRECTION direction;
    char _pad[4];
} BCSCLIENT_PUBLIC;
STATIC_ASSERT(sizeof(BCSCLIENT_PUBLIC) == 16);

// public information about player, which is sent only on request
// открытая информация об игроке, которая отсылается только по запросу
typedef struct __bcsclient_info_public_ext {
    uint16_t frags;
    uint16_t deaths;
    char nickname[BCSPLAYER_NICKLEN + 1]; // 19 + '\0'
} BCSCLIENT_PUBLIC_EXT; // aligned to 24 bytes on x64 and x86, FIXED
STATIC_ASSERT(sizeof(BCSCLIENT_PUBLIC_EXT) == 24);

// private information, only server know this
// закрытая информация, которую о клиенте знает только сервер
typedef struct __bcsclient_info_private {
    // timestamp of last fire event, to limit fire rate
    timeval128_t time_last_fire;
    // timestamp of last received datagram, to kick on connection drop
    timeval128_t time_last_dgram;
    // timestamp of last move event, to limit speed
    timeval128_t time_last_move;
    // timestamp of last move/rotate/fire event
    timeval128_t time_last_activity;
    // ip address and port of client
    struct sockaddr_in endpoint; // 16 bytes
    // last received packet no
    uint32_t last_packet_no;
    //char _pad[4];
} BCSCLIENT_PRIVATE;
STATIC_ASSERT(sizeof(BCSCLIENT_PRIVATE) == 88);

// TODO: extract fields directly into struct?
// all information about client
// вся информация о клиенте
typedef struct {
    BCSCLIENT_PUBLIC public_info; // 16
    BCSCLIENT_PUBLIC_EXT public_ext_info; // 24
    BCSCLIENT_PRIVATE private_info; // 72
} BCSCLIENT;
STATIC_ASSERT(sizeof(BCSCLIENT) == 128);

typedef union {
    int64_t lng;
    struct {
        int32_t int_lo; // первый параметр
        int32_t int_hi; // второй параметр
    } ints;
    uint8_t bytes[8];
} BCSMSGPARAM;
STATIC_ASSERT(sizeof(BCSMSGPARAM) == 8);

// client's message
// сообщение, сгенерированное клиентом
typedef struct __bcsmsg {
    // accurate to microseconds
    timeval128_t time_gen;
    // TODO: номер может переполниться, добавить обработку такой ситуации
    uint32_t packet_no;
    // action that the client wants to do
    BCSACTION action;
    // additional params - 8 bytes
    BCSMSGPARAM un;
} BCSMSG;
STATIC_ASSERT(sizeof(BCSMSG) == 32);

// базовая часть сообщения сервера
typedef struct __bcsmsg_reply {
    // accurate to microseconds
    timeval128_t time_gen;
    // number of packet from incoming message
    // из приходящего сообщения
    uint32_t packet_no;
    // response type
    BCS_REPLY_TYPE type;
} BCSMSGREPLY;
STATIC_ASSERT(sizeof(BCSMSGREPLY) == 24);

// эта структура - ЧАСТЬ ответа, идёт после заголовка BCSMSGREPLY
// только в случае type == BCSREPLT_ANNOUNCE
typedef struct __bcsmsg_announce {
    // количество записей в массивах
    uint16_t count;
    // количество пуль в анонсе
    uint16_t count_bullets;
    // номер записи, соответствующей самому игроку
    // спасибо за идею NRshka
    uint16_t index_self;
    // all public information
    // the very first element [0] is always the client that received the message
    // вся публичная информация о клиентах
    // самый первый элемeнт [0] всегда тот клиент, который получил сообщение
    //  BCSCLIENT_PUBLIC *public_info;
    //char _pad[2];
    uint16_t _zero;
} BCSMSGANNOUNCE;
STATIC_ASSERT(sizeof(BCSMSGANNOUNCE) == 8);

// structure of the broadcast message from the server
// структура широковещательного сообщения от сервера
typedef struct __bcs_beacon {
    // константа: BCSBEACON_MAGIC
    uint64_t magic;
    // server port. The IP address will be automatically extracted from the broadcast message
    // порт сервера. IP адрес будет вычленен автоматически из broadcast-сообщения
    uint16_t port;
    // версия протокола сервера, вкомпиливается в бинарник из константы BCSPROTO_VERSION
    uint16_t proto_ver;
    // string with the human-readable name of the server
    // строка с человекочитаемым названием сервера
    char description[BCSBEACON_DESCRLEN + 1];
} BCSBEACON;
STATIC_ASSERT(sizeof(BCSBEACON) == 64);

typedef struct __bcs_bullet {
    uint16_t x;
    uint16_t y;
    BCSDIRECTION direction;
    uint16_t creator_id;
    uint16_t _zero;
    //char _pad[6];
} BCSBULLET;
STATIC_ASSERT(sizeof(BCSBULLET) == 12);

// unified interface. good to think about it.
extern bool bcsproto_send(int sockfd, struct sockaddr_in *client_endpoint_to, BCSMSG *msg);
extern bool bcsproto_recv(int sockfd, struct sockaddr_in *client_endpoint_from, BCSMSG *msg);
extern bool bcsproto_validate_message(BCSMSG *msg);

// for simpler message generation
extern uint32_t bcsproto_next_packet_no;

// this function is not reentrant (and so not thread-safe)
// "проштамповывает" сообщение:
// устанавливает номер и время, инкрементирует номер для следующего
extern void bcsproto_new_packet(BCSMSG *msg);

// this function is a clone of `sendto' for packet duplication
extern ssize_t sendto2(int fd, const void *buf, size_t n, int flags, struct sockaddr *addr
                     , socklen_t addr_len);

// read EXACTLY n bytes from TCP connection
// UPD 03.08.2018: this is already can be done by recv() with MSG_WAITALL flag
