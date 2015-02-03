#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT "5104"

static void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) { // ipv4
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr); // ipv6
}

int main(void)
{
	fd_set descriptors;
	fd_set read_fds;
	int highestDescriptor;

	int server;
	int newConnection;

	struct sockaddr_storage remoteAddr;
	socklen_t addrLength;

	char buf[256];
	int nbytes;

	char remoteIP[INET6_ADDRSTRLEN];

	int yes = 1;

	int i, j, rv;

	struct addrinfo hints, *ai, *p;

	FD_ZERO(&descriptors);
	FD_ZERO(&read_fds);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}
	
	for(p = ai; p != NULL; p = p->ai_next) {
		server = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (server < 0) {
			continue;
		}

		setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

		if (bind(server, p->ai_addr, p->ai_addrlen) < 0) {
			close(server);
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "failed to bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(ai);

	if (listen(server, 10) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	FD_SET(server, &descriptors);
	highestDescriptor = server;

	while (true) {
		read_fds = descriptors;
		if (select(highestDescriptor+1, &read_fds, NULL, NULL, NULL) == -1) {
			perror("select");
			exit(EXIT_FAILURE);
		}

		for(i = 0; i <= highestDescriptor; i++) {
			if (FD_ISSET(i, &read_fds)) {
				if (i == server) {
					// new connection
					addrLength = sizeof remoteAddr;
					newConnection = accept(server,
						(struct sockaddr *)&remoteAddr,
						&addrLength);

					if (newConnection == -1) {
						perror("accept");
					} else {
						FD_SET(newConnection, &descriptors);
						if (newConnection > highestDescriptor) {
							highestDescriptor = newConnection;
						}
						printf("new connection from %s on "
							"socket %d\n",
							inet_ntop(remoteAddr.ss_family,
								get_in_addr((struct sockaddr*)&remoteAddr),
								remoteIP, INET6_ADDRSTRLEN),
							newConnection);
					}
				} else {
					// new data
					if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
						if (nbytes == 0) {
							printf("socket %d hung up\n", i);
						} else {
							perror("recv");
						}
						close(i); // bye!
						FD_CLR(i, &descriptors);
					} else {
						for(j = 0; j <= highestDescriptor; j++) {
							if (FD_ISSET(j, &descriptors)) {
								if (j != server && j != i) {
									if (send(j, buf, nbytes, 0) == -1) {
										perror("send");
									}
								}
							}
						}
					}
				}
			}
		}
	}
	
}
