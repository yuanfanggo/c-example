#include <netdb.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

char *ip_addr = NULL;

static void limit_set()
{
	struct rlimit nofile_rlim, nproc_rlim;

	memset(&nofile_rlim, 0, sizeof(nofile_rlim));
	if (getrlimit(RLIMIT_NOFILE, &nofile_rlim) == 0)
	{
		printf("RLIMIT_NOFILE nofile_rlim.rlim_cur %lu\n", nofile_rlim.rlim_cur);
		printf("RLIMIT_NOFILE nofile_rlim.rlim_max %lu\n", nofile_rlim.rlim_max);
	}
	nofile_rlim.rlim_cur = nofile_rlim.rlim_max;
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

static void *myconnect(void *ptr)
{
	int sockfd;
	uint16_t port;
	struct sockaddr_in servaddr, cli;

	port = *(uint16_t *)ptr;
	//printf("connected to port %d..\n", port);

	// socket create and verification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		printf("Port %d socket creation failed. errno:%u, reason:%s\n", port, errno, strerror(errno));
		return NULL;
	}

	memset(&servaddr, 0, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(ip_addr);
	servaddr.sin_port = htons(port);

	// connect the client socket to server socket
	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		printf("Port %d connection with the server failed. errno:%u, reason:%s\n", port, errno, strerror(errno));
		return NULL;
	}
	else
		printf("connected to the server [%s:%d]\n", ip_addr, port);

	// close the socket
	close(sockfd);
}

int main(int argc, char *argv[])
{
	pthread_t *thread;
	uint16_t *port;
	uint16_t len, min_port, max_port;

	if (argc != 4)
	{
		printf("%s ip_address min_port max_port\n", argv[0]);
		return -1;
	}

	ip_addr = argv[1];
	min_port = atoi(argv[2]);
	max_port = atoi(argv[3]);
	len = max_port - min_port;
	thread = malloc(sizeof(pthread_t) * len);
	port = malloc(sizeof(uint16_t) * len);

	limit_set();

	/* Create independent threads each of which will execute function */

	for (int i = 0; i <= len; i++)
	{
		port[i] = i + min_port;
		while (true)
		{
			if (pthread_create(&(thread[i]), NULL, myconnect, (void *)&(port[i])) != 0)
			{
				printf("Port %d pthread_create failed. errno:%u, reason:%s\n", i + min_port, errno, strerror(errno));
				continue;
				return 1;
			}
			else{
				break;
			}
		}
	}
	printf("pthread_create ok...\n");

	/* Wait till threads are complete before main continues. Unless we  */
	/* wait we run the risk of executing an exit which will terminate   */
	/* the process and all threads before the threads have completed.   */
	for (int i = 0; i <= len; i++)
	{
		pthread_join(thread[i], NULL);
	}
	free(thread);
	free(port);
	return 0;
}
