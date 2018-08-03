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

#define BCSBEACON_MAGIC 0x1324214277da7aff
#define BCSBEACON_DESCRLEN 45
#define BCSSERVER_DEFAULT_PORT 2018

// broadcast to the same port
#define BCSSERVER_BCAST_PORT BCSSERVER_DEFAULT_PORT

// from csds.c
#define BCSDGRAM_MAX 65535
#define BCSPLAYER_NICKLEN 19

// from cs.c
#define BCSRECV_TIMEO 1000

// deprecated
#define NICK_SIZE BCSPLAYER_NICKLEN
//#define DEFAULT_PORT BCSSERVER_DEFAULT_PORT

// TODO: export symbols to manipulate protocol datagrams
typedef struct __point {
	uint16_t x;
	uint16_t y;
} __attribute__((packed)) POINT;

typedef enum __bcsaction {
	  BCSACTION_CONNECT // params: nickname: TODO
	, BCSACTION_CONNECT2 // noparams
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

typedef enum __bcsdir {
	  BCSDIR_LEFT
	, BCSDIR_RIGHT
	, BCSDIR_UP
	, BCSDIR_DOWN
} BCSDIRECTION;

typedef enum __bcsreply_type {
	  BCSREPLT_NONE = 0
	, BCSREPLT_ACK
	, BCSREPLT_NACK
	, BCSREPLT_MAP
	, BCSREPLT_STATS
// send message, initiated by server, to client
// сообщения такого типа будут рассылаться каждому клиенту с частотой "фреймрейта" сервера
// (скорее всего, это 30 раз в секунду)
// сообщения ANNOUNCE содержат только минимальный набор публичной информации: 
// расположение, направление, состояние.
// состояние отправляем на тот хрен, что чувака могли убить, или он ещё не подключился
// в таком случае его не нужно рисовать
// TODO: ограничить анонс каждому клиенту его зоной видимости
	, BCSREPLT_ANNOUNCE
// экстренное (немедленное) сообщение
	, BCSREPLT_EMERGENCY
} BCS_REPLY_TYPE;

typedef enum __bcsclient_state {
	  BCSCLST_UNDEF // error?
	, BCSCLST_CONNECTING // downloading map, for ex.
	, BCSCLST_CONNECTED // not playing, spectator
// переход из состояния "подключен" в состояние "играет" 
// осуществляется посылкой сообщения "FIRE"
// ответа на это сообщение нужно дождаться и узнать свои координаты.
// только тогда игрок включается в игру
	, BCSCLST_PLAYING
	, BCSCLST_RESPAWNING // dead
} BCSCLST;

typedef struct __bcsclient_info_public {
	BCSCLST state;
	POINT position;
	BCSDIRECTION direction;
} BCSCLIENT_PUBLIC;

typedef struct __bcsclient_info_public_ext {
	uint16_t frags;
	uint16_t deaths;
	char nickname[BCSPLAYER_NICKLEN + 1]; // 19 + '\0'
} BCSCLIENT_PUBLIC_EXT; // aligned to 24 bytes on x64 and x86, FIXED

typedef struct __bcsclient_info_private {
	struct sockaddr_in endpoint;
	struct timeval time_last_fire;
	struct timeval time_last_dgram;
} BCSCLIENT_PRIVATE;

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
	uint8_t bytes[8]; // 
} BCSMSGPARAM;

// сообщение, сгенерированное клиентом
typedef struct __bcsmsg {
// TODO: номер может переполниться, добавить обработку такой ситуации
	int32_t packet_no;
	BCSACTION action;
// accurate to microseconds
	struct timeval time_gen;
// additional params - 8 bytes
	BCSMSGPARAM un;
} BCSMSG;

// базовая часть сообщения сервера
typedef struct __bcsmsg_reply {
	int32_t packet_no; //из приходящего сообщения
	BCS_REPLY_TYPE type;
} BCSMSGREPLY;

// эта структура - ЧАСТЬ ответа, идёт после заголовка BCSMSGREPLY
// только в случае type == BCSREPLT_ANNOUNCE
typedef struct __bcsmsg_announce {
	uint16_t count; // количество записей в массивах
// 0-й элемент анонса - всегда сам игрок
	BCSCLIENT_PUBLIC *public_info;
} BCSMSGANNOUNCE;

// Структура широковещательного сообщения от сервера
typedef struct __bcs_beacon {
// константа: BCSBEACON_MAGIC
	uint64_t magic;
// порт сервера. IP адрес будет вычленен автоматически из broadcast-сообщения
	uint16_t port;
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
