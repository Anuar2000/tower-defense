#pragma once
#include <string>
#include <utility>
#include <vector>
#include "game.h"

void press_escape(Game& game);
std::string handle_menu_event(Game& game, const Event& event);  // "quit" қайтса — ойыннан шығамыз
std::vector<std::pair<std::string, Rect>> clickable_rects(const Game& game);
void update_menu_hover(Game& game, const std::vector<std::pair<std::string, Rect>>& rects);
void draw_menu(const Font& font, const Font& big_font, const Game& game);
