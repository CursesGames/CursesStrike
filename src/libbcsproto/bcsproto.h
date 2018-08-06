#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <stdint.h> // for known-sized types
#include <stdbool.h>
#include <netinet/in.h>

// this include is important on MIPS for `struct timeval'
// ReSharper disable once CppUnusedIncludeDirective
#include <sys/time.h>

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

#define BCSPROTO_VERSION 0x0002

// beacon packet signature
#define BCSBEACON_MAGIC 0x1324214277da7aff
// длина человекочитаемого имени сервера без '\0':
#define BCSBEACON_DESCRLEN 47

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

// unions to simplify broadcasting
typedef union __bcast_un {
	struct {
		in_addr_t bcast;
		in_addr_t mask;
	} v4;
	uint64_t _vval;
} BCAST_UN;

typedef union __bcast_srv_ep {
	struct __endpoint {
		in_addr_t addr;
		uint16_t port;
		uint16_t zero; // should be 0 after init
	} endpoint;
	uint64_t _vval;
} BCAST_SRV_UN;

// structure for storing coordinates
typedef struct __point {
	uint16_t x;
	uint16_t y;
} __attribute__((packed)) POINT;

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
} BCSACTION;

// direction of the character's movement
typedef enum __bcsdir {
// what about BCSDIR_UNDEF?
// some actions doesn't have direction, but what if I
// want to be strict and disallow all unexistent directions
// except for L, R, U, D?
// Solution: use `BCSDIR_LEFT' where direction does not matter.
	  BCSDIR_LEFT, BCSDIR_UNDEF = BCSDIR_LEFT
	, BCSDIR_RIGHT
	, BCSDIR_UP
	, BCSDIR_DOWN
} BCSDIRECTION;

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
} BCS_REPLY_TYPE;

// possible states of client
typedef enum __bcsclient_state {
// error? initial state?
	  BCSCLST_FREESLOT, BCSCLST_STANDALONE = BCSCLST_FREESLOT
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

// public information about player, server send it to ever clients with ANNOUNCE
// публичная информация об игроке, которую сервер отсылает всем в ANNOUNCE:
typedef struct __bcsclient_info_public {
	BCSCLST state;
	POINT position;
	BCSDIRECTION direction;
} BCSCLIENT_PUBLIC;

// public information about player, which is sent only on request
// открытая информация об игроке, которая отсылается только по запросу
typedef struct __bcsclient_info_public_ext {
	uint16_t frags;
	uint16_t deaths;
	char nickname[BCSPLAYER_NICKLEN + 1]; // 19 + '\0'
} BCSCLIENT_PUBLIC_EXT; // aligned to 24 bytes on x64 and x86, FIXED

// private information, only server know this
// закрытая информация, которую о клиенте знает только сервер
typedef struct __bcsclient_info_private {
	// ip address and port of client
	struct sockaddr_in endpoint;
	// last received packet no
	uint32_t last_packet_no;
	// timestamp of last fire event,
	// to limit fire rate
	struct timeval time_last_fire;
	// timestamp of last received datagram, to kick on connection drop
	struct timeval time_last_dgram;
} BCSCLIENT_PRIVATE;

// TODO: extract fields directly into struct?
// all information about client
// вся информация о клиенте
typedef struct {
	BCSCLIENT_PUBLIC public_info;
	BCSCLIENT_PUBLIC_EXT public_ext_info;
	BCSCLIENT_PRIVATE private_info;
} BCSCLIENT;

typedef union {
	int64_t long_p;
	struct {
		int32_t int_lo;
		int32_t int_hi;
	} ints;
	uint8_t bytes[8];
} BCSMSGPARAM;

// client's message
// сообщение, сгенерированное клиентом
typedef struct __bcsmsg {
// TODO: номер может переполниться, добавить обработку такой ситуации
	uint32_t packet_no;
// accurate to microseconds
	struct timeval time_gen;
// action that the client wants to do
	BCSACTION action;
// additional params - 8 bytes
	BCSMSGPARAM un;
} BCSMSG;

// базовая часть сообщения сервера
typedef struct __bcsmsg_reply {
// number of packet from incoming message
// из приходящего сообщения
	uint32_t packet_no;
// accurate to microseconds
	struct timeval time_gen;
// response type
	BCS_REPLY_TYPE type;
} BCSMSGREPLY;

// эта структура - ЧАСТЬ ответа, идёт после заголовка BCSMSGREPLY
// только в случае type == BCSREPLT_ANNOUNCE
typedef struct __bcsmsg_announce {
	uint16_t count; // количество записей в массивах
// номер записи, соответствующей самому игроку
// спасибо за идею NRshka
	uint16_t index_self;
// all public information
// the very first element [0] is always the client that received the message
// вся публичная информация о клиентах
// самый первый элемeнт [0] всегда тот клиент, который получил сообщение
//	BCSCLIENT_PUBLIC *public_info;
} BCSMSGANNOUNCE;

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
extern ssize_t sendto2(
	int fd, const void *buf, size_t n,
	int flags, struct sockaddr *addr, socklen_t addr_len
);

// read EXACTLY n bytes from TCP connection
// UPD 03.08.2018: this is already can be done by recv() with MSG_WAITALL flag
