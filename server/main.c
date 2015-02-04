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
  // generates random name
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
  // sends message to every active player or spectator except of player "sender"
  printf("broadcasting %s len %d sender %d\n", buf, len, sender);
  PlayerList *list = game.players;
  while (list) {
    if (list->player->id != sender) {
      if (list->player->name || list->player->spectator) {
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
  // sends player list to player "id"
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

void sendGameEvent(Player *player, char* buf, int len) {
  if (!player || !player->opponent || !player->inGame) return;
  
  Player *opponent = findPlayerByName(game.players, player->opponent);
  
  if (!opponent || !opponent->opponent || !opponent->inGame) return;
  
  char gameName[11] = "";
  
  // make game name persistent regardless of which side initiated the event
  if (strncmp(player->name, opponent->name, 5) < 0) {
    strncat(gameName, player->name, 5);
    strncat(gameName, opponent->name, 5);
  } else {
    strncat(gameName, opponent->name, 5);
    strncat(gameName, player->name, 5);
  }
  
  char newBuf[255] = "EVENT";
  strcat(newBuf, gameName);
  strncat(newBuf, buf, len);
  strcat(newBuf, "NOMOR");
  
  int newLen = strlen(newBuf);
  
  if (write(player->id, newBuf, newLen) == -1) {
    perror("sendGameEvent write player");
  }
  if (write(opponent->id, newBuf, newLen) == -1) {
    perror("sendGameEvent write opponent");
  }
  
  // send to spectators
  PlayerList *list = game.players;
  while (list) {
    if (list->player->spectator && list->player != player && list->player != opponent) {
      if (write(list->player->id, newBuf, newLen) == -1) {
        perror("sendGameEvent write spectator");
      }
    }
    list = list->next;
  }
  
}

void startGame(char* name, char* opponentName) {
  Player *player = findPlayerByName(game.players, name), *opponent = findPlayerByName(game.players, opponentName);
  
  if (!player || !opponent) {
    printf("couldn't start the game!\n");
    return;
  }
  
  if (player->opponent) {
    free(player->opponent);
  }
  player->opponent = strdup(opponentName);
  
  if (opponent->opponent) {
    free(opponent->opponent);
  }
  opponent->opponent = strdup(name);
  
  player->inGame = true;
  opponent->inGame = true;
  
  player->hp = 100;
  opponent->hp = 100;
	
	broadcast("OLD", 3, -1);
	broadcast(player->name, 5, player->id);
	broadcast(opponent->name, 5, opponent->id);
	
  if (write(player->id, "GAME", 4) == -1) {
    perror("startGame write player");
  }
  if (write(opponent->id, "GAME", 4) == -1) {
    perror("startGame write opponent");
  }
  
  sendGameEvent(player, "S", 1);
  
}

bool endGame(Player *winner) {
  if (!winner || !winner->inGame) return false;
  Player* opponent = findPlayerByName(game.players, winner->opponent);
  if (!opponent) return false;
  
  char data[7] = "E";
  printf("player %d wins!\n", winner->id);
  strncat(data, winner->name, 5);
  sendGameEvent(winner, data, 6);
  
  winner->inGame = false;
  free(winner->opponent);
  winner->opponent = NULL;
  opponent->inGame = false;
  free(opponent->opponent);
  opponent->opponent = NULL;
  
  broadcast("NEW", 3, winner->id);
  broadcast(winner->name, 5, winner->id);
  broadcast("NEW", 3, opponent->id);
  broadcast(opponent->name, 5, opponent->id);
  
  return true;
}

void disconnect(int id) {
  if (close(id)) {
    perror("couldn't close the socket of disconnected player");
  }
  FD_CLR(id, &game.descriptors);
  
  Player *player = findPlayerById(game.players, id);

  endGame(findPlayerByName(game.players, player->opponent));
  
  if (player) {
    if (player->name && !player->inGame) {
      broadcast("OLD", 3, id);
      broadcast(player->name, 5, id);
    }

    game.players = deletePlayerById(game.players, id);
  }
  printf("socket %d disconnected\n", id);
}



bool attack(Player *player, char type1, char type2) {
  if (!player) return false;
  
  if ((!(type1 == 'X' || type1 == 'Y' || type1 == 'Z')) || (!(type2 == 'X' || type2 == 'Y' || type2 == 'Z'))) {
    printf("socket %d sent incorrect attack type %c%c\n", player->id, type1, type2);
    if (write(player->id, "FAIL", 4) == -1) {
      perror("write");
    }
    return false;
  }

  Player *opponent = findPlayerByName(game.players, player->opponent);
  
  if (!opponent || !player->inGame) return false;
  
  // FIXME: check with current position instead of generating random outcome
  
  char data[10] = "A";
  
  if (rand() % 2) {
    // hit
    printf("HIT!\n");
    strcat(data, "H");
    
    // substract random value between 5 and 15 from opponent's HP
    opponent->hp -= 5;
    opponent->hp -= rand() % 11;
    
    if (opponent->hp < 0) {
      opponent->hp = 0;
    }
    
  } else {
    // miss
    printf("MISS!\n");
    strcat(data, "M");
  }
  strncat(data, opponent->name, 5);
  
  if (opponent->hp < 100) {
    strcat(data, "0");
  }
  if (opponent->hp < 10) {
    strcat(data, "0");
  }
  char hp[4];
  snprintf(hp, 4, "%d", opponent->hp);
  strcat(data, hp);
  
  sendGameEvent(player, data, strlen(data));
  
  if (opponent->hp == 0) {
    endGame(player);
  }
  
  return true;
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
            // connection dropped
            
            if (nbytes == 0) {
              printf("socket %d hung up\n", i);
            } else {
              perror("recv");
            }

            disconnect(i);

          } else {
            
            // Got some data from client!
            
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

                bool starting = joinGame(game.players, i, player->_state.buf);
                
                if (write(i, "OKAY", 4) == -1) {
                  perror("write");
                }
                
                if (starting) {
                  printf("LET THE GAME BEGIN\n");
                  
                  startGame(player->name, player->_state.buf);
                }
              }
              
            } else if (player->_state.attack) {
              // player wants to attack

              if (player->_state.attack == 2) {
                player->_state.buf[0] = buf[0];
              } else {
                attack(player, player->_state.buf[0], buf[0]);
              }
              player->_state.attack--;
              
            } else if (player->_state.position) {
              // player wants to update his position

              updatePosition(player, buf[0]);
              player->_state.position = 0;
              
            } else {
              
              // no special state, so it's a new command!
              
              switch (buf[0]) {
                case 'H':
                  // clients wants to be a player
                  if (player->name) {
                    printf("socket %d wants to become a player, but it's already a player!\n", i);
                    if (write(i, "NOOK", 4) == -1) {
                      perror("write");
                    }
                    break;
                  }
                  
                  printf("socket %d wants to become a player\n", i);
                  player->name = generateName();
                  broadcast("NEW", 3, i);
                  broadcast(player->name, 5, -1); // -1 cause player also needs to know its name
                  
                  printf("socket %d got name %s\n", i, player->name);
                  
                  sendPlayerList(i);
                  
                  break;
                case 'S':
                  // client wants to enable spectator mode
                  player->spectator = true;
                  if (write(i, "SPOK", 4) == -1) {
                    perror("write");
                  }
                  sendPlayerList(i);
                  break;
                case 'J':
                  // client wants to join game
                  if (player->inGame) {
                    printf("player %d already in game wants to join another one!\n", i);
                    if (write(i, "NOOK", 4) == -1) {
                      perror("write");
                    }
                    break;
                  }
                  
                  if (!player->name) {
                    printf("non-player %d wants to join the game!\n", i);
                    if (write(i, "NOOK", 4) == -1) {
                      perror("write");
                    }
                    break;
                  }
                  printf("player %d wants to join the game. listening for opponent...\n", i);
                  player->_state.join = 5;
                  break;
                case 'P':
                  // client wants to update his position
                  player->_state.position = 1;
                  printf("player %d wants to update its position\n", i);
                  break;
                case 'A':
                  // client wants to attack
                  if (player->inGame) {
                    player->_state.attack = 2;
                    printf("player %d wants to attack\n", i);
                  } else {
                    printf("player %d wants to attack, but it's not in active game!\n", i);
                    if (write(i, "FAIL", 4) == -1) {
                      perror("write");
                    }
                  }
                  break;
                case 'L':
                  // client leaves his game (gives up), but stays connected
                  if (!player->name || !player->opponent || !player->inGame) {
                    printf("socket %d issued illegal leave command\n", i);
                    if (write(i, "NOOK", 4) == -1) {
                      perror("write");
                    }
                    break;
                  }
                  
                  endGame(findPlayerByName(game.players, player->opponent));
                  
                  break;
                case 'Q':
                  // client wants the server to disconnect him. well, okay then!
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
