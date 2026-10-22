#pragma once
#include <vector>
#include "game.h"

void update_turret(Game& game, float dt);
void use_volley(Game& game);
void update_bullets(Game& game, float dt);
void draw_bullets(const std::vector<Bullet>& bullets);
