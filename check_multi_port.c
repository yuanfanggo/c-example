#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <pthread.h>

static char *ip_addr;

static void limit_set()
{
	struct rlimit nofile_rlim, nproc_rlim;

	memset(&nofile_rlim, 0, sizeof(nofile_rlim));
	if (getrlimit(RLIMIT_NOFILE, &nofile_rlim) == 0)
	{
		printf("RLIMIT_NOFILE nofile_rlim.rlim_cur %lu\n", nofile_rlim.rlim_cur);
		printf("RLIMIT_NOFILE nofile_rlim.rlim_max %lu\n", nofile_rlim.rlim_max);
	}
	nofile_rlim.rlim_cur = nofile_rlim.rlim_max = 655350;
	if (setrlimit(RLIMIT_NOFILE, &nofile_rlim) == 0)
	{
		printf("RLIMIT_NOFILE nofile_rlim.rlim_cur %lu nofile_rlim.rlim_max %lu\n", nofile_rlim.rlim_cur, nofile_rlim.rlim_max);
	}

	memset(&nproc_rlim, 0, sizeof(nproc_rlim));
	if (getrlimit(RLIMIT_NPROC, &nproc_rlim) == 0)
	{
		printf("RLIMIT_NOROC nproc_rlim.rlim_cur %lu\n", nproc_rlim.rlim_cur);
		printf("RLIMIT_NOROC nproc_rlim.rlim_max %lu\n", nproc_rlim.rlim_max);
	}
	nproc_rlim.rlim_cur = nproc_rlim.rlim_max = 65535;
	if (setrlimit(RLIMIT_NPROC, &nproc_rlim) == 0)
	{
		printf("RLIMIT_NOFILE nproc_rlim.rlim_cur %lu nproc_rlim.rlim_max %lu\n", nproc_rlim.rlim_cur, nproc_rlim.rlim_max);
	}
}

static void myconnect(uint32_t *sockfd, uint16_t port)
{
	struct sockaddr_in servaddr;

	// socket create and verification
	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (*sockfd == -1)
	{
		printf("Port %d socket creation failed. errno:%u, reason:%s\n", port, errno, strerror(errno));
		exit(-1);
	}

	memset(&servaddr, 0, sizeof(servaddr));

	fcntl(*sockfd, F_SETFL, fcntl(*sockfd, F_GETFL) | O_NONBLOCK);

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip_addr);
	servaddr.sin_port = htons(port);

	// connect the client socket to server socket
	if (connect(*sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		if (errno != EINPROGRESS)
		{
			printf("Port %d connection with the server failed. errno:%u, reason:%s\n", port, errno, strerror(errno));
			exit(-1);
		}
	}
	else
		printf("connected to the server [%s:%d]\n", ip_addr, port);
}

static void port_detect(uint32_t *sockfd, uint16_t min_port, uint16_t len)
{
	uint32_t event_count;
	uint32_t port_event = 0;
	struct epoll_event event, events[len];
	int epoll_fd = epoll_create1(0);

	if (epoll_fd == -1)
	{
		printf("Failed to create epoll file descriptor. errno:%u, reason:%s\n", errno, strerror(errno));
		exit(-1);
	}
	else
	{
		printf("create epoll file descriptor success %d\n", epoll_fd);
	}

	for (int i = 0; i < len; i++)
	{
		memset(&event, 0, sizeof(event));
		event.events = EPOLLIN | EPOLLOUT;
		event.data.fd = sockfd[i];

		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event))
		{
			printf("Failed to add file descriptor to epoll. errno:%u, reason:%s\n", errno, strerror(errno));
			printf("add file descriptor to epoll fail %d EEXIST %d\n", event.data.fd, EEXIST);
			close(epoll_fd);
			exit(-1);
		}
		else
		{
			port_event++;
			// printf("add file descriptor to epoll success %d\n", event.data.fd);
		}
	}

	do
	{
		printf("\nPolling for input...\n");
		event_count = epoll_wait(epoll_fd, events, len, -1);
		printf("%d ready events\n", event_count);
		for (int i = 0; i < event_count; i++)
		{
			if (events[i].events & (EPOLLERR | EPOLLHUP))
			{
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
				port_event--;
				continue;
			}
			else if (events[i].events & EPOLLOUT)
			{
				for (int j = 0; j < len; j++)
				{
					if (events[i].data.fd == sockfd[j])
					{
						port_event--;
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, &events[i]);
						printf("connected to the server [%s:%d]\n", ip_addr, j + min_port);
						break;
					}
				}
			} 
		}
	} while (port_event);

	if (close(epoll_fd))
	{
		printf("Failed to close epoll file descriptor. errno:%u, reason:%s\n", errno, strerror(errno));
		exit(-1);
	}
}

int main(int argc, char *argv[])
{
	uint32_t *sockfd;
	uint16_t len, min_port, max_port;

	if (argc != 4)
	{
		printf("%s ip_address min_port max_port\n", argv[0]);
		return -1;
	}

	ip_addr = argv[1];
	min_port = atoi(argv[2]);
	max_port = atoi(argv[3]);
	len = max_port - min_port + 1;
	sockfd = malloc(sizeof(uint32_t) * len);

	limit_set();

	/* Create independent threads each of which will execute function */

	for (int i = 0; i < len; i++)
	{
		myconnect(&sockfd[i], i + min_port);
		// printf("socket create success %d\n", sockfd[i]);
	}

	port_detect(sockfd, min_port, len);

	free(sockfd);
	return 0;
}
