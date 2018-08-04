#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>
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

//sign beacon
#define BCSBEACON_MAGIC 0x1324214277da7aff
//длина человекочитаемого имени сервера без '\0':
#define BCSBEACON_DESCRLEN 45
#define BCSSERVER_DEFAULT_PORT 2018

// port for broadcast messages
#define BCSSERVER_BCAST_PORT BCSSERVER_DEFAULT_PORT

// from csds.c
//The maximum size of the buffer to receive a message from the serverЖ
#define BCSDGRAM_MAX 65535
//max length of player's nickname without '\0'
#define BCSPLAYER_NICKLEN 19

// from cs.c
//timeout in seconds to recive datagram from server
#define BCSRECV_TIMEO 1000

// deprecated
#define NICK_SIZE BCSPLAYER_NICKLEN
//#define DEFAULT_PORT BCSSERVER_DEFAULT_PORT

// TODO: export symbols to manipulate protocol datagrams
//structure for storing coordinates
typedef struct __point {
	uint16_t x;
	uint16_t y;
} __attribute__((packed)) POINT;

//all possible actions that the client wants to accomplish
typedef enum __bcsaction {
//connect to server, but without start plaing:
	  BCSACTION_CONNECT // params: nickname: TODO
//if client already connectd start to play
	, BCSACTION_CONNECT2 // noparams
//disconnect from server:
	, BCSACTION_DISCONNECT // noparams
// move without rotation
	, BCSACTION_MOVE // params: BCSDIRECTION
// fire to the current direction
	, BCSACTION_FIRE // noparams
//	, BCSACTION_STRAFE // params: BCSDIRECTION
// rotate around without move
	, BCSACTION_ROTATE // params: BCSDIRECTION (только LEFT или RIGHT)
// request statistics
	, BCSACTION_REQSTATS // noparams
} BCSACTION;

//direction of the character's movement selected by the player
typedef enum __bcsdir {
	  BCSDIR_LEFT
	, BCSDIR_RIGHT
	, BCSDIR_UP
	, BCSDIR_DOWN
} BCSDIRECTION;

//all possible types of server's messages to client
typedef enum __bcsreply_type {
//на всякий случай
	  BCSREPLT_NONE = 0
//operation confirmation:
	, BCSREPLT_ACK
//negative response
	, BCSREPLT_NACK
//message to client about necessity to download the map:
	, BCSREPLT_MAP
//statistic about players (ex. kills/deaths):
	, BCSREPLT_STATS
// send message, initiated by server, to client
// messages of this type will be sent to each client with a server's "frame-rate" frequency
// ANNOUNCE messages contain only a minimal set of public information (BCSCLIENT_PUBLIC)
// we send state cause player can be killed or be connecting
// so, client won't draw character
// TODO: ограничить анонс каждому клиенту его зоной видимости
	, BCSREPLT_ANNOUNCE
// emergency (immediate) message
	, BCSREPLT_EMERGENCY
} BCS_REPLY_TYPE;

//all possible states of client
typedef enum __bcsclient_state {
	  BCSCLST_UNDEF // error?
	, BCSCLST_CONNECTING // downloading map, for ex.
	, BCSCLST_CONNECTED // not playing, spectator
// transition from the "connected" state to the "play" state
// is carried out by sending the message "FIRE"
// ответа на это сообщение нужно дождаться и узнать свои координаты.
// client should wait the responce to know coordinates
// Only after that client starts playing:
	, BCSCLST_PLAYING
	, BCSCLST_RESPAWNING // dead
} BCSCLST;

//публичная информация об игроке, которую сервер отсылает всем в ANNOUNCE_MESSAGE:
//public information about player, server send it to ever clients with ANNOUNCE_MESSAGE:
typedef struct __bcsclient_info_public {
	BCSCLST state;
	POINT position;
	BCSDIRECTION direction;
} BCSCLIENT_PUBLIC;

//открытая информация об игроке, которая отсылается только по запросу:
//public information about the player, which is sent only on request:
typedef struct __bcsclient_info_public_ext {
	uint16_t frags;
	uint16_t deaths;
	char nickname[BCSPLAYER_NICKLEN + 1]; // 19 + '\0'
} BCSCLIENT_PUBLIC_EXT; // aligned to 24 bytes on x64 and x86, FIXED

//private information, only server knew this
//закрытая информация, которую о клиенте знает только сервер
typedef struct __bcsclient_info_private {
	//ip address and port of client
	struct sockaddr_in endpoint;
	//time of last fire, 
	//to simulate rate of fire of weapn
	struct timeval time_last_fire;
	//time of last datagram from client to disctonnect AFK
	struct timeval time_last_dgram;
} BCSCLIENT_PRIVATE;

//вся информация о клиенте:
//all information about client
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
	uint8_t bytes[8]; //для выравнивания
} BCSMSGPARAM;

// client's message
typedef struct __bcsmsg {
// TODO: номер может переполниться, добавить обработку такой ситуации
	int32_t packet_no;
//action that the client wants to accomplish
	BCSACTION action;
// accurate to microseconds
	struct timeval time_gen;
// additional params - 8 bytes
	BCSMSGPARAM un;
} BCSMSG;

// базовая часть сообщения сервера
typedef struct __bcsmsg_reply {
	//number of packet from coming message
	int32_t packet_no;
	//type of response
	BCS_REPLY_TYPE type;
} BCSMSGREPLY;

// эта структура - ЧАСТЬ ответа, идёт после заголовка BCSMSGREPLY
// только в случае type == BCSREPLT_ANNOUNCE
typedef struct __bcsmsg_announce {
	uint16_t count; // количество записей в массивах
// вся публичная информация о клиентах
//самый первый элемeнт ([0]) всегда тот клиент, который получил сообщение
//all public information
//the very first element ([0]) is always the client that received the message
	BCSCLIENT_PUBLIC *public_info;
} BCSMSGANNOUNCE;

// The structure of the broadcast message from the server
typedef struct __bcs_beacon {
// константа: BCSBEACON_MAGIC
	uint64_t magic;
// server port. The IP address will be automatically extracted from the broadcast message
	uint16_t port;
// string with the human-readable name of the server
	char description[BCSBEACON_DESCRLEN + 1];
} BCSBEACON;

// unified interface. good to think about it.
extern bool bcsproto_send(int sockfd, struct sockaddr_in *client_endpoint_to, BCSMSG *msg);
extern bool bcsproto_recv(int sockfd, struct sockaddr_in *client_endpoint_from, BCSMSG *msg);
extern bool bcsproto_validate_message(BCSMSG *msg);

// for simpler message generation
extern uint32_t bcsproto_next_packet_no;

// this function is not reentrant (and so not thread-safe)
//устанавливает номер сообщения, инкрементирует для следующего и заполняет время сообщения
extern void bcsproto_new_packet(BCSMSG *msg);

// this function is a clone of `sendto' for packet duplication
extern ssize_t sendto2(
	int fd, const void *buf, size_t n,
	int flags, struct sockaddr *addr, socklen_t addr_len
);

// read EXACTLY n bytes from TCP connection
// or return -1 if it's impossible.
// In this condition, actual read count is returned into n
//extern ssize_t recv4(int fd, void *buf, size_t *n, int flags);
