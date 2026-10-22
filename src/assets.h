#pragma once
#include <map>
#include <string>
#include <vector>
#include "render.h"

struct Images {
    Texture sky;
    std::vector<Texture> horizon;
    Texture sky_low;
    Texture grass;
    Texture road;
    std::vector<Texture> commander;
    Texture dirt;
    Texture tower[3][3];  // [зеңбірек саны - 1][HP безендіру деңгейі]
    Texture door;
    std::vector<Texture> flag;
    Texture coin;
    std::map<std::string, std::vector<Texture>> enemies;  // әр түрге кадрлар тізімі
};

Images load_images();
