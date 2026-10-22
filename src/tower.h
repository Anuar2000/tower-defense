#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "assets.h"
#include "game.h"
#include "image.h"

const float COMMANDER_JUMP_TIME = 0.35f;  // қолбасшының секіру уақыты, секунд
const float RECOIL_TIME = 0.12f;  // атқаннан кейін ұңғының тебу уақыты, секунд
const float GLOW_TIME = 0.8f;  // жаңа модельге өткендегі жарқырау уақыты, секунд
const float LAUNCHER_RECOIL_TIME = 0.2f;  // залп қондырғысының тебу уақыты, секунд

// Мұнара ұясы: модуль тұратын нүкте
struct Slot {
    float x, y;
    bool balcony;
};

std::vector<Slot> tower_slots(int cannons);  // cannons: зеңбірек саны = мұнара деңгейі (1..3)
glm::vec2 launcher_position(int cannons);
int tower_tier(const Game& game);
int cannon_tier(const Game& game);
Image build_tower_image(int height_tiles, int tier);
void draw_tower(const Images& images, const Game& game);
