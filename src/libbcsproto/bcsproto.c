#include <stddef.h>
#include <errno.h>
#include <endian.h>
#include <sys/ioctl.h>
#include "bcsproto.h"
#include "../liblinux_util/linux_util.h"

_Static_assert(sizeof(BCSDIRECTION) == sizeof(int), "govno");

uint32_t bcsproto_next_packet_no = 0;

inline void bcsproto_new_packet(BCSMSG *msg) {
	msg->packet_no = htobe32(bcsproto_next_packet_no);
	++bcsproto_next_packet_no;
	__syscall(gettimeofday(&msg->time_gen, NULL));
}

ssize_t sendto2(
	int fd, const void *buf, size_t n,
	int flags, struct sockaddr *addr, socklen_t addr_len
) {
	int nb_yes = 1;
	int nb_no = 0;
	__syscall(ioctl(fd, FIONBIO, &nb_yes, sizeof(nb_yes)));

	int ret = sendto(fd, buf, n, flags, addr, addr_len);
	lassert(errno == EWOULDBLOCK);

	__syscall(ioctl(fd, FIONBIO, &nb_no, sizeof(nb_no)));
	return sendto(fd, buf, n, flags, addr, addr_len);
}

/*extern ssize_t recv4(int fd, void *buf, size_t *n, int flags) {
	size_t req_n = *n;
	char *ptr = (char*)buf;
	while(true) {
		ssize_t rcvd = recv(fd, ptr, req_n, flags);
		if(rcvd == -1)
	}
}*/