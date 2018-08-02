#pragma once

// Note: this include is a beta feature for design- and compile-time
#include "../liblinux_util/mscfix.h"

#include <stdint.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdbool.h>

#define BCSBEACON_MAGIC 0x1324214277da7aff
#define BCSBEACON_DESCRLEN 45
#define DEFAULT_PORT 2018

// from csds.c
#define BCSPLAYER_NICKLEN 20
// deprecated
#define NICK_SIZE BCSPLAYER_NICKLEN

// TODO: export symbols to manipulate protocol datagrams
typedef struct __point {
	uint16_t x;
	uint16_t y;
} __attribute__((packed)) POINT;

typedef enum __bcsaction {
	  BCSACTION_CONNECT // params: nickname: TODO
	, BCSACTION_CONNECT2 // noparams
	, BCSACTION_DISCONNECT // noparams
	, BCSACTION_MOVE // params: BCSDIRECTION
	, BCSACTION_FIRE // noparams
// move without rotation
	, BCSACTION_STRAFE // params: BCSDIRECTION
// rotation without move
	, BCSACTION_ROTATE // params: BCSDIRECTION
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
	char nickname[BCSPLAYER_NICKLEN + 1]; // 20 + '\0'
} BCSCLIENT_PUBLIC_EXT; // aligned to 32 bytes on x64, FIXME

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

// сообщение, сгенерированное клиентом
typedef struct __bcsmsg {
	int32_t packet_no; // TODO: номер может переполниться, добавить обработку такой ситуации
	BCSACTION action;
// accurate to microseconds
	struct timeval time_gen;
// additional params - 8 bytes
	union {
		int64_t long_p;
		struct {
			int32_t int_lo;
			int32_t int_hi;
		} ints;
		uint8_t bytes[8]; // 
	} un;
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

typedef struct __bcs_beacon {
// константа: BCSBEACON_MAGIC
	uint64_t magic;
// порт сервера. IP адрес будет вычленен автоматически из broadcast-сообщения
	uint16_t port;
// строка с человекочитаемым названием сервера
	char description[BCSBEACON_DESCRLEN + 1];
} BCSBEACON;

bool bcsproto_send(int sockfd, struct sockaddr_in *client_endpoint_to, BCSMSG *msg);
bool bcsproto_recv(int sockfd, struct sockaddr_in *client_endpoint_from, BCSMSG *msg);
bool bcsproto_validate_message(BCSMSG *msg);
