#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "players.h"

Player* findPlayerById(PlayerList *list, int id) {
  while (list) {
    if (list->player->id == id) {
      return list->player;
    }
    list = list->next;
  }
  return NULL;
}

Player* findPlayerByName(PlayerList *list, char* name) {
  while (list) {
    if (strncmp(name, list->player->name, 5) == 0) {
      return list->player;
    }
    list = list->next;
  }
  return NULL;
}

PlayerList* addPlayer(PlayerList *list, int id) {
  Player *player = malloc(sizeof(Player));
  player->id = id;
  player->name = NULL;
  player->opponent = NULL;
  player->spectator = false;
  player->position = 0;
  player->hp = 0;
  
  player->_state.join = 0;
  player->_state.position = 0;
  player->_state.attack = 0;
  player->_state.moko = 0;
  
  PlayerList *elem = malloc(sizeof(PlayerList));
  elem->player = player;
  elem->next = NULL;
  
  if (list) {
    PlayerList *pom = list;
    while (pom->next) {
      pom = pom->next;
    }
    pom->next = elem;
    return list;
  } else {
    return elem;
  }
}

static void _freePlayerListEntry(PlayerList *entry) {
  if (entry->player->name) {
    free(entry->player->name);
  }
  if (entry->player->opponent) {
    free(entry->player->opponent);
  }
  free(entry->player);
}

PlayerList *deletePlayerById(PlayerList *list, int id) {
  PlayerList *first = list;
  
  if (list->player->id == id) {
    PlayerList *pom = list->next;
    leaveGame(list, id);
    _freePlayerListEntry(list);
    free(list);
    return pom;
  }
  
  while (list->next) {
    if (list->next->player->id == id) {
      PlayerList *pom = list->next;
      list->next = list->next->next;
      
      leaveGame(list, id);
      _freePlayerListEntry(pom);
      free(pom);

      return first;
    }
    list = list->next;
  }
  return first;
}

bool joinGame(PlayerList *list, int playerId, char* game) {
  
  Player *player = findPlayerById(list, playerId); // TODO: error handling
  
  if (player->opponent) {
    free(player->opponent);
  }
  player->opponent = strdup(game);
  
  Player *opponent = findPlayerByName(list, game); // TODO: error handling
  
  if (opponent && strncmp(opponent->opponent, player->name, 5) == 0) {
    // game is joined mutually - let the game begin!
    player->inGame = true;
    opponent->inGame = true;
    return true;
  }
  
  return false;
}

bool leaveGame(PlayerList *list, int playerId) {
  Player *player = findPlayerById(list, playerId); // TODO: error handling
  
  if (! player->opponent) {
    return false;
  }
  
  Player *opponent = findPlayerByName(list, player->opponent); // TODO: error handling
  
  if (opponent) {
    if (strncmp(player->name, opponent->opponent, 5) == 0) {
      free(opponent->opponent);
      opponent->opponent = NULL;
      opponent->inGame = false;
    }
  }
  
  free(player->opponent);
  player->opponent = NULL;
  player->inGame = false;
  
  return true;
}
