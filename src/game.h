#pragma once
// Ойынның өзгеретін деректері: жаулар, оқтар, эффектілер және бүкіл ойын күйі (Game)
#include <map>
#include <random>
#include <string>
#include <vector>
#include "render.h"

struct Enemy {
    int id;  // әр жаудың өз нөмірі: оқ, ұшақ, басымдық жауды осы нөмір арқылы табады
    std::string kind;  // ENEMY_TYPES ішіндегі атау
    float x;  // ортасы бойынша x
    float y;  // аяғы (төменгі шеті) бойынша y
    float base_y;
    double hp;
    double max_hp;
    float hp_multiplier;  // бөлінгенде/шақырғанда балаларына да беріледі
    float attack_timer;
    float ability_timer;  // емдеу/шақыру қабілетіне дейінгі уақыт
    float time;  // анимация мен тербелу үшін
    float flash;  // оқ тигенде ақ болып жыпылықтау уақыты
};

struct Bullet {
    float x, y;
    float dx, dy;  // ұшу бағыты (бірлік вектор)
    int target;  // нысана жаудың id-і, 0 — нысана жоқ
    double damage;
    bool pierce;  // залп оғы: сауыт әсер етпейді
    float turn;
};

// Кеңейетін сақина: жау өлгенде (ақ), емдегенде (жасыл), жарылғанда (сарғыш)
struct Effect {
    float x, y;
    float time;
    Color color;
    float grow;  // секундына неше пиксель өседі
};

// Өлген жаудың үстінен көтерілетін «+N» жазуы
struct Popup {
    float x, y;
    std::string text;
    float time;
};

// Мұнара ұясындағы модуль: type бос болса — бос ұя
struct Module {
    std::string type;
    float angle;
    float cooldown;
    float recoil;
};

struct Plane {
    float x;
    int target;
    float target_x;
    bool dropped;
};

struct Bomb {
    float x, y;
    float vy;
    int target;
};

// Толқын кестесінің бір жолы: қашан шығады, қандай жау
struct Spawn {
    float time;
    std::string kind;
};

struct Game {
    std::string state;  // pause (1-толқынға дейін) / wave / shop (толқындар арасы) / win / lose
    float pause_timer;
    int wave_index;
    float wave_time;
    std::vector<Spawn> spawn_queue;
    double tower_hp;
    int coins;
    float coin_pulse;  // тиын түскенде санағыштың жарқырау уақыты
    std::vector<Popup> popups;
    // Дүкенде жақсартылатын көрсеткіштер және олардың деңгейлері
    std::map<std::string, double> stats;
    std::map<std::string, int> levels;
    std::vector<Enemy> enemies;
    int next_enemy_id;
    std::vector<Bullet> bullets;
    std::vector<Effect> effects;
    int priority;  // ойыншы басып таңдаған жаудың id-і, 0 — жоқ
    std::vector<Module> modules;  // мұнара ұяларындағы модульдер (tower_slots ретімен)
    float cannon_glow;  // зеңбірек жаңа модельге өткенде жарқырау уақыты
    float tower_glow;  // мұнара жаңа түрге өткенде жарқырау уақыты
    float commander_jump;  // қолбасшы навык қолданғанда секіреді
    float launcher_recoil;  // залп қондырғысының тебу уақыты
    // Навыктар: перезарядка таймерлері және әрекет ету уақыттары
    std::map<std::string, float> skill_timers;
    float freeze_time;
    float shield_time;
    float overdrive_time;
    bool targeting;  // «Авиаудар» үшін жау таңдап жатырмыз ба
    std::vector<Plane> planes;
    std::vector<Bomb> bombs;
    std::string menu;  // "" / "pause" / "options" / "shop" (мәзір ашық болса ойын тоқтайды)
    std::string options_tab;  // параметрлердің ашық қойындысы
    std::string shop_tab;  // дүкеннің ашық қойындысы
    std::string drag;  // қазір тартылып жатқан сырғытпа
    std::string menu_hover;  // тышқан тұрған батырма (дыбыс үшін)
};

// Пернетақта мен тышқан оқиғасы (pygame.event сияқты)
struct Event {
    std::string type;  // quit / keydown / mousedown / mouseup / motion
    int key;  // GLFW_KEY_...
    int button;  // 1 — сол жақ, 3 — оң жақ (pygame сияқты)
    float x, y;  // ойын координатасындағы тышқан орны
};

// main.cpp ішінде: тышқан орны, уақыт (мс), толық экранды ауыстыру
extern float mouse_x, mouse_y;
int get_ticks();
void toggle_fullscreen();

// Кездейсоқ сандар (Python random модулі сияқты)
extern std::mt19937 rng;
int random_int(int a, int b);  // a..b, екеуі де кіреді
float random_float(float a, float b);
