#include "assets.h"
#include <cstdio>
#include <random>
#include "image.h"
#include "settings.h"
#include "tower.h"

const std::string PACK = std::string(ASSET_DIR) + "/pixel-platformer/Tiles/";


// Бір тайлды жүктейміз, экранда SCALE есе үлкен салынады
Texture load_tile(const std::string& folder, int number) {  // folder: "" / "Characters/" / "Backgrounds/", number: тайл нөмірі
    char name[32];
    snprintf(name, sizeof(name), "tile_%04d.png", number);
    return make_texture(load_image(PACK + folder + name), SCALE);
}


// Жол беті: тапталған топырақ, ұсақ тастар, доңғалақ іздері, артқы шетінде шөп
Texture build_road() {
    int width = WIDTH / SCALE;
    int height = (ROAD_BOTTOM - ROAD_TOP) / SCALE;
    Image road = new_image(width, height, {196, 146, 98});
    std::mt19937 generator(7);  // әр жолы бірдей сурет шығуы үшін тұрақты кездейсоқтық
    auto randrange = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b - 1)(generator); };  // a..b-1
    auto chance = [&]() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(generator); };

    // 1. Ұсақ дақтар мен тастар
    Color spots[3] = {{178, 128, 84}, {212, 166, 116}, {168, 120, 80}};
    for (int i = 0; i < width * height / 14; i++) {
        int x = randrange(0, width);
        int y = randrange(3, height);
        set_pixel(road, x, y, spots[randrange(0, 3)]);
    }
    for (int i = 0; i < width / 10; i++) {
        int x = randrange(0, width);
        int y = randrange(4, height - 2);
        image_fill_rect(road, x, y, 2, 1, {150, 140, 136});  // тас
        set_pixel(road, x, y - 1, {190, 184, 180});
    }
    // 2. Доңғалақ іздері — жолақтар арасындағы ашық сызықтар
    for (int y : {height / 3, height * 2 / 3}) {
        for (int x = 0; x < width; x += 3) {
            if (chance() < 0.7f) {
                set_pixel(road, x, y, {214, 170, 122});
            }
        }
    }
    // 3. Артқы шеті: шөп жиегі және шөп шоқтары
    image_fill_rect(road, 0, 0, width, 2, {88, 170, 72});
    for (int x = 0; x < width; x += 2) {
        if (chance() < 0.5f) {
            set_pixel(road, x, 2, {88, 170, 72});
        }
        if (chance() < 0.15f) {
            set_pixel(road, x, 3, {110, 190, 86});
        }
    }
    return make_texture(road, SCALE);
}


Images load_images() {
    Images images;

    // Фон: аспан, көкжиек, төменгі бөлік
    images.sky = load_tile("Backgrounds/", 0);
    images.horizon = {load_tile("Backgrounds/", 8), load_tile("Backgrounds/", 9),
                      load_tile("Backgrounds/", 10), load_tile("Backgrounds/", 11)};
    images.sky_low = load_tile("Backgrounds/", 16);

    // Жер: үсті шөп, асты топырақ
    images.grass = load_tile("", 22);  // 22: астында қара жиегі жоқ шөп
    images.road = build_road();
    // Шатырдағы қолбасшы: пакеттегі кейіпкердің тұрған кезіндегі екі кадры
    images.commander = {load_tile("Characters/", 9), load_tile("Characters/", 10)};
    images.dirt = load_tile("", 122);

    // Мұнара: [зеңбірек саны - 1][HP безендіру деңгейі] — барлығы 3 x 3 сурет; есік, жалау
    for (int level = 0; level < 3; level++) {
        for (int tier = 0; tier < 3; tier++) {
            images.tower[level][tier] = make_texture(build_tower_image(TOWER_LEVELS[level], tier), SCALE);
        }
    }
    images.door = load_tile("", 130);
    images.flag = {load_tile("", 111), load_tile("", 112)};  // желбіреу анимациясының екі кадры

    // Монета: тайлдың бос жиектерін қиып аламыз
    images.coin = make_texture(crop_to_content(load_image(PACK + "tile_0151.png")), SCALE);

    // Жаулар: әр түрге кадрлар тізімі (өлшемі, айналуы, түсі салған кезде беріледі)
    for (const auto& [name, data] : ENEMY_TYPES) {
        for (int number : data.frames) {
            images.enemies[name].push_back(load_tile("Characters/", number));
        }
    }
    return images;
}
