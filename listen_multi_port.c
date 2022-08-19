#include <stdio.h>
#include <stdint.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/socket.h>

static void limit_set()
{
	struct rlimit rlim;

	memset(&rlim, 0, sizeof(rlim));
	if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
	{
		printf("RLIMIT_NOFILE rlim.rlim_cur %lu\n", rlim.rlim_cur);
		printf("RLIMIT_NOFILE rlim.rlim_max %lu\n", rlim.rlim_max);
	}

	rlim.rlim_cur = rlim.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rlim) == 0)
	{
		printf("RLIMIT_NOFILE rlim.rlim_cur %lu rlim.rlim_max %lu\n", rlim.rlim_cur, rlim.rlim_max);
	}
}

int main(int argc, char *argv[])
{
	uint32_t *sockfd;
	uint32_t len, min_port, max_port;
	struct sockaddr_in servaddr;
	char string[256];

	if (argc != 3)
	{
		printf("%s min_port max_port\n", argv[0]);
		return -1;
	}

	limit_set();

	min_port = atoi(argv[1]);
	max_port = atoi(argv[2]);
	len = max_port - min_port + 1;
	sockfd = malloc(sizeof(uint32_t) * len);

	// socket create and verification

	for (int i = 0; i < len; i++)
	{
		sockfd[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd[i] == -1)
		{
			printf("%d socket creation failed. errno:%u, reason:%s\n", i + min_port, errno, strerror(errno));
			exit(0);
		}

		memset(&servaddr, 0, sizeof(servaddr));
		// assign IP, PORT
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port = htons(i + min_port);

		// Binding newly created socket to given IP and verification
		if ((bind(sockfd[i], (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		{
			printf("%d socket bind failed. errno:%u, reason:%s\n", i + min_port, errno, strerror(errno));
			continue;
		}

		// Now server is ready to listen and verification
		if ((listen(sockfd[i], 5)) != 0)
		{
			printf("%d Listen failed. errno:%u, reason:%s\n", i + min_port, errno, strerror(errno));
			continue;
		}
	}
	printf("TCP listen form %d to %d...\n", min_port, max_port);

	fgets(string, sizeof(string), stdin);

	// After chatting close the socket
	for (int i = 0; i < len; i++)
	{
		close(sockfd[i]);
	}
	free(sockfd);
	return 0;
}
