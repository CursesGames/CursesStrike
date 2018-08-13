// Note: this include is a beta feature for design- and compile-time
#include "imit.h"

// catching broadcast messages timeout
#define BCAST_SCAN_TIMEOUT_SEC 4
// assume no more than 3 good ifaces by default
#define BCSIFACES_APPROX 3
// assume no more than 10 good servers by default
#define BCSSERVERS_APPROX 10

// WARNING: this is a workaround for dumb C libraries like musl
typedef union sigval sigval_t;

// taken from Bionicle Commander
typedef enum {
// white-on-black default terminal scheme
	  CPAIR_DEFAULT = 1
// map primitives
	, CPAIR_CELL_BLANK = CPAIR_DEFAULT
	, CPAIR_CELL_WALL
	, CPAIR_CELL_CRATE
	, CPAIR_CELL_WATER
// players
	, CPAIR_PLAYER_SELF
	, CPAIR_PLAYER_FRIENDLY
	, CPAIR_PLAYER_ENEMY
	, CPAIR_PLAYER_NPC
} cpair_list;

// look at bcsproto.h:
//typedef enum __bcsdir {
//	  BCSDIR_LEFT
//	, BCSDIR_RIGHT
//	, BCSDIR_UP
//	, BCSDIR_DOWN
//} BCSDIRECTION;
FILE* log_file = NULL;
uint64_t count_threads;
pthread_mutex_t ctm;

typedef BCSBEACON BCAST_SRV;
#define MAXBUF 65535

size_t init_broadcast_receiver(VECTOR *ipv4_faces) {
	char addrstr[INET_ADDRSTRLEN];
	in_addr_t ifaddr;
	BCAST_UN un;
	size_t count = 0;
	struct ifaddrs *ifap_head;
	__syswrap(getifaddrs(&ifap_head));
	struct ifaddrs *ifap = ifap_head;
	lassert(vector_init(ipv4_faces, BCSIFACES_APPROX));
	while(ifap != NULL) {
		if(ifap->ifa_addr != NULL 
			&& ifap->ifa_addr->sa_family == AF_INET 
			&& ifap->ifa_flags & IFLA_BROADCAST
		) {
			ALOGD("iface: %s\n", ifap->ifa_name);
			ifaddr = ((struct sockaddr_in*)ifap->ifa_addr)->sin_addr.s_addr;
			lassert(inet_ntop(AF_INET, &ifaddr, addrstr, INET_ADDRSTRLEN));
			ALOGD("addr: %s\t", addrstr);
			un.v4.mask = ((struct sockaddr_in*)ifap->ifa_netmask)->sin_addr.s_addr;
			lassert(inet_ntop(AF_INET, &(un.v4.mask), addrstr, INET_ADDRSTRLEN));
			logprint("mask: %s\t", addrstr);

			un.v4.bcast = ((struct sockaddr_in*)ifap->ifa_ifu.ifu_broadaddr)->sin_addr.s_addr;
			if((ifaddr | (~(un.v4.mask))) != un.v4.bcast) {
				logprint(ANSI_BKGRD_BRIGHT_RED ANSI_COLOR_WHITE);
			}
			lassert(inet_ntop(AF_INET, &un.v4.bcast, addrstr, INET_ADDRSTRLEN));
			logprint("bcast: %s\t", addrstr);
			logprint(ANSI_CLRST);

			// add to vector
			VECTOR_VALTYPE vval = { .lng = un._vval };
			lassert(vector_push_back(ipv4_faces, vval));
			count++;

			logprint("\n");
		}

		ifap = ifap->ifa_next;
		if(ifap == ifap_head)
			break;
	}
	freeifaddrs(ifap_head);
	return count;
}

BCSMSGREPLY* hack_beacon(int fd, char* buf){
	while(1){
            recvfrom(fd, buf, MAXBUF, 0, NULL, NULL);
			BCSBEACON* try_beacon = (BCSBEACON*)buf;
			if(try_beacon->magic != BCSBEACON_MAGIC)
				break;
	}
	return (BCSMSGREPLY*)buf;
}

void test_combinations(BCSPLAYER_FULL_STATE* pfs){
	fprintf(stdout, "Start combinations!\n");
	for(int i = 6; i > 0; i--){
		for(int j = 0; j < 7; j++){
			sendto(pfs->sockfd, &all_possible[j], sizeof(msg_connect),
			0, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
			while(1){
				recvfrom(pfs->sockfd, buf, MAXBUF, 0, NULL, NULL);
				try_beacon = (BCSBEACON*)buf;
				if(try_beacon->magic != BCSBEACON_MAGIC)
					break;
			}
			reply = (BCSMSGREPLY*)buf;
			int rep = be32toh(reply->type);
			fprintf(log_file, "->We've send %s\n", names_resp[be32toh(all_possible[j].action)]);
			if(rep > 6){
				fprintf(log_file, "<-Stranger thing %d\n", rep);
				continue;
			}
			fprintf(log_file, "<-And got %s\n", names_reply[rep]);
		}
		BCSMSG t = all_possible[i];
		all_possible[i] = all_possible[i - 1];
		all_possible[i - 1] = t;
	}
}

void send_anything(BCSPLAYER_FULL_STATE* pfs){
	fprintf(stdout, "Start send all possible messages\n");
    for(int i = 0; i < 7; i++){
        sendto(pfs->sockfd, &all_possible[i % 7], sizeof(all_possible[i % 7]),
        0, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
		int count_try = 0;//количество попыток
		++count_try;
		fprintf(log_file, "->Send: %s\n", names_resp[be32toh(all_possible[i].action)]);
        while(1){
			recvfrom(pfs->sockfd, buf, MAXBUF, 0, NULL, NULL);
			try_beacon = (BCSBEACON*)buf;
			if(try_beacon->magic != BCSBEACON_MAGIC)
				break;
		}

		reply = (BCSMSGREPLY*)buf;
		
		int rep = be32toh(reply->type);
		//не connect:
		if(i % 7 < 6){
			if(rep > 6){
				fprintf(log_file, "Stranger thing: %d\n", rep);
				continue;
			}
			if(rep != BCSREPLT_NACK){
				fprintf(log_file, "!!Unexpected reply on %s not NACK but %s\n",
				names_resp[i % 7], names_resp[rep]);
			}
		}else{
			if(rep != BCSREPLT_MAP && rep != BCSREPLT_ACK){
				fprintf(log_file, "!!!Unexpected reply on connect: %s\n", names_reply[rep]);
			}
		}
		break;
		if(count_try == 6)
			fprintf(log_file, "!!!Server has no response on %d\n", be32toh(all_possible[i % 7].action));
    }
}

void connect_from_same(BCSPLAYER_FULL_STATE* pfs, uint64_t count_zombie_clients){
	printf("Start test multiconnect from same endpoint.\n");
    for(uint64_t i = 0; i < count_zombie_clients; i++){
        sendto(pfs->sockfd, &msg_connect, sizeof(msg_connect),
        0, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
        while(1){
            recvfrom(pfs->sockfd, buf, MAXBUF, 0, (sockaddr*)&serv, &size_serv);
			try_beacon = (BCSBEACON*)buf;
			if(try_beacon->magic != BCSBEACON_MAGIC)
				break;
        }
        reply = (BCSMSGREPLY*)buf;
        int rep = be32toh(reply->type);
        switch(rep){
            case BCSREPLT_ACK:{
                fprintf(log_file, "<-Server has connected with same client.\n");
                break;
            }
            case BCSREPLT_NACK:{
                fprintf(log_file, "<-OK.\n");
                break;
            }
			case BCSREPLT_MAP:{
				fprintf(log_file, "<-OK: MAP\n");
				break;
			}
            default:{
                fprintf(log_file, "<-Server sent some shit: %s.\n", names_reply[rep]);
				break;
            }
        }
    }
}

void several_connecting(BCSPLAYER_FULL_STATE* pfs, int count_zombie_clients){	
	int udp_zombie_sockets[count_zombie_clients];
	for(int i = 0; i < count_zombie_clients; i++)
		udp_zombie_sockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	printf("Start connect several clients\n");
    //пытаемся нагрузить сервер отсылая connect с разных портов
	int connected_clients = 0;
	sendto(pfs->sockfd, &msg_disconnect, sizeof(msg_disconnect),
	0, (sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
    for(int i = 0; i < count_zombie_clients; i++){
        udp_zombie_sockets[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		sendto(udp_zombie_sockets[i], &msg_connect, sizeof(msg_connect),
		0, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
        //TODO waiting with epoll
        while(1){
            recvfrom(udp_zombie_sockets[i], buf, MAXBUF, 0, (sockaddr*)&serv, &size_serv);
			try_beacon = (BCSBEACON*)buf;
			if(try_beacon->magic != BCSBEACON_MAGIC)
				break;
		}

        reply = (BCSMSGREPLY*)buf;
        int rep = be32toh(reply->type);
        switch(rep){
            case BCSREPLT_ACK:{
				++connected_clients;
                fprintf(stderr, "Server has connected with new client.\n");
                break;
            }
			case BCSREPLT_MAP:{
				++connected_clients;
				fprintf(stdout, "Server thinks we're connected and says: <-%s\n", names_reply[rep]);
				break;
			}
            case BCSREPLT_NACK:{
                fprintf(stderr, "Server has no connected new client with %d number.\n", i + 1);
				printf("<-Reply: %s\n", names_reply[rep]);
                break;
            }
            default:{
                fprintf(stderr, "Server sent some shit: %d.\n", rep);
				break;
    		}
        }
		if(i > 32 && i < 56){
			sendto(udp_zombie_sockets[i], &msg_disconnect, sizeof(msg_disconnect),
			0, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
			if(i > 34 && i < 48){
				sendto(udp_zombie_sockets[i], &msg_connect2, sizeof(msg_disconnect),
				0, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
			}
			/*reply = hack_beacon(udp_zombie_sockets[i], buf, &pfs);
			int rep = be32toh(reply->type);
			if(rep == BCSREPLT_ACK)
				--connected_clients;*/
		}
        close(udp_zombie_sockets[i]);
    }
	fprintf(stdout, "[I]-----So, we have connected %d clients from %d\n", connected_clients, count_zombie_clients);
}

void test_disconnectig(BCSPLAYER_FULL_STATE* pfs){
	fprintf(stdout, "Start testing disconnect\n");
	int count_disconnect = 0;
	for(int i = 0; i < count_disconnect; i++){
		sendto(pfs->sockfd, &msg_disconnect, sizeof(msg_disconnect),
		0, (sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
		fprintf(stdout, "->Disconnect\n");
		sendto(pfs->sockfd, &msg_connect, sizeof(msg_disconnect),
		0, (sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
		fprintf(stdout, "->Connect\n");
		hack_beacon(pfs->sockfd, buf);
		reply = (BCSMSGREPLY*)buf;
		int rep = be32toh(reply->type);
		fprintf(stdout, "<-%s\n", names_reply[rep]);
	}
}
typedef struct{
	BCSPLAYER_FULL_STATE* pfs;
	int* socks;
	uint64_t n;
	uint64_t k;
	uint64_t count_actions;
	time_t t;
} DOS_ARGV;

void *dos(void *args){
    DOS_ARGV *argv = (DOS_ARGV*)args;
	for(uint64_t i = 0; i < argv->n; i++){
		argv->socks[argv->n*argv->k + i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		for(uint64_t j = 0; j < argv->count_actions; j++){
			sendto(argv->socks[i], &all_possible[j % 6 + 1], sizeof(all_possible[j % 6 + 1]),
        	0, (sockaddr*)&argv->pfs->endpoint, sizeof(argv->pfs->endpoint));
			fd_set read_set;
			FD_ZERO(&read_set);
			FD_SET(argv->socks[i], &read_set);
			struct timeval t;
			t.tv_sec = argv->t;
			t.tv_usec = 0;
			int res_selects = select(argv->socks[i] + 1, &read_set, NULL, NULL, &t);
			int a = be32toh(all_possible[j % 6 + 1].action);
			if(res_selects == 0)
				fprintf(log_file, "!!!Server timeout on %s request for %ld.%d time.\n",
				names_resp[a], argv->t, 0);
		}
		sendto(argv->socks[i], &msg_disconnect, sizeof(msg_disconnect),
		0, (sockaddr*)&argv->pfs->endpoint, sizeof(argv->pfs->endpoint));
		close(argv->socks[i]);
	}
	pthread_mutex_lock(&ctm);
	--count_threads;
	pthread_mutex_unlock(&ctm);
    // conform pthread interface
    return NULL;
}


void test_dos(BCSPLAYER_FULL_STATE* pfs, uint64_t count_clients, uint64_t count_actions, uint64_t num_threads, time_t sec){
	fprintf(log_file, "Start DoS\n");
	int* udp_zombie_sockets = malloc(count_clients * sizeof(int));
	if(!udp_zombie_sockets)
		fprintf(stderr, "Can't allocate memory!\n");
	count_threads = 0;
	pthread_t* threads_array = malloc(num_threads * sizeof(pthread_t));
	DOS_ARGV* argv = malloc(num_threads * sizeof(DOS_ARGV));
	pthread_mutex_init(&ctm, NULL);//todo check return vaule == 0
	for(uint64_t i = 0; i < num_threads; i++){
		argv[i].pfs = pfs;
		argv[i].count_actions = count_actions;
		argv[i].n = i;
		argv[i].k = count_clients / num_threads;
		argv[i].socks = udp_zombie_sockets;
		argv[i].t = sec;
		++count_threads;
		pthread_create(&threads_array[i], NULL, dos, &argv[i]);
	}
	while(count_threads != 0){
		printf("%ld\n", count_threads);
	}
	free(udp_zombie_sockets);
	free(threads_array);
	free(argv);
}

bool is_equal(char* str1, char* str2){
	for(int i = 0; str1[i] != 0 && str2[i] != 0; i++){
		if(str1[i] != str2[i])
			return false;
	}
	return true;
}

void get_map(BCSPLAYER_FULL_STATE* pfs){
	fprintf(log_file, "Start tcp connecting\n");
    int sock_tcp = -1;
    sock_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	connect(sock_tcp, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
    //why not?
	char *ololo = malloc(1024*1024*200);
    memset(ololo, 254, 1024*1024*200);
    send(sock_tcp, ololo, 1024*1024*200, 0);
    close(sock_tcp);
	sock_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	connect(sock_tcp, (struct sockaddr*)&pfs->endpoint, sizeof(pfs->endpoint));
	close(sock_tcp);
	free(ololo);
}

int main(int argc, char const *argv[])
{
	if(argc < 2){
		fprintf(stderr, "Too few arguments");
		return 2;
	}
	#pragma region
   	verbose = true;
	// Random is good. PRNG is.. enough good.
	srand(time(NULL));

	BCSPLAYER_FULL_STATE pfs = {
		  .mutex_self  = PTHREAD_MUTEX_INITIALIZER
		, .mutex_frame = PTHREAD_MUTEX_INITIALIZER
		, .mutex_sock  = PTHREAD_MUTEX_INITIALIZER
		, .self = { 
			  .state = BCSCLST_STANDALONE
			, .direction = BCSDIR_UP
			, .position= {
				  .x = 0
				, .y = 0
			  }
		  }
		, .self_ext = {
			  .frags = 0
			, .deaths = 0
			, .nickname = "Unknown Soldier"
		  }
		, .others = {
			  .count = 0
		  }
		, .map = { // DONE: intialize right <- there
			  .width = 0
			, .height = 0
			, .map_primitives = NULL
		  }
		, .map_overlay = {
			  .width = 0
			, .height = 0
			, .map_primitives = NULL
		  }
		, .mappad = NULL
		, .below = NULL
		, .frames = 0
		, .endpoint = {
			  .sin_addr = INADDR_ANY
			, .sin_port = 0
			, .sin_family = AF_INET
		  }
		, .sockfd = -1 // erroneous socket
	};

	char addrstr[INET_ADDRSTRLEN];
	// узнать все пригодные интерфейсы
	VECTOR ifaces;

	// ReSharper disable once CppJoinDeclarationAndAssignment
	size_t iface_count;
	int epollfd;
	struct epoll_event ev_catch;
	int reuse_port = 1;

start_bcast_scan:
	// ReSharper disable once CppJoinDeclarationAndAssignment
	iface_count = init_broadcast_receiver(&ifaces);

	int *ubcls = (int*)malloc(sizeof(int) * ifaces.size);
	__syswrap(epollfd = epoll_create1(0));
	for(uint32_t i = 0; i < iface_count; i++) {
		struct sockaddr_in sin = {
			  .sin_addr.s_addr = ((BCAST_UN*)(&(ifaces.array[i])))->v4.bcast
			, .sin_port = htobe16(BCSSERVER_BCAST_PORT)
			, .sin_family = AF_INET
		};
		__syswrap(ubcls[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
		// reuse addr to allow server & client on the same iface
		__syswrap(setsockopt(ubcls[i], SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port)));
		__syswrap(bind(ubcls[i], (sockaddr*)&sin, sizeof(sin)));

		struct epoll_event evt = {
			  .events = EPOLLIN | EPOLLERR
			, .data.fd = ubcls[i]
		};
		__syswrap(epoll_ctl(epollfd, EPOLL_CTL_ADD, ubcls[i], &evt));
	}

	VECTOR servers;
	lassert(vector_init(&servers, 10)); // TODO: get rid of magic number
	uint32_t number = 0;
	struct timeval tv_last, tv_now, tv_diff;
	__syswrap(gettimeofday(&tv_last, NULL));

	ALOGI("Scanning for servers, please wait...\n");
	while(true) {
		int ret;
		__syswrap(ret = epoll_wait(epollfd, &ev_catch, 1, BCAST_SCAN_TIMEOUT_SEC * 1000));
		if(ret == 0) {
			ALOGI("No broadcast into local networks, exiting.\n");
			ALOGI("Maybe connect to server by IP:Port, or create your own?\n");
			// TODO: ask for endpoint
			//break;
			exit(EXIT_FAILURE);
		}
		// ret should be = 1
		lassert(ret == 1);
		struct sockaddr_in srv_sin;
		socklen_t sa_len = sizeof(srv_sin);

		if(ev_catch.events & EPOLLERR) {
			ALOGE("epoll_wait got EPOLLERR\n");
			continue;
		}

		if(ev_catch.events & EPOLLIN) {
			ssize_t len = recvfrom(ev_catch.data.fd, buf, BCSDGRAM_MAX, 0
				, (sockaddr*)&srv_sin, &sa_len);
			BCSBEACON *beacon = (BCSBEACON*)buf;
			if(be64toh(beacon->magic) != BCSBEACON_MAGIC) {
				// log format is reversed for readability
				ALOGW("Received beacon magic %016lx != %016lx incorrect, skipping\n"
					, beacon->magic, htobe64(BCSBEACON_MAGIC));
				continue;
			}

			if(len < (ssize_t)sizeof(BCSBEACON)) {
				ALOGW("Received beacon is too short, skipping\n");
				continue;
			}

			if(len > (ssize_t)sizeof(BCSBEACON)) {
				ALOGW("Received beacon is too long\n");
			}

			// print server if new
			BCAST_SRV_UN srv_new = { 
				.endpoint = { 
					  .addr = srv_sin.sin_addr.s_addr
					, .port = beacon->port
					, .zero = 0
				}
			};

			for(size_t i = 0; i < servers.size; i++) {
				if(servers.array[i].lng == srv_new._vval) {
					// this server is already listed
					goto next_epevent;
				}
			}

			VECTOR_VALTYPE vval = { .lng = srv_new._vval };
			lassert(vector_push_back(&servers, vval));
			lassert(inet_ntop(AF_INET, &srv_sin.sin_addr, addrstr, INET_ADDRSTRLEN));
			printf("\t%s:%hu\t%.*s - %u\n"
				, addrstr, be16toh(beacon->port), BCSBEACON_DESCRLEN, beacon->description
				, number + 1
			);
			++number;
			__syswrap(gettimeofday(&tv_last, NULL));
			continue;
		}
next_epevent:
		__syswrap(gettimeofday(&tv_now, NULL));
		timersub(&tv_now, &tv_last, &tv_diff);
		if(tv_diff.tv_sec >= BCAST_SCAN_TIMEOUT_SEC) {
			ALOGI("Server scan finished\n");
			break;
		}
	}

	// TODO: close b/cast listeners there?
	for(size_t i = 0; i < iface_count; i++) {
		close(ubcls[i]);
	}
	free(ubcls);
	close(epollfd);

	// if we are there, then servers.size > 0?
	lassert(servers.size > 0);
	uint32_t idx;
	while(true) {
		idx = 1;
		printf("Enter 0 to rescan, or the number of server to connect [1]: ");
		fflush(stdout);
		fgets(buf, 256, stdin); // TODO: buffer overflow, check?
		if(buf[0] == '\n')
			break;
		if(sscanf(buf, "%u", &idx) == 1 && idx <= servers.size)
			break;
		printf("Sorry you can't!\n");
	}

	if(idx == 0) {
		vector_free(&servers);
		vector_free(&ifaces);
		goto start_bcast_scan; // FIXME
	}
	
	// connect to selected server
	BCAST_SRV_UN *srv = (BCAST_SRV_UN*)(&(servers.array[idx - 1]));
	pfs.endpoint.sin_addr.s_addr = srv->endpoint.addr;
	pfs.endpoint.sin_port = srv->endpoint.port;

	lassert(inet_ntop(AF_INET, &(srv->endpoint.addr), addrstr, INET_ADDRSTRLEN) != NULL);
	ALOGI("Connecting to %s:%hu\n", addrstr, ntohs(srv->endpoint.port));

	VERBOSE ALOGV("Creating UDP socket... ");
	__syswrap(pfs.sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
	struct timeval rcvtimeo = {
		  .tv_sec = (BCSRECV_TIMEO / 1000)
		, .tv_usec = (BCSRECV_TIMEO % 1000)
	};
	__syswrap(setsockopt(pfs.sockfd, SOL_SOCKET, SO_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo)));
	VERBOSE logprint("OK.\n");

	VERBOSE ALOGV("Sending CONNECT...\n");
	#pragma endregion

	msg_disconnect.action = htobe32(BCSACTION_DISCONNECT);
    msg_connect2.action = htobe32(BCSACTION_CONNECT2);
    msg_reqstat.action = htobe32(BCSACTION_REQSTATS);
    msg_move.action = htobe32(BCSACTION_MOVE);
    msg_fire.action = htobe32(BCSACTION_FIRE);
    msg_rotate.action = htobe32(BCSACTION_ROTATE);
	msg_connect.action = htobe32(BCSACTION_CONNECT);
    
	all_possible[0].action = htobe32(BCSACTION_DISCONNECT);
	all_possible[1].action = htobe32(BCSACTION_REQSTATS);
	all_possible[2].action = htobe32(BCSACTION_MOVE);
	all_possible[3].action = htobe32(BCSACTION_ROTATE);
	all_possible[4].action = htobe32(BCSACTION_FIRE);
	all_possible[5].action = htobe32(BCSACTION_CONNECT2);
	all_possible[6].action = htobe32(BCSACTION_CONNECT);
    
    recvfrom(pfs.sockfd, buf, MAXBUF, 0, NULL, NULL);

    ssize_t rcvd = -1;


	for(int i = 0; i < 7; i++){
	    bcsproto_new_packet(&all_possible[i % 7]);
		names_reply[i] = malloc(11);
		names_resp[i] = malloc(11);
	}
	FILE* fdname = fopen("names_reply.txt", "r");
	for(int i = 0; i < 7; i++)
		fscanf(fdname, "%s", names_reply[i]);
	fclose(fdname);
	fdname = fopen("names_response.txt", "r");
	for(int i = 0; i < 7; i++)
		fscanf(fdname, "%s", names_resp[i]);
	fclose(fdname);

	log_file = fopen(argv[2], "w");
	if(log_file == NULL)
		printf("Log file does not exist\n");
	
	char comm[80];
	while(1){
		printf("1] Test all possible message combinations\n2] Send all types of messages\n 3] Try connect form one port\n4] Try connect from several ports\n5] Test disconnect\n6]Make DoS\n7] Test TCP connection\n0] Leave\n\n");
		fgets(comm, 80, stdin);
		int choose = atoi(comm);
		switch(choose){
			case 1:{
				test_combinations(&pfs);
				break;
			}
			case 2:{
				send_anything(&pfs);
				break;
			}
			case 3:{
				printf("Please, input the number of connections:\n->");
				fgets(comm, 80, stdin);
				uint64_t count = atoll(comm);
				connect_from_same(&pfs, count);
				break;
			}
			case 4:{
				printf("Please, input the number of connections:\n->");
				fgets(comm, 80, stdin);
				uint64_t count = atoll(comm);
				several_connecting(&pfs, count);
				break;
			}
			case 5:{
				test_disconnectig(&pfs);
				break;
			}
			case 6:{
				test_dos(&pfs, 600, 601, 602, 603);
				break;
			}
			case 7:{
				get_map(&pfs);
				break;
			}
			case 0:{
				exit(0);
			}
			default:{
				printf("Unknown command!\n");
				break;
			}
		}
	}

    return 0;
}
