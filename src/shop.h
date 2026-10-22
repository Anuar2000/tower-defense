#pragma once
#include <string>
#include <utility>
#include <vector>
#include "assets.h"
#include "game.h"

bool click_hud(Game& game, float x, float y);
void handle_shop_event(Game& game, const Event& event);
std::vector<std::pair<std::string, Rect>> shop_rects(const Game& game);
void draw_coin_counter(const Font& font, const Texture& coin, int coins, float right, float y, float pulse);
void draw_hud_buttons(const Font& font, const Game& game);
void draw_shop(const Images& images, const Font& font, const Font& big_font, const Game& game);
