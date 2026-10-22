#pragma once
// Ойынның барлық сандары мен кестелері осында: жаңарту жүйесі кейін осы мәндерді өзгертеді
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "render.h"

const int WIDTH = 1280;
const int HEIGHT = 720;

// Пиксель-арт спрайттарды 2 есе үлкейтеміз (кішірек — картада орын көбірек)
const int SCALE = 2;
const int TILE = 18 * SCALE;  // жер/мұнара тайлының өлшемі (18px * 2)

// Жол сәл жоғарыдан көрінеді (2.5D): артқы шеті ROAD_TOP, алдыңғы шеті ROAD_BOTTOM, астында жер жиегі
// Жердегі жаулар 3 жолақпен жүреді: 0 — алыс (жоғарырақ), 2 — жақын (төменірек, алдында салынады)
const int LANES = 3;
const int LANE_GAP = 18;  // жолақтар арасы, пиксель
const int ROAD_TOP = HEIGHT - 5 * TILE;  // жолдың артқы шеті (артында шөп пен фон)
const int GROUND_Y = ROAD_TOP + 12;  // ең алыс жолақ: мұнара мен алыс жаулар осында тұрады
const int ROAD_BOTTOM = GROUND_Y + (LANES - 1) * LANE_GAP + 12;  // жолдың алдыңғы шеті

// Мұнара: сол жақта, тас кірпіштен (tower.cpp)
const int TOWER_X = 40;
const int TOWER_WIDTH_TILES = 3;
const int TOWER_MAX_HP = 100;
const int TOWER_RIGHT = TOWER_X + TOWER_WIDTH_TILES * TILE;  // мұнараның оң шеті

// Мұнара деңгейлері (зеңбірек саны бойынша): биіктігі (тайлмен). Зеңбіректер қабат-қабат балкондарда:
// 1-деңгей — 1-қабатта бір зеңбірек, 2-деңгей — 1 және 2-қабатта, 3-деңгей — үш қабатта. Шатырда — қолбасшы
const int TOWER_LEVELS[3] = {4, 6, 8};

// Негізгі турель
const float TURRET_DAMAGE = 10;
const float TURRET_FIRE_RATE = 1.5f;  // секундына неше оқ
const float TURRET_RANGE = 1000;  // пиксельмен
const float BULLET_SPEED = 700;  // пиксель/секунд
const float EXTRA_DAMAGE = 0.5f;  // балкондағы қосымша зеңбіректердің зияны (негізгі зеңбірекке қатысты)

// Белсенді қабілет: «Залп»
const float VOLLEY_COOLDOWN = 15.0f;  // секунд (дүкенде қысқартуға болады)
const int VOLLEY_BULLETS = 5;  // оқ саны әрқашан осынша, жау аз болса да (дүкенде көбейеді)
const float VOLLEY_DAMAGE = 15;
const float VOLLEY_SPREAD = 0.9f;  // желпуіштің жалпы бұрышы (радиан)
const float VOLLEY_TURN = 6.0f;  // оқтың нысанаға бұрылу жылдамдығы (үлкен болса — тезірек)

// Жау түрі. frames: Characters папкасындағы тайл нөмірлері (анимация кадрлары)
// Міндетті емес өрістер: size — сурет өлшемі, tint — түс беру,
// armor — турель оғының зиян көбейткіші (залп толық соғады), heal / split / summon — қабілеттер
struct EnemyType {
    std::vector<int> frames;
    int max_hp;
    float speed;  // пиксель/секунд
    int damage;  // бір соққының зияны
    float attack_rate;  // секундына неше соққы
    int reward_min, reward_max;  // өлтіргенде берілетін тиын: кездейсоқ (ең азы, ең көбі)
    bool flying;
    std::string death_sound;
    int size = 24 * SCALE;  // жау тайлы 24px
    Color tint = {255, 255, 255};
    float armor = 1.0f;
    int heal_amount = 0;  // 0 — емдемейді
    float heal_radius = 0;
    float heal_interval = 0;
    std::string split_kind = "";  // бос — бөлінбейді
    int split_count = 0;
    std::string summon_kind = "";  // бос — ешкімді шақырмайды
    int summon_count = 0;
    float summon_interval = 0;
};

inline const std::map<std::string, EnemyType> ENEMY_TYPES = {
    {"walker", {.frames = {15, 16}, .max_hp = 30, .speed = 60, .damage = 5, .attack_rate = 1.0f,
                .reward_min = 14, .reward_max = 26, .flying = false, .death_sound = "death_walker"}},
    {"bat", {.frames = {24, 25, 26, 25}, .max_hp = 20, .speed = 120, .damage = 3, .attack_rate = 1.5f,
             .reward_min = 20, .reward_max = 40, .flying = true, .death_sound = "death_bat"}},
    {"tank", {.frames = {21, 22}, .max_hp = 140, .speed = 30, .damage = 15, .attack_rate = 0.5f,
              .reward_min = 70, .reward_max = 130, .flying = false, .death_sound = "death_tank"}},
    // Рой: әлсіз, жылдам, топпен келеді
    {"swarm", {.frames = {18, 19}, .max_hp = 8, .speed = 100, .damage = 2, .attack_rate = 2.0f,
               .reward_min = 6, .reward_max = 14, .flying = false, .death_sound = "death_bat", .size = 40}},
    // Қалқанды: турель оғынан жартылай ғана зиян алады
    {"shield", {.frames = {11, 12}, .max_hp = 60, .speed = 40, .damage = 8, .attack_rate = 0.8f,
                .reward_min = 45, .reward_max = 75, .flying = false, .death_sound = "death_tank", .armor = 0.5f}},
    // Емші: маңайындағы жауларды емдейді
    {"healer", {.frames = {13, 14}, .max_hp = 35, .speed = 55, .damage = 3, .attack_rate = 1.0f,
                .reward_min = 45, .reward_max = 75, .flying = false, .death_sound = "death_walker",
                .heal_amount = 10, .heal_radius = 170, .heal_interval = 3.0f}},
    // Бөлінетін: өлгенде бірнеше кішкентай шарға бөлінеді
    {"splitter", {.frames = {8}, .max_hp = 60, .speed = 50, .damage = 6, .attack_rate = 1.0f,
                  .reward_min = 30, .reward_max = 50, .flying = false, .death_sound = "death_walker",
                  .split_kind = "splitling", .split_count = 3}},
    {"splitling", {.frames = {8}, .max_hp = 12, .speed = 90, .damage = 2, .attack_rate = 1.5f,
                   .reward_min = 6, .reward_max = 14, .flying = false, .death_sound = "death_bat", .size = 27}},
    // Босс: үлкен, баяу, жолда «рой» шақырады
    {"boss", {.frames = {21, 22}, .max_hp = 700, .speed = 20, .damage = 25, .attack_rate = 0.5f,
              .reward_min = 450, .reward_max = 750, .flying = false, .death_sound = "death_tank",
              .size = 100, .tint = {255, 130, 130},
              .summon_kind = "swarm", .summon_count = 2, .summon_interval = 8.0f}},
};

// Толқын. gap — бір жауға келетін орташа шығу аралығы (секунд), hp — жаулар HP көбейткіші,
// enemies — (жау түрі, саны), boss_hp — тек боссқа қосымша HP көбейткіші. Босс әрқашан толқынның басында шығады
// pack — бір топта ең көп неше жау шығады, spacing — бір түрлі жаулар арасындағы ең аз қашықтық, сурет енімен
struct Wave {
    float gap;
    float hp;
    int pack = 3;
    float spacing = 0.4f;  // 0.4 — тығызырақ жүреді
    float boss_hp = 1.0f;
    std::vector<std::pair<std::string, int>> enemies;
};

inline const std::vector<Wave> WAVES = {
    {.gap = 1.5f, .hp = 1.0f, .pack = 1, .spacing = 1.0f, .enemies = {{"walker", 6}}},
    {.gap = 0.8f, .hp = 1.0f, .pack = 1, .spacing = 1.0f, .enemies = {{"walker", 8}, {"bat", 5}}},
    {.gap = 0.8f, .hp = 1.0f, .pack = 1, .spacing = 1.0f, .enemies = {{"walker", 9}, {"bat", 6}, {"tank", 2}}},
    {.gap = 0.5f, .hp = 1.0f, .pack = 2, .spacing = 1.0f, .enemies = {{"walker", 8}, {"bat", 4}, {"swarm", 12}}},
    {.gap = 1.0f, .hp = 1.0f, .pack = 2, .spacing = 1.0f, .enemies = {{"boss", 1}, {"walker", 6}, {"swarm", 6}}},
    {.gap = 0.8f, .hp = 1.0f, .pack = 2, .spacing = 1.0f, .enemies = {{"walker", 8}, {"bat", 5}, {"shield", 4}}},
    {.gap = 0.8f, .hp = 1.05f, .enemies = {{"walker", 8}, {"shield", 3}, {"healer", 2}, {"tank", 2}}},
    {.gap = 0.8f, .hp = 1.1f, .enemies = {{"walker", 8}, {"bat", 6}, {"splitter", 4}}},
    {.gap = 0.6f, .hp = 1.15f, .enemies = {{"walker", 10}, {"swarm", 10}, {"shield", 4}, {"healer", 2}, {"splitter", 3}}},
    {.gap = 0.9f, .hp = 1.5f, .enemies = {{"boss", 1}, {"shield", 4}, {"healer", 2}, {"swarm", 8}}},
    {.gap = 0.6f, .hp = 1.25f, .enemies = {{"walker", 12}, {"bat", 8}, {"tank", 3}, {"shield", 4}}},
    {.gap = 0.5f, .hp = 1.3f, .enemies = {{"swarm", 18}, {"splitter", 5}, {"healer", 3}, {"bat", 6}}},
    {.gap = 0.65f, .hp = 1.25f, .enemies = {{"walker", 10}, {"shield", 4}, {"healer", 2}, {"tank", 3}, {"splitter", 3}}},
    {.gap = 0.55f, .hp = 1.3f, .enemies = {{"walker", 12}, {"bat", 8}, {"swarm", 10}, {"shield", 4}, {"healer", 2},
                                           {"splitter", 4}, {"tank", 3}}},
    {.gap = 0.6f, .hp = 1.6f, .boss_hp = 1.8f, .enemies = {{"boss", 1}, {"walker", 8}, {"shield", 3}, {"healer", 2},
                                                          {"tank", 2}, {"swarm", 8}}},
};

const float WAVE_PAUSE = 4.0f;  // бірінші толқынға дейінгі кері санақ, секунд

// Дүкен (толқындар арасында): key — game.stats ішіндегі көрсеткіш, step — бір деңгейдің өзгерісі
// group — дүкеннің қай қойындысында тұрады (SHOP_TABS)
const double COST_GROWTH = 1.5;  // әр деңгейде баға 1.5 есе өседі (cost_growth көрсетілмесе)

struct Upgrade {
    std::string key;
    std::string title;
    std::string group;
    double step;
    int base_cost;
    int max_level;
    double cost_growth = COST_GROWTH;
};

inline const std::vector<Upgrade> UPGRADES = {
    {.key = "damage", .title = "Урон", .group = "cannon", .step = 2, .base_cost = 100, .max_level = 10},
    {.key = "fire_rate", .title = "Скорострельность", .group = "cannon", .step = 0.2, .base_cost = 120, .max_level = 10},
    {.key = "max_hp", .title = "HP башни", .group = "tower", .step = 20, .base_cost = 80, .max_level = 10},
    {.key = "cannons", .title = "Новая пушка", .group = "tower", .step = 1, .base_cost = 2000, .max_level = 2,
     .cost_growth = 2.0},
    {.key = "volley_cooldown", .title = "Перезарядка залпа", .group = "volley", .step = -1, .base_cost = 100,
     .max_level = 10},
    {.key = "volley_bullets", .title = "Патроны залпа", .group = "volley", .step = 1, .base_cost = 120, .max_level = 10},
    // Навыктар: бір рет сатып алынады (0 — жоқ, 1 — бар)
    {.key = "freeze", .title = "Заморозка", .group = "skill", .step = 1, .base_cost = 600, .max_level = 1},
    {.key = "airstrike", .title = "Авиаудар", .group = "skill", .step = 1, .base_cost = 800, .max_level = 1},
    {.key = "shield", .title = "Щит", .group = "skill", .step = 1, .base_cost = 1000, .max_level = 1},
    {.key = "overdrive", .title = "Перегрев", .group = "skill", .step = 1, .base_cost = 1200, .max_level = 1},
};
// (кілт, жазуы)
inline const std::vector<std::pair<std::string, std::string>> SHOP_TABS = {
    {"cannon", "Пушки"}, {"tower", "Башня"}, {"volley", "Залп"}, {"skill", "Навыки"}};

// Навыктар (залптан басқа): cooldown — перезарядка, duration — әрекет ету уақыты (секунд)
struct Skill {
    float cooldown;
    float duration = 0;
    float slow = 1.0f;  // жаулар жылдамдығының көбейткіші
    float damage = 0;
    float radius = 0;
};

inline const std::map<std::string, Skill> SKILLS = {
    {"freeze", {.cooldown = 30.0f, .duration = 5.0f, .slow = 0.5f}},
    {"airstrike", {.cooldown = 25.0f, .damage = 50, .radius = 110}},  // бомба аймаққа зиян салады
    {"shield", {.cooldown = 40.0f, .duration = 3.0f}},  // мұнара зиян алмайды
    {"overdrive", {.cooldown = 35.0f, .duration = 6.0f}},  // зеңбіректер екі есе жиі атады
};

const float MUSIC_MAX = 0.08f;  // «Музыка» сырғытпасы 100% болғандағы нақты деңгей (файлдың өзі өте қатты)

// Параметрлер: ойын барысында ESC → «Параметры» мәзірінен өзгереді (ауыстырғыштар: 1 — қосулы, 0 — өшірулі)
inline std::map<std::string, float> options = {
    {"master", 1.0f},  // жалпы дыбыс деңгейі (0..1)
    {"music", 0.4f},  // музыка (0..1)
    {"sfx", 1.0f},  // эффектілер (0..1)
    {"sound_on", 1},
    {"fullscreen", 0},
    {"show_fps", 0},
};
