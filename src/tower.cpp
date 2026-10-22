#include "tower.h"
#include <cmath>
#include "settings.h"

// Мұнара безендіруі HP деңгейіне қарай: 0 — сұр тас, 1 — бекітілген (темір белдіктер), 2 — қамал (алтын, ту)
struct TowerLook {
    Color stone, light, mortar;
};

const TowerLook TOWER_LOOKS[3] = {
    {{150, 144, 156}, {184, 178, 190}, {104, 94, 112}},
    {{146, 136, 124}, {178, 168, 154}, {100, 90, 82}},
    {{96, 104, 126}, {128, 138, 162}, {66, 70, 90}},
};
const Color IRON = {70, 72, 82};
const Color GOLD = {232, 182, 60};
const Color OUTLINE = {62, 39, 49};  // пакеттегі суреттердің контур түсі
const Color WINDOW = {52, 40, 58};
const Color WOOD = {196, 128, 76};
const Color WOOD_DARK = {150, 92, 56};
const int BATTLEMENT = 7;  // артқы тістердің биіктігі (1x пиксельмен)
const int TOP_DEPTH = (ROAD_BOTTOM - ROAD_TOP) / SCALE - 6;  // шатыр алаңының тереңдігі (1x) — жолдың енімен бірдей көрінеді
const int FRONT_MERLON = 5;  // алдыңғы тістердің биіктігі (1x)
const int OVERHANG = 3;  // іргетас қабырғадан әр жағына қанша шығып тұрады (1x)
const int BASE_Y = ROAD_BOTTOM - 4;  // мұнараның алдыңғы қабырғасы жолдың алдыңғы шетінде — мұнара бүкіл жолды алып тұрады
const double K = SCALE / 3.0;  // төмендегі пиксель өлшемдері SCALE=3 үшін жазылған — осы көбейткішпен кішірейеді
const float LAUNCHER_ANGLE = 30;  // залп құбырларының көкжиектен көтерілу бұрышы, градус
const float PI = 3.14159265f;


// Мұнараның алдыңғы қабырғасының жоғарғы шеті (шатыр алаңының алдыңғы жиегі)
int front_top_y(int cannons) {
    return BASE_Y - TOWER_LEVELS[cannons - 1] * TILE;
}


// Мұнара ұялары: әр қабатта оң жақ балкон (1-қабат — жерден 2 тайл, әр келесі қабат +2 тайл)
std::vector<Slot> tower_slots(int cannons) {
    std::vector<Slot> slots;
    for (int floor = 0; floor < cannons; floor++) {
        int floor_y = BASE_Y - (2 + 2 * floor) * TILE;
        slots.push_back({(float)(TOWER_RIGHT + (int)(24 * K)), (float)(floor_y - (int)(20 * K)), true});
    }
    return slots;
}


// Залп қондырғысының орны (қораптың ортасы): шатырдың сол жақ алдыңғы тісінің үстінде
glm::vec2 launcher_position(int cannons) {
    int merlon_top = front_top_y(cannons) - FRONT_MERLON * SCALE;
    return {(float)(TOWER_X + (int)(20 * K)), (float)(merlon_top - (int)(22 * K))};
}


// Қолбасшының аяғы тұратын нүкте: шатыр алаңының ортасы
glm::vec2 commander_position(int cannons) {
    return {(float)((TOWER_X + TOWER_RIGHT) / 2 + 4), (float)(front_top_y(cannons) - TOP_DEPTH * SCALE / 2)};
}


// Мұнара безендіруінің деңгейі (0, 1, 2): «HP башни» жақсартуының деңгейі бойынша
int tower_tier(const Game& game) {
    int level = game.levels.at("max_hp");
    if (level >= 8) {
        return 2;
    }
    if (level >= 4) {
        return 1;
    }
    return 0;
}


// Кірпіш өрнегін тіктөртбұрыш ішіне саламыз: 4 px биік қатарлар, әр қатар жарты кірпішке жылжиды
void fill_bricks(Image& image, int x, int y, int w, int h, TowerLook look) {
    image_set_clip(image, x, y, w, h);
    image_fill_rect(image, x, y, w, h, look.stone);
    for (int row = 0; row < h / 4 + 1; row++) {
        int row_y = y + row * 4;
        int shift = row % 2 == 0 ? 0 : 5;
        image_line(image, x, row_y, x + w, row_y, look.mortar);
        for (int bx = x - shift; bx < x + w; bx += 10) {
            image_line(image, bx, row_y, bx, row_y + 3, look.mortar);
            image_line(image, bx + 1, row_y + 1, bx + 8, row_y + 1, look.light);  // кірпіштің жарық жиегі
        }
    }
    image_reset_clip(image);
}


// Түсті күңгірттеу: percent — жарықтықтың неше пайызы қалады
Color darker(Color color, int percent) {
    return {color.r * percent / 100, color.g * percent / 100, color.b * percent / 100};
}


// 2.5D мұнара суреті (1x пиксельмен саламыз, экранда SCALE есе үлкейеді): артқы тістер, жоғарыдан көрінетін
// шатыр алаңы, кірпіш қабырға, алдыңғы тістер, іргетас; tier — HP бойынша безендіру
Image build_tower_image(int height_tiles, int tier) {  // height_tiles: қабырға биіктігі (тайлмен)
    TowerLook look = TOWER_LOOKS[tier];
    int face_w = TOWER_WIDTH_TILES * 18;
    int face_h = height_tiles * 18;
    int ox = OVERHANG;  // қабырғаның сол шеті (іргетас одан кеңірек)
    int fy = BATTLEMENT + TOP_DEPTH;  // алдыңғы қабырғаның жоғарғы шеті
    int width = face_w + 2 * OVERHANG;
    int height = fy + face_h;
    Image image = new_image(width, height);

    // 1. Артқы тістер мен аласа қоршау (алыста — сәл күңгірт)
    TowerLook back = {darker(look.stone, 85), look.stone, look.mortar};
    fill_bricks(image, ox, BATTLEMENT - 3, face_w, 4, back);
    fill_bricks(image, ox, 0, 10, BATTLEMENT, back);
    fill_bricks(image, ox + face_w - 10, 0, 10, BATTLEMENT, back);

    // 2. Шатыр алаңы — жоғарыдан көрінетін жарық тақталар
    image_fill_rect(image, ox, BATTLEMENT, face_w, TOP_DEPTH, look.light);
    image_line(image, ox, BATTLEMENT + TOP_DEPTH / 2, ox + face_w, BATTLEMENT + TOP_DEPTH / 2, look.stone);
    for (int x = ox + 6; x < ox + face_w; x += 9) {
        image_line(image, x, BATTLEMENT, x, BATTLEMENT + TOP_DEPTH, look.stone);
    }

    // 3. Алдыңғы кірпіш қабырға
    fill_bricks(image, ox, fy, face_w, face_h, look);
    image_line(image, ox, fy, ox + face_w - 1, fy, look.light);  // қабырғаның жоғарғы жиегі

    // 4. Безендіру: бекітілгенде — бұрыш бағаналары, темір белдіктер; қамалда — алтын шегелер
    if (tier >= 1) {
        image_fill_rect(image, ox, fy, 3, face_h, look.mortar);
        image_fill_rect(image, ox + face_w - 3, fy, 3, face_h, look.mortar);
        Color rivet = tier == 2 ? GOLD : Color{150, 152, 160};
        for (int y = fy + 3; y < height - 20; y += 30) {
            image_fill_rect(image, ox, y, face_w, 3, IRON);
            for (int x = ox + 3; x < ox + face_w; x += 7) {
                set_pixel(image, x, y + 1, rivet);
            }
        }
    }

    // 5. Тар терезелер (ортада); қамалда — жарық жанып тұр
    Color window = tier == 2 ? Color{250, 200, 90} : WINDOW;
    for (int y = fy + 12; y < height - 30; y += 32) {
        image_fill_rect(image, ox + face_w / 2 - 2, y, 4, 8, window);
        image_fill_rect(image, ox + face_w / 2 - 1, y - 1, 2, 1, window);
    }

    // 6. Қамал: қабырғада қызыл ту, ортасында алтын белгі
    if (tier == 2) {
        int flag_x = ox + 6, flag_y = fy + 6;
        image_polygon(image, {{flag_x, flag_y}, {flag_x + 12, flag_y}, {flag_x + 12, flag_y + 22},
                              {flag_x + 6, flag_y + 17}, {flag_x, flag_y + 22}}, {170, 40, 40});
        image_fill_rect(image, flag_x + 1, flag_y + 1, 10, 2, {210, 70, 60});
        image_fill_rect(image, flag_x + 4, flag_y + 7, 4, 4, GOLD);
        image_fill_rect(image, flag_x - 1, flag_y - 1, 14, 2, IRON);
    }

    // 7. Алдыңғы тістер (шатыр алаңының алдыңғы жиегінде, бұрыштарда)
    for (int mx : {ox, ox + face_w - 10}) {
        fill_bricks(image, mx, fy - FRONT_MERLON, 10, FRONT_MERLON + 1, look);
        image_rect_outline(image, mx, fy - FRONT_MERLON, 10, FRONT_MERLON + 1, OUTLINE);
        if (tier == 2) {
            image_line(image, mx + 1, fy - FRONT_MERLON, mx + 8, fy - FRONT_MERLON, GOLD);
        }
    }

    // 8. Іргетас: төменгі жағында қабырғадан кеңірек ірі қою тастар
    image_fill_rect(image, 0, height - 6, width, 6, darker(look.stone, 80));
    for (int x = 0; x < width; x += 8) {
        image_line(image, x, height - 6, x, height, look.mortar);
        image_line(image, x + 1, height - 5, x + 6, height - 5, look.stone);
    }
    image_line(image, 0, height - 6, width, height - 6, look.mortar);

    // 9. Сыртқы контур және шатыр алаңы мен қабырға арасындағы сызық
    image_outline(image, OUTLINE);
    image_line(image, ox, BATTLEMENT, ox + face_w - 1, BATTLEMENT, OUTLINE);
    return image;
}


// Зеңбірек моделі (0, 1, 2): «Урон» мен «Скорострельность» деңгейлерінің қосындысы бойынша
int cannon_tier(const Game& game) {
    int total = game.levels.at("damage") + game.levels.at("fire_rate");
    if (total >= 14) {
        return 2;
    }
    if (total >= 7) {
        return 1;
    }
    return 0;
}


// Әр модельдің түстері мен өлшемдері
struct CannonLook {
    Color barrel, light;
    int length, thick, barrels;
    Color head;
    int radius;
    Color base;
    bool has_trim = false;
    Color trim = {0, 0, 0};
};

const CannonLook CANNON_LOOKS[3] = {
    // 0: қарапайым сұр
    {.barrel = {110, 114, 128}, .light = {150, 154, 168}, .length = 44, .thick = 14, .barrels = 1,
     .head = {120, 124, 138}, .radius = 16, .base = {90, 92, 104}},
    // 1: болат, қалың ұңғы, қола сақиналар, шегелер
    {.barrel = {96, 118, 140}, .light = {150, 172, 194}, .length = 50, .thick = 18, .barrels = 1,
     .head = {104, 126, 148}, .radius = 18, .base = {80, 96, 112}, .has_trim = true, .trim = {210, 160, 70}},
    // 2: қара «қатыгез», қос ұңғы, қызыл жолақ, тікенектер
    {.barrel = {58, 60, 70}, .light = {100, 102, 114}, .length = 54, .thick = 12, .barrels = 2,
     .head = {62, 64, 74}, .radius = 20, .base = {50, 50, 58}, .has_trim = true, .trim = {210, 50, 50}},
};


// Ұңғы (оңға қарап, өз координатасында: сол жақ жоғарғы бұрышы — (0, 0))
void draw_barrel(CannonLook look) {
    int length = look.length;
    int thick = look.thick;
    int height = look.barrels * thick + (look.barrels - 1) * 2;
    for (int b = 0; b < look.barrels; b++) {
        float top = 3 + b * (thick + 2);
        draw_rect({0, top, (float)length, (float)thick}, OUTLINE);
        draw_rect({2, top + 2, (float)length - 4, (float)thick - 4}, look.barrel);
        draw_rect({2, top + 2, (float)length - 4, 3}, look.light);  // жарық жиек
        if (look.has_trim && look.barrels == 1) {
            // Қола сақиналар
            for (int ring_x : {length / 2, length * 3 / 4}) {
                draw_rect({(float)ring_x, top + 1, 4, (float)thick - 2}, look.trim);
            }
        }
        if (look.has_trim && look.barrels == 2) {
            draw_rect({6, top + thick / 2 - 1, (float)length - 14, 2}, look.trim);  // қызыл жолақ
        }
    }
    // Дуло тежегіші — ұңғы ұшындағы жалпақ бөлік
    if (look.has_trim) {
        draw_rect({(float)length - 4, 0, 10, (float)height + 6}, OUTLINE);
        draw_rect({(float)length - 2, 2, 6, (float)height + 2}, look.light);
    }
}


// Зеңбірек: негіз, айналатын ұңғы (атқанда артқа тебеді), басы. Бәрі 140x140 «кенепте» салынады,
// кенеп модель матрицасымен (x, y) нүктесіне жылжып, size_factor есе кішірейеді
// recoil — тебу (0..1), glow — жарқырау (0..1), hot: «Перегрев» кезінде ұңғы қызарады
void draw_cannon(float x, float y, float angle, int tier, float size_factor, float recoil, float glow, bool hot) {
    CannonLook look = CANNON_LOOKS[tier];
    if (hot) {
        look.barrel = {190, 70, 40};
        look.light = {255, 150, 80};
    }
    float cx = 70, cy = 70;  // кенептің ортасы
    glm::mat4 canvas = translate(x, y) * scale(size_factor) * translate(-cx, -cy);
    set_model(canvas);

    // 1. Негіз (айналмайды)
    draw_rect({cx - 24, cy + 6, 48, 15}, OUTLINE, 0, 3);
    draw_rect({cx - 21, cy + 9, 42, 9}, look.base, 0, 2);
    if (look.has_trim) {
        draw_line(cx - 18, cy + 13, cx + 18, cy + 13, look.trim, 2);
    }

    // 2. Ұңғы: нысанаға бұрылған, атқанда артқа тебеді (ұңғы ортасы center нүктесінде)
    float shift = look.length / 2 - 6 * recoil;
    float center_x = cx + std::cos(angle) * shift;
    float center_y = cy + std::sin(angle) * shift;
    float barrel_w = look.length + 6;
    float barrel_h = look.barrels * look.thick + (look.barrels - 1) * 2 + 6;
    set_model(canvas * translate(center_x, center_y) * rotate(angle) * translate(-barrel_w / 2, -barrel_h / 2));
    draw_barrel(look);
    set_model(canvas);

    // 3. Дуло жарқылы — атқан сәтте
    if (recoil > 0.5f) {
        float tip = look.length - 6 * recoil + 6;
        float flash_x = cx + std::cos(angle) * tip;
        float flash_y = cy + std::sin(angle) * tip;
        draw_circle(flash_x, flash_y, 9, {255, 150, 40});
        draw_circle(flash_x, flash_y, 5, {255, 240, 150});
    }

    // 4. Басы
    float radius = look.radius;
    if (tier == 2) {
        // Тікенектер: басының айналасында 6 үшбұрыш
        for (int k = 0; k < 6; k++) {
            float a = k * PI / 3 + PI / 6;
            draw_polygon({{cx + std::cos(a) * (radius + 9), cy + std::sin(a) * (radius + 9)},
                          {cx + std::cos(a - 0.35f) * radius, cy + std::sin(a - 0.35f) * radius},
                          {cx + std::cos(a + 0.35f) * radius, cy + std::sin(a + 0.35f) * radius}}, OUTLINE);
        }
    }
    draw_circle(cx, cy, radius + 3, OUTLINE);
    draw_circle(cx, cy, radius, look.head);
    draw_circle(cx - (int)radius / 3, cy - (int)radius / 3, (int)radius / 3, look.light);  // жарқыл
    if (tier == 1) {
        // Шегелер
        for (int k = 0; k < 4; k++) {
            float a = k * PI / 2 + PI / 4;
            draw_circle(cx + std::cos(a) * (radius - 5), cy + std::sin(a) * (radius - 5), 2, look.trim);
        }
    }
    if (tier == 2) {
        draw_circle(cx, cy, 5, look.trim);  // қызыл өзек
    }

    // 5. Жаңа модельге өткенде ақ сақина
    if (glow > 0) {
        draw_circle(cx, cy, (int)(radius + 6 + 20 * (1 - glow)), {255, 255, 255}, 3);
    }
    reset_model();
}


// Залп қондырғысы: сол жақ тістің үстіндегі тұғыр және үш құбырлы бронды қорап; ready — залп дайын ба (шам түсі)
void draw_launcher(float final_x, float final_y, float recoil, bool ready) {  // final_x, final_y — қораптың ортасы, recoil: тебу (0..1)
    // Толық өлшемде 110x90 кенепте саламыз, модель матрицасы оны K есе кішірейтеді
    float x = 55, y = 45;
    glm::mat4 canvas = translate(final_x, final_y) * scale((float)K) * translate(-x, -y);
    set_model(canvas);

    // 1. Тұғыр: тістің үстіне қойылған тірек
    draw_rect({x - 12, y + 12, 24, 14}, OUTLINE, 0, 2);
    draw_rect({x - 9, y + 15, 18, 8}, {90, 84, 96});

    // 2. Қорап пен құбырлар: 60x40 өлшемде оңға қарап саламыз, LAUNCHER_ANGLE градусқа жоғары бұраймыз
    set_model(canvas * translate(x, y) * rotate(-LAUNCHER_ANGLE * PI / 180) * translate(-30, -20));
    float shift = (int)(4 * recoil);  // атқанда құбырлар артқа тебеді
    for (int k = 0; k < 3; k++) {
        float top = 7 + k * 9;
        draw_rect({26 - shift, top, 24, 8}, OUTLINE);
        draw_rect({28 - shift, top + 2, 20, 4}, {70, 74, 70});
        draw_rect({46 - shift, top + 2, 3, 4}, {25, 20, 25});  // құбыр аузы
    }
    draw_rect({2, 2, 30, 36}, OUTLINE, 0, 4);
    draw_rect({5, 5, 24, 30}, {104, 116, 72}, 0, 3);
    draw_rect({5, 5, 24, 4}, {136, 150, 96});  // жарық жиек
    for (int k = 0; k < 3; k++) {
        Color stripe = k % 2 == 0 ? Color{200, 60, 50} : Color{230, 200, 70};
        draw_rect({5.0f + k * 8, 26, 8, 5}, stripe);  // ескерту жолағы
    }
    Color lamp = ready ? Color{80, 230, 100} : Color{230, 60, 50};
    draw_circle(17, 14, 5, OUTLINE);
    draw_circle(17, 14, 3, lamp);
    set_model(canvas);

    // 3. Атқан сәтте құбыр ауыздарында жарқыл (ауыздың орнын бұрыш бойынша есептейміз)
    if (recoil > 0.5f) {
        float a = LAUNCHER_ANGLE * PI / 180;
        for (int k = 0; k < 3; k++) {
            float dx = 50 - 30, dy = 11 + k * 9 - 20;  // ауыздың қорап ортасынан ығысуы (бұрмай тұрғанда)
            float fx = x + dx * std::cos(a) + dy * std::sin(a);
            float fy = y - dx * std::sin(a) + dy * std::cos(a);
            draw_circle(fx, fy, 7, {255, 150, 40});
            draw_circle(fx, fy, 4, {255, 240, 150});
        }
    }
    reset_model();
}


// Бос ұя: модуль қоюға болатын тұғыр
void draw_empty_slot(float x, float y) {
    draw_rect({x - 18 * (float)K, y + 8 * (float)K, 36 * (float)K, 12 * (float)K}, OUTLINE, 0, 3);
    draw_rect({x - 15 * (float)K, y + 11 * (float)K, 30 * (float)K, 6 * (float)K}, {110, 104, 116}, 0, 2);
}


// Балкон: мұнарадан оңға шығып тұрған жұқа тақтай және астындағы қиғаш тірек
void draw_balcony(Slot slot) {
    float floor_y = slot.y + (int)(20 * K);  // модуль тұратын тақтайдың үсті
    float wall_x = TOWER_RIGHT;  // мұнараның оң қабырғасы
    // 1. Тірек: қабырғадан тақтайдың астына
    float start_x = wall_x, start_y = floor_y + 48 * K;
    float end_x = wall_x + 48 * K, end_y = floor_y + 12 * K;
    draw_line(start_x, start_y, end_x, end_y, OUTLINE, (int)(10 * K));
    draw_line(start_x, start_y, end_x, end_y, WOOD_DARK, (int)(5 * K));
    // 2. Тақтай: үсті (жоғарыдан көрінеді, ашық) және алдыңғы қыры (қою)
    float depth = 4 * SCALE;  // тақтайдың жоғарыдан көрінетін тереңдігі (балкон тар)
    Rect deck = {wall_x - (int)(6 * K), floor_y - depth, (float)(int)(72 * K), depth};
    Rect edge = {deck.x, floor_y, deck.w, (float)(int)(10 * K)};
    draw_rect({deck.x - 1, deck.y - 1, deck.w + 2, depth + edge.h + 2}, OUTLINE);
    draw_rect(deck, {222, 160, 104});
    for (float x = deck.x + 7; x < deck.x + deck.w; x += 9) {
        draw_rect({x, deck.y, 1, deck.h}, WOOD);  // тақтай арасы
    }
    draw_rect(edge, WOOD_DARK);
}


void draw_tower(const Images& images, const Game& game) {
    int cannons = (int)game.stats.at("cannons");

    // 1. Мұнараның жолға түскен көлеңкесі (оң жаққа)
    float h = TOP_DEPTH * SCALE + 4;
    float sx = TOWER_RIGHT - 10, sy = BASE_Y - h + 2;
    draw_polygon({{sx, sy}, {sx + 55, sy}, {sx + 85, sy + h}, {sx, sy + h}}, {0, 0, 0, 60});

    // 2. Мұнара денесі (биіктігі — зеңбірек санына, безендіруі — HP деңгейіне қарай), есік
    const Texture& image = images.tower[cannons - 1][tower_tier(game)];
    Color glow = {0, 0, 0};
    if (game.tower_glow > 0) {
        // Мұнара жаңа түрге өткенде ағарып, біртіндеп қалпына келеді
        int light = (int)(140 * game.tower_glow / GLOW_TIME);
        glow = {light, light, light};
    }
    float top = BASE_Y - image.height;
    draw_texture(image, TOWER_X - OVERHANG * SCALE, top, image.width, image.height, {255, 255, 255}, glow);
    draw_texture(images.door, TOWER_X + TILE, BASE_Y - TILE - 4, images.door.width, images.door.height);

    // 3. Жалау: артқы оң жақ тісте, ұзын сырықта
    float flag_x = TOWER_RIGHT - 8 * SCALE;
    float flag_y = top - TILE - (int)(40 * K);
    int frame = (get_ticks() / 300) % 2;  // кадр әр 0.3 секундта ауысады
    draw_texture(images.flag[frame], flag_x, flag_y, TILE, TILE);
    Rect pole = {0, (float)(TILE - 3 * SCALE), (float)(6 * SCALE), (float)(3 * SCALE)};  // жалау суретінің төменгі бөлігі — сырық
    for (float pole_y = flag_y + TILE; pole_y < top + 2; pole_y += 3 * SCALE) {
        draw_texture_part(images.flag[0], pole, {flag_x, pole_y, pole.w, pole.h});
    }

    // 4. Қолбасшы: шатырда тұрып, кезек-кезек қимылдайды; навык қолданғанда секіреді
    glm::vec2 feet = commander_position(cannons);
    const Texture& body = images.commander[(get_ticks() / 450) % images.commander.size()];
    int jump = 0;
    if (game.commander_jump > 0) {
        jump = (int)(std::sin(PI * (1 - game.commander_jump / COMMANDER_JUMP_TIME)) * 14);
    }
    draw_texture(body, feet.x - body.width / 2, feet.y - jump - body.height, body.width, body.height);

    // 5. Залп қондырғысы (сол жақ алдыңғы тісте)
    glm::vec2 launcher = launcher_position(cannons);
    draw_launcher(launcher.x, launcher.y, game.launcher_recoil / LAUNCHER_RECOIL_TIME, game.skill_timers.at("volley") == 0);

    // 6. Қабаттағы балкондар мен зеңбіректер
    std::vector<Slot> slots = tower_slots(cannons);
    for (int i = 0; i < (int)slots.size(); i++) {
        draw_balcony(slots[i]);
        if (i >= (int)game.modules.size() || game.modules[i].type == "") {
            draw_empty_slot(slots[i].x, slots[i].y);
        } else if (game.modules[i].type == "cannon") {
            const Module& module = game.modules[i];
            float size_factor = i == 0 ? (float)K : 0.85f * (float)K;  // жоғарғы қабаттағы зеңбіректер сәл кішірек
            draw_cannon(slots[i].x, slots[i].y, module.angle, cannon_tier(game), size_factor, module.recoil / RECOIL_TIME,
                        game.cannon_glow / GLOW_TIME, game.overdrive_time > 0);
        }
    }
}
