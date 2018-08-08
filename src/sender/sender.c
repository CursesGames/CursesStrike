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

int main(int argc, char const *argv[])
{
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BCSMSG msg_disconnect = {
		  .action = htobe32(BCSACTION_DISCONNECT)
	};
    BCSMSG msg_connect2 = {
		  .action = htobe32(BCSACTION_CONNECT2)
	};
    BCSMSG msg_reqstat = {
		  .action = htobe32(BCSACTION_REQSTATS)
	};
    BCSMSG msg_move = {
		  .action = htobe32(BCSACTION_MOVE)
	};
    BCSMSG msg_fire = {
		  .action = htobe32(BCSACTION_FIRE)
	};
    BCSMSG msg_rotate = {
		  .action = htobe32(BCSACTION_ROTATE)
	};
	BCSMSG msg_connect = {
		  .action = htobe32(BCSACTION_CONNECT)
	};
    BCSMSG all_possible[7];
	all_possible[0].action = htobe32(BCSACTION_DISCONNECT);
	all_possible[1].action = htobe32(BCSACTION_REQSTATS);
	all_possible[2].action = htobe32(BCSACTION_MOVE);
	all_possible[3].action = htobe32(BCSACTION_ROTATE);
	all_possible[4].action = htobe32(BCSACTION_FIRE);
	all_possible[5].action = htobe32(BCSACTION_CONNECT2);
	all_possible[6].action = htobe32(BCSACTION_CONNECT);

    char* names_reply[7];
	char* names_resp[7];
	for(int i = 0; i < 7; i++){
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

    BCSBEACON* try_beacon;//чтобы проверять, вдруг бекон?
    BCSMSGREPLY* reply;//ответ
    int rep = -1;//тип ответа
    int n = 0;
    char buf[BUFSIZ];
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(2018);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    while(1){
        for(int i = 0; i < 7; i++)
            printf("%d] %s\n", i + 1, names_resp[i]);
        printf("0] Exit\n\n->");
        scanf("%d", &n);
        if(n == 0)
            break;
        
        send(fd, (struct sockaddr*)&all_possible[n - 1], sizeof(all_possible[n - 1]), 0);
        printf("->We've sent %s\n", names_resp[n - 1]);
        while(1){
            recvfrom(fd, buf, BUFSIZ, 0, NULL, NULL);
            try_beacon = (BCSBEACON*)buf;
            if(try_beacon->magic != BCSBEACON_MAGIC)
                break;
        }
        reply = (BCSMSGREPLY*)buf;
        printf("<-We've got %s\n", names_reply[rep]);
        memset(buf, 0, BUFSIZ);
    }


    close(fd);
    return 0;
}
