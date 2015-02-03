#include <stdbool.h>

typedef struct Player {
  int id;
  char *name;
  char *opponent;
  bool spectator;
  int position;
  int hp;
  bool inGame;
  
  struct {
    int join;
    int position;
    int attack;
    int moko;
    char buf[5];
  } _state;
} Player;

typedef struct PlayerList {
  Player *player;
  struct PlayerList *next;
} PlayerList;

Player* findPlayerById(PlayerList *list, int id);
Player* findPlayerByName(PlayerList *list, char* name);
PlayerList* addPlayer(PlayerList *list, char* name);
bool joinGame(PlayerList *list, int playerId, char* game);
bool leaveGame(PlayerList *list, int playerId);
PlayerList *deletePlayerById(PlayerList *list, int id);