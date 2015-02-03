#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "helpers.h"
#include "players.h"

#define PORT "5104"

static char* DICT = "0123456789ABCDEF";

struct {
  PlayerList *players;
  fd_set descriptors;
} game; // global object

char* generateName() {
  char* res = strdup("12345");
  int i;
  for (i=0; i<5; i++) {
    res[i] = DICT[rand() % strlen(DICT)];
  }
  
  // check if name already exists and regenerate if necessary
  if (findPlayerByName(game.players, res)) {
    return generateName();
  }
  
  return res;
}

void broadcast(char* buf, int len, int sender) {
  printf("broadcasting %s len %d sender %d\n", buf, len, sender);
  PlayerList *list = game.players;
  while (list) {
    if (list->player->id != sender) {
      if (list->player->active) {
        printf("  sending to %d\n", list->player->id);
        if (write(list->player->id, buf, len) == -1) {
          perror("write");
        }
      }
    }
    list = list->next;
  }
}

void sendPlayerList(int id) {
  PlayerList *list = game.players;
  while (list) {
    if (list->player->name && list->player->id != id && !list->player->inGame) {
      if (write(id, list->player->name, 5) == -1) {
        perror("sendPlayerList write");
      }
    }
    list = list->next;
  }
  if (write(id, "NOMOR", 5) == -1) { // no more!
    perror("sendPlayerList write 2");
  }
}

void disconnect(int id) {
  if (close(id)) {
    perror("couldn't close the socket of disconnected player");
  }
  FD_CLR(id, &game.descriptors);
  
  Player *player = findPlayerById(game.players, id);
  
  if (player) {
    if (player->name && !player->inGame) {
      broadcast("OLD", 3, id);
      broadcast(player->name, 5, id);
    }

    game.players = deletePlayerById(game.players, id);
  }
  printf("socket %d disconnected\n", id);
}

int main() {
  
  srand(time(NULL));
  
  game.players = NULL;
  
  fd_set read_fds;
  int highestDescriptor;

  int server;
  int newConnection;

  struct sockaddr_storage remoteAddr;
  socklen_t addrLength;

  char buf[1];
  int nbytes;

  char remoteIP[INET6_ADDRSTRLEN];

  int yes = 1;

  int i, rv;

  struct addrinfo hints, *ai, *p;

  FD_ZERO(&game.descriptors);
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

  if (listen(server, 32) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  FD_SET(server, &game.descriptors);
  highestDescriptor = server;

  printf("MokoFight Server: alive and kicking!\n");
  
  // here's where the fun begins
  
  while (true) {
    read_fds = game.descriptors;
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
            FD_SET(newConnection, &game.descriptors);
            if (newConnection > highestDescriptor) {
              highestDescriptor = newConnection;
            }
            printf("new connection from %s on socket %d\n",
              inet_ntop(remoteAddr.ss_family,
                get_in_addr((struct sockaddr*)&remoteAddr),
                remoteIP, INET6_ADDRSTRLEN),
              newConnection);
            
            game.players = addPlayer(game.players, newConnection);
            Player *newPlayer = findPlayerById(game.players, newConnection);
            
            newPlayer->_state.moko = 4; // expect ping message
            
          }
        } else {
          // new data
          
          Player *player = findPlayerById(game.players, i);
          
          if (!player) {
            printf("no player for socket! MASSIVE BREAKAGE\n");
            close(i);
            continue;
          }
          
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
            if (nbytes == 0) {
              printf("socket %d hung up\n", i);
            } else {
              perror("recv");
            }

            disconnect(i);

          } else {
            
            if (buf[0] == '\n' || buf[0] == ' ' || buf[0] == '\r') {
              // whitespace: ignore for easier debugging
              continue;
            }
            
            if (player->_state.moko) {
              // ping pong state: MOKO -> FIGHT
              
              player->_state.buf[4-player->_state.moko] = buf[0];
              player->_state.moko--;
              if (!player->_state.moko) {
                if (strncmp(player->_state.buf, "MOKO", 4) == 0) {
                  if (write(i, "FIGHT", 5) == -1) {
                    perror("write");
                  }
                  player->active = true;
                  printf("pingpong successed for client %d\n", i);
                } else {
                  printf("invalid client %d!\n", i);
                  disconnect(i);
                }
              }
              
            } else if (player->_state.join) {
              // player wants to join the game
              
              player->_state.buf[5-player->_state.join] = buf[0];
              player->_state.join--;
              
              if (!player->_state.join) {
                printf("player %d selected the opponent %s\n", i, player->_state.buf);

                if (joinGame(game.players, i, player->_state.buf)) {
                  printf("LET THE GAME BEGIN\n");
                  
                  // TODO: start game
                  
                }
              }
              
            } else {
              
              // no special state, so it's a new command!
              
              switch (buf[0]) {
                case 'H':
                  // clients wants to be a player
                  printf("socket %d wants to become a player\n", i);
                  player->name = generateName();
                  broadcast("NEW", 3, i);
                  broadcast(player->name, 5, -1); // player also needs to know its name
                  
                  printf("socket %d got name %s\n", i, player->name);
                  
                  sendPlayerList(i);
                  
                  break;
                case 'S':
                  // client wants to enable spectator mode
                  player->spectator = true;
                  if (write(i, "SPOK", 4) == -1) {
                    perror("write");
                  }
                  break;
                case 'J':
                  // client wants to join game
                  if (!player->name) {
                    printf("non-player %d wants to join the game!\n", i);
                    if (write(i, "NOOK", 4) == -1) {
                      perror("write");
                    }
                  }
                  printf("player %d wants to join the game. listening for opponent...\n", i);
                  player->_state.join = 5;
                  break;
                case 'Q':
                  if (write(i, "BYE\n", 4) == -1) {
                    perror("write");
                  }
                  printf("disconnecting client %d per request\n", i);
                  disconnect(i);
                  break;
                default:
                  printf("got an unrecognized command %c from client %d\n", buf[0], i);
                  break;
              }
            }
          }
        }
      }
    }
  } // bjutiful!
  
}
