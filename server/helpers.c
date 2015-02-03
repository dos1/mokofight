#include <arpa/inet.h>

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) { // ipv4
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr); // ipv6
}
