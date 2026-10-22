#include "turret.h"
#include <algorithm>
#include <cmath>
#include "enemies.h"
#include "settings.h"
#include "sounds.h"
#include "tower.h"


float distance_to(const Enemy& enemy, Slot slot) {  // slot: мұнара ұясы (tower_slots)
    glm::vec2 center = enemy_center(enemy);
    return std::hypot(center.x - slot.x, center.y - slot.y);
}


// Нысана таңдау: алдымен ойыншы басқан жау, болмаса осы ұяға ең жақын жау (id, 0 — жоқ)
int choose_target(Game& game, Slot slot) {
    Enemy* priority = find_enemy(game, game.priority);
    if (priority != nullptr && distance_to(*priority, slot) <= TURRET_RANGE) {
        return priority->id;
    }

    int best = 0;
    float best_distance = TURRET_RANGE;
    for (const Enemy& enemy : game.enemies) {
        float distance = distance_to(enemy, slot);
        if (distance <= best_distance) {
            best = enemy.id;
            best_distance = distance;
        }
    }
    return best;
}


// pierce: true болса қалқанды жауға толық зиян; spread: бастапқы бағытқа қосылатын бұрыш;
// turn: 0 — оқ бірден нысанаға бұрылады, әйтпесе біртіндеп (залп желпуіші көрінуі үшін)
void fire_bullet(Game& game, Slot slot, int target, double damage, bool pierce, float spread, float turn) {
    glm::vec2 center = enemy_center(*find_enemy(game, target));
    float angle = std::atan2(center.y - slot.y, center.x - slot.x) + spread;
    Bullet bullet;
    bullet.x = slot.x + std::cos(angle) * 13 * SCALE;  // оқ ұңғының ұшынан шығады
    bullet.y = slot.y + std::sin(angle) * 13 * SCALE;
    bullet.dx = std::cos(angle);
    bullet.dy = std::sin(angle);
    bullet.target = target;
    bullet.damage = damage;
    bullet.pierce = pierce;
    bullet.turn = turn;
    game.bullets.push_back(bullet);
}


// Әр ұядағы модуль өз нысанасын таңдап, өзі атады
void update_turret(Game& game, float dt) {
    game.launcher_recoil = std::fmax(0.0f, game.launcher_recoil - dt);

    std::vector<Slot> slots = tower_slots((int)game.stats["cannons"]);
    for (int i = 0; i < (int)game.modules.size(); i++) {
        Module& module = game.modules[i];
        if (module.type == "") {
            continue;
        }
        Slot slot = slots[i];
        module.cooldown -= dt;
        module.recoil = std::fmax(0.0f, module.recoil - dt);
        int target = choose_target(game, slot);
        if (target == 0) {
            continue;
        }

        // Ұңғыны нысанаға бұрып, дайын болса атамыз
        glm::vec2 center = enemy_center(*find_enemy(game, target));
        module.angle = std::atan2(center.y - slot.y, center.x - slot.x);
        if (module.cooldown <= 0) {
            double damage = game.stats["damage"];
            if (i > 0) {
                damage = damage * EXTRA_DAMAGE;  // балкондағы зеңбірек әлсізірек
            }
            fire_bullet(game, slot, target, damage, false, 0.0f, 0.0f);
            play("shoot");
            double fire_rate = game.stats["fire_rate"];
            if (game.overdrive_time > 0) {
                fire_rate = fire_rate * 2;  // «Перегрев» навыгы
            }
            module.cooldown = 1 / fire_rate;
            module.recoil = RECOIL_TIME;
        }
    }
}


// «Залп»: әрқашан бірдей оқ саны, желпуіш болып ұшып, ең жақын жауларға бөлінеді
void use_volley(Game& game) {  // дайындығы мен перезарядкасын skills.cpp басқарады
    // Залп бөлек қондырғыдан атылады (мұнараның оң қабырғасында)
    glm::vec2 position = launcher_position((int)game.stats["cannons"]);
    Slot main_slot = {position.x, position.y, false};
    std::vector<Enemy> targets = game.enemies;
    std::stable_sort(targets.begin(), targets.end(), [&](const Enemy& a, const Enemy& b) {
        return distance_to(a, main_slot) < distance_to(b, main_slot);
    });
    if (targets.empty()) {
        return;
    }
    int count = (int)game.stats["volley_bullets"];
    for (int i = 0; i < count; i++) {
        // Желпуіш: оқтар бұрыштары бойынша біркелкі таралады (ортасы — нысанаға)
        float spread = 0.0f;
        if (count > 1) {
            spread = ((float)i / (count - 1) - 0.5f) * VOLLEY_SPREAD;
        }
        int target = targets[i % targets.size()].id;  // жау аз болса, бір жауға бірнеше оқ
        fire_bullet(game, main_slot, target, VOLLEY_DAMAGE, true, spread, VOLLEY_TURN);
    }
    play("volley");
    game.launcher_recoil = LAUNCHER_RECOIL_TIME;
}


void update_bullets(Game& game, float dt) {
    float speed = BULLET_SPEED;
    for (int i = 0; i < (int)game.bullets.size();) {
        Bullet& bullet = game.bullets[i];

        // 1. Нысана тірі болса, оқ соның артынан бұрылады (бірден немесе біртіндеп)
        Enemy* target = find_enemy(game, bullet.target);
        if (target != nullptr) {
            glm::vec2 center = enemy_center(*target);
            float length = std::hypot(center.x - bullet.x, center.y - bullet.y);
            if (length > 0) {
                float want_dx = (center.x - bullet.x) / length;
                float want_dy = (center.y - bullet.y) / length;
                if (bullet.turn == 0) {
                    bullet.dx = want_dx;
                    bullet.dy = want_dy;
                } else {
                    float k = std::fmin(1.0f, bullet.turn * dt);
                    float dx = bullet.dx + (want_dx - bullet.dx) * k;
                    float dy = bullet.dy + (want_dy - bullet.dy) * k;
                    float size = std::hypot(dx, dy);
                    bullet.dx = dx / size;
                    bullet.dy = dy / size;
                }
            }
        } else {
            bullet.target = 0;
        }

        bullet.x += bullet.dx * speed * dt;
        bullet.y += bullet.dy * speed * dt;

        // 2. Тию: нысанаға немесе (нысана өлсе) кез келген жауға
        int hit = 0;
        if (target != nullptr) {
            if (rect_contains(enemy_rect(*target), bullet.x, bullet.y)) {
                hit = target->id;
            }
        } else {
            for (const Enemy& enemy : game.enemies) {
                if (rect_contains(enemy_rect(enemy), bullet.x, bullet.y)) {
                    hit = enemy.id;
                    break;
                }
            }
        }
        if (hit != 0) {
            damage_enemy(game, hit, bullet.damage, bullet.pierce);
            game.bullets.erase(game.bullets.begin() + i);
            continue;
        }

        // 3. Экраннан шықса, өшіреміз
        if (bullet.x < 0 || bullet.x > WIDTH || bullet.y < 0 || bullet.y > HEIGHT) {
            game.bullets.erase(game.bullets.begin() + i);
            continue;
        }
        i++;
    }
}


void draw_bullets(const std::vector<Bullet>& bullets) {
    float size = SCALE / 3.0f;  // оқтың өлшемі масштабқа қарай
    for (const Bullet& bullet : bullets) {
        // Залп оғы (pierce) қызғылт-сарғыш, кәдімгі оқ — сары
        if (bullet.pierce) {
            draw_circle(bullet.x, bullet.y, 8 * size, {120, 30, 0});
            draw_circle(bullet.x, bullet.y, 6 * size, {255, 120, 40});
        } else {
            draw_circle(bullet.x, bullet.y, 7 * size, {120, 60, 0});
            draw_circle(bullet.x, bullet.y, 5 * size, {255, 210, 60});
        }
    }
}
