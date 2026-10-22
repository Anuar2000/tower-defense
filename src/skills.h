#pragma once
#include <string>
#include <vector>
#include "game.h"

const std::vector<std::string> SKILL_ORDER = {"volley", "freeze", "airstrike", "shield", "overdrive"};  // төменгі тақтадағы реті

std::vector<std::string> owned_skills(const Game& game);
void cast_skill(Game& game, const std::string& key);
bool choose_airstrike_target(Game& game, float x, float y);
bool skill_bar_click(Game& game, float x, float y);
void update_skills(Game& game, float dt);
void draw_skill_world(const Game& game);
void draw_skill_bar(const Font& font, const Game& game);
