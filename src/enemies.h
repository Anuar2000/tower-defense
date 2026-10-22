#pragma once
#include <string>
#include <glm/glm.hpp>
#include "assets.h"
#include "game.h"

int enemy_size(const std::string& kind);
Enemy create_enemy(Game& game, const std::string& kind, float hp_multiplier);
Rect enemy_rect(const Enemy& enemy);
glm::vec2 enemy_center(const Enemy& enemy);
Enemy* find_enemy(Game& game, int id);  // id бойынша жау; өліп кетсе — nullptr
std::vector<int> enemy_ids(const Game& game);  // қазіргі жаулардың id тізімі (цикл кезінде тізім өзгерсе де қауіпсіз)
int enemy_under(Game& game, float x, float y);
void damage_enemy(Game& game, int id, double amount, bool pierce);
void update_enemies(Game& game, float dt);
void draw_enemy(const Images& images, const Enemy& enemy, bool is_priority, bool frozen, bool hover, Color hover_color);
