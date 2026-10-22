#include "enemies.h"
#include <cmath>
#include "settings.h"
#include "sounds.h"


int enemy_size(const std::string& kind) {
    return ENEMY_TYPES.at(kind).size;
}


Enemy create_enemy(Game& game, const std::string& kind, float hp_multiplier) {  // kind: ENEMY_TYPES ішіндегі атау, hp_multiplier: HP көбейткіші
    const EnemyType& data = ENEMY_TYPES.at(kind);
    int max_hp = (int)(data.max_hp * hp_multiplier);
    // Ұшатын жау ауада, қалғандары жерде — кездейсоқ жолақпен (босс ортаңғы жолақта)
    float base_y;
    if (data.flying) {
        base_y = random_int(GROUND_Y - 330, GROUND_Y - 170);
    } else {
        int lane = kind == "boss" ? 1 : random_int(0, LANES - 1);
        base_y = GROUND_Y + lane * LANE_GAP;
    }
    Enemy enemy;
    enemy.id = game.next_enemy_id;
    game.next_enemy_id += 1;
    enemy.kind = kind;
    enemy.x = WIDTH + enemy_size(kind);
    enemy.y = base_y;
    enemy.base_y = base_y;
    enemy.hp = max_hp;
    enemy.max_hp = max_hp;
    enemy.hp_multiplier = hp_multiplier;
    enemy.attack_timer = 0.0f;
    enemy.ability_timer = 2.0f;
    enemy.time = random_float(0, 10);
    enemy.flash = 0.0f;
    return enemy;
}


Rect enemy_rect(const Enemy& enemy) {
    int size = enemy_size(enemy.kind);
    int hit = (int)(size * 0.84);  // соғылу аймағы суреттен сәл кішірек
    return {enemy.x - hit / 2, enemy.y - hit, (float)hit, (float)hit};
}


glm::vec2 enemy_center(const Enemy& enemy) {
    return {enemy.x, enemy.y - enemy_size(enemy.kind) * 0.45f};
}


// Жау әлі тірі ме (тізімде бар ма) — id бойынша іздейміз
Enemy* find_enemy(Game& game, int id) {
    for (Enemy& enemy : game.enemies) {
        if (enemy.id == id) {
            return &enemy;
        }
    }
    return nullptr;
}


std::vector<int> enemy_ids(const Game& game) {
    std::vector<int> ids;
    for (const Enemy& enemy : game.enemies) {
        ids.push_back(enemy.id);
    }
    return ids;
}


// Тышқан астындағы жау: бірнешеуі болса — ортасы курсорға ең жақыны; ешкім болмаса 0
int enemy_under(Game& game, float x, float y) {
    int best = 0;
    float best_distance = 0;
    for (const Enemy& enemy : game.enemies) {
        if (rect_contains(inflate(enemy_rect(enemy), 16, 16), x, y)) {
            glm::vec2 center = enemy_center(enemy);
            float distance = std::hypot(center.x - x, center.y - y);
            if (best == 0 || distance < best_distance) {
                best = enemy.id;
                best_distance = distance;
            }
        }
    }
    return best;
}


// Жаңа жауды берілген жерге қою (бөліну және шақыру үшін)
void spawn_near(Game& game, const std::string& kind, float x, float hp_multiplier) {
    Enemy child = create_enemy(game, kind, hp_multiplier);
    child.x = x;
    game.enemies.push_back(child);
}


void damage_enemy(Game& game, int id, double amount, bool pierce) {  // pierce: true болса сауыт (armor) әсер етпейді
    Enemy* enemy = find_enemy(game, id);
    if (enemy == nullptr) {
        play("hit");
        return;
    }
    const EnemyType& data = ENEMY_TYPES.at(enemy->kind);
    if (!pierce) {
        amount = amount * data.armor;
    }
    enemy->hp -= amount;
    enemy->flash = 0.1f;
    if (enemy->hp > 0) {
        play("hit");
        return;
    }

    // Жау өлді: тізімнен алып тастаймыз (көшірмесі қалады), тиын, эффект, дыбыс
    Enemy dead = *enemy;
    game.enemies.erase(game.enemies.begin() + (enemy - game.enemies.data()));
    int reward = random_int(data.reward_min, data.reward_max);
    game.coins += reward;
    game.coin_pulse = 0.25f;  // санағыш біраз уақыт жарқырайды
    glm::vec2 center = enemy_center(dead);
    game.popups.push_back({center.x, center.y - 30, "+" + std::to_string(reward), 0.0f});
    game.effects.push_back({center.x, center.y, 0.0f, {255, 255, 255}, 120});
    play(data.death_sound);
    play("coin");

    // Бөлінетін жау кішкентай жауларға бөлінеді
    for (int i = 0; i < data.split_count; i++) {
        spawn_near(game, data.split_kind, dead.x + (i - 1) * 30, dead.hp_multiplier);
    }
}


// Емші: радиус ішіндегі жаулардың HP-ын толтырады
void use_heal(Game& game, Enemy healer, const EnemyType& data) {
    for (Enemy& other : game.enemies) {
        if (std::hypot(other.x - healer.x, other.y - healer.y) <= data.heal_radius) {
            other.hp = std::fmin(other.max_hp, other.hp + data.heal_amount);
        }
    }
    glm::vec2 center = enemy_center(healer);
    game.effects.push_back({center.x, center.y, 0.0f, {80, 230, 120}, data.heal_radius / 0.3f});
}


void update_enemies(Game& game, float dt) {
    // «Заморозка» кезінде жаулар баяу жүріп, сирек соғады
    float slow = game.freeze_time > 0 ? SKILLS.at("freeze").slow : 1.0f;
    int count = (int)game.enemies.size();  // осы кадрда шақырылған жаулар келесі кадрдан бастап жүреді
    for (int i = 0; i < count; i++) {
        Enemy& enemy = game.enemies[i];
        const EnemyType& data = ENEMY_TYPES.at(enemy.kind);
        enemy.time += dt;
        enemy.flash = std::fmax(0.0f, enemy.flash - dt);

        // 1. Мұнараға дейін жүру, жеткен соң соғу (үлкен жау алысырақ тоқтайды)
        float stop_x = TOWER_RIGHT + enemy_size(enemy.kind) * 0.42f;
        if (enemy.x > stop_x) {
            enemy.x = std::fmax(stop_x, enemy.x - data.speed * slow * dt);
        } else {
            enemy.attack_timer -= dt * slow;
            if (enemy.attack_timer <= 0) {
                // «Щит» кезінде мұнара зиян алмайды
                if (game.shield_time > 0) {
                    play("hit");
                } else {
                    game.tower_hp -= data.damage;
                    play("tower_hit");
                }
                enemy.attack_timer = 1 / data.attack_rate;
            }
        }

        // 2. Ұшатын жау жоғары-төмен тербеледі
        if (data.flying) {
            enemy.y = enemy.base_y + std::sin(enemy.time * 3) * 15;
        }

        // 3. Қабілеттер: емдеу немесе көмекші шақыру (экранға кірген соң ғана)
        if (enemy.x < WIDTH && (data.heal_amount > 0 || data.summon_count > 0)) {
            enemy.ability_timer -= dt;
            if (enemy.ability_timer <= 0) {
                if (data.heal_amount > 0) {
                    enemy.ability_timer = data.heal_interval;
                    use_heal(game, enemy, data);
                } else {
                    // Жаңа жау тізімге қосылған соң enemy сілтемесі ескіреді, сондықтан алдымен таймер
                    enemy.ability_timer = data.summon_interval;
                    float x = enemy.x;
                    float multiplier = enemy.hp_multiplier;
                    for (int k = 0; k < data.summon_count; k++) {
                        spawn_near(game, data.summon_kind, x + 40 + k * 30, multiplier);
                    }
                }
            }
        }
    }
}


// frozen: «Заморозка» кезінде көгілдір; hover: тышқан астында болса hover_color түсті контур
void draw_enemy(const Images& images, const Enemy& enemy, bool is_priority, bool frozen, bool hover, Color hover_color) {
    const EnemyType& data = ENEMY_TYPES.at(enemy.kind);
    float size = enemy_size(enemy.kind);
    // Жердегі жаудың аяғының астында көлеңке — қай жолақта екені көрінеді
    if (!data.flying) {
        float shadow_w = (int)(size * 0.7f);
        draw_ellipse({enemy.x - shadow_w / 2, enemy.y - 2 - 4, shadow_w, 8}, {0, 0, 0, 70});
    }
    const std::vector<Texture>& frames = images.enemies.at(enemy.kind);
    const Texture& frame = frames[(int)(enemy.time * 6) % frames.size()];
    // Қосылатын түс: «Заморозка» кезінде көгілдір, оқ тиген сәтте ағарады
    Color add = {0, 0, 0};
    if (frozen) {
        add = {0, 50, 130};
    }
    if (enemy.flash > 0) {
        add = {add.r + 120, add.g + 120, add.b + 120};
    }
    float left = enemy.x - (int)size / 2;
    float top = enemy.y - size;
    // Контур: пішінді бір түспен 8 бағытқа 2 пиксель жылжытып саламыз, үстінен — жаудың өзі
    if (hover) {
        for (int dx = -2; dx <= 2; dx += 2) {
            for (int dy = -2; dy <= 2; dy += 2) {
                draw_silhouette(frame, left + dx, top + dy, size, size, hover_color, true);
            }
        }
    }
    // Суреттер оңға қарайды, жаулар солға жүреді — көлденең айналдырамыз; tint — түс беру (мысалы, босс қызғылт)
    draw_texture(frame, left, top, size, size, data.tint, add, true);

    // Жарақат алса, HP жолағын көрсетеміз
    float bar_top = enemy.y - size - 4;
    if (enemy.hp < enemy.max_hp) {
        float width = 36;
        draw_rect({enemy.x - width / 2, bar_top, width, 6}, {60, 20, 20});
        draw_rect({enemy.x - width / 2, bar_top, (float)(int)(width * enemy.hp / enemy.max_hp), 6}, {230, 60, 60});
    }

    // Басымдықты нысананың үстінде сары үшбұрыш
    if (is_priority) {
        float x = enemy.x;
        float y = bar_top - 10;
        draw_polygon({{x - 10, y - 12}, {x + 10, y - 12}, {x, y}}, {255, 210, 0});
        draw_polygon({{x - 10, y - 12}, {x + 10, y - 12}, {x, y}}, {90, 60, 0}, 2);
    }
}
