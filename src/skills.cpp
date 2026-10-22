#include "skills.h"
#include <cmath>
#include "enemies.h"
#include "settings.h"
#include "sounds.h"
#include "tower.h"
#include "turret.h"

const float SQUARE = 92;  // навык шаршысының өлшемі
const float ICON_BASE = 64;  // суреттер осы өлшемде салынып, шаршыға дейін үлкейтіледі
const float GAP = 14;
const float BAR_Y = HEIGHT - SQUARE - 8;
const float PLANE_Y = 125;  // ұшақтың ұшу биіктігі
const float PLANE_SPEED = 380;  // пиксель/секунд
const Color OUTLINE = {40, 30, 45};
const float PI = 3.14159265f;


// Ойыншыда бар навыктар: залп әрқашан, қалғандары дүкенде сатып алынса
std::vector<std::string> owned_skills(const Game& game) {
    std::vector<std::string> owned;
    for (const std::string& key : SKILL_ORDER) {
        if (key == "volley" || game.stats.at(key) > 0) {
            owned.push_back(key);
        }
    }
    return owned;
}


float skill_cooldown(const Game& game, const std::string& key) {
    if (key == "volley") {
        return game.stats.at("volley_cooldown");  // залп перезарядкасы дүкенде қысқарады
    }
    return SKILLS.at(key).cooldown;
}


// Шаршылар экранның ортасына тураланады: біреу болса дәл ортада, көп болса екі жаққа тарайды
Rect skill_rect(int i, int count) {
    float total = count * SQUARE + (count - 1) * GAP;
    float x = WIDTH / 2 - (int)total / 2 + i * (SQUARE + GAP);
    return {x, BAR_Y, SQUARE, SQUARE};
}


void cast_skill(Game& game, const std::string& key) {
    // Навык тек толқын кезінде және перезарядка біткенде қолданылады
    if (game.state != "wave" || game.skill_timers[key] > 0 || game.enemies.empty()) {
        play("error");
        return;
    }
    if (key == "airstrike") {
        game.targeting = true;  // перезарядка жау таңдалғанда басталады
        play("click");
        return;
    }
    if (key == "volley") {
        use_volley(game);
    } else if (key == "freeze") {
        game.freeze_time = SKILLS.at("freeze").duration;
        play("freeze");
    } else if (key == "shield") {
        game.shield_time = SKILLS.at("shield").duration;
        play("shield");
    } else if (key == "overdrive") {
        game.overdrive_time = SKILLS.at("overdrive").duration;
        play("overdrive");
    }
    game.skill_timers[key] = skill_cooldown(game, key);
    game.commander_jump = COMMANDER_JUMP_TIME;
}


// Авиасоққы: басылған жауға ұшақ жібереміз; басылса true
bool choose_airstrike_target(Game& game, float x, float y) {
    Enemy* enemy = find_enemy(game, enemy_under(game, x, y));
    if (enemy == nullptr) {
        return false;
    }
    game.planes.push_back({-80.0f, enemy->id, enemy->x, false});
    game.targeting = false;
    game.skill_timers["airstrike"] = skill_cooldown(game, "airstrike");
    game.commander_jump = COMMANDER_JUMP_TIME;
    play("plane");
    return true;
}


// Төменгі тақтадағы шаршыны басу; басылса true
bool skill_bar_click(Game& game, float x, float y) {
    std::vector<std::string> owned = owned_skills(game);
    for (int i = 0; i < (int)owned.size(); i++) {
        if (rect_contains(skill_rect(i, (int)owned.size()), x, y)) {
            cast_skill(game, owned[i]);
            return true;
        }
    }
    return false;
}


void explode(Game& game, float x, float y) {
    const Skill& data = SKILLS.at("airstrike");
    for (int id : enemy_ids(game)) {
        Enemy* enemy = find_enemy(game, id);
        if (enemy == nullptr) {
            continue;
        }
        glm::vec2 center = enemy_center(*enemy);
        if (std::hypot(center.x - x, center.y - y) <= data.radius) {
            damage_enemy(game, id, data.damage, true);
        }
    }
    game.effects.push_back({x, y, 0.0f, {255, 150, 40}, data.radius / 0.3f});
    play("explosion");
}


void update_skills(Game& game, float dt) {
    // 1. Таймерлер
    for (auto& [key, timer] : game.skill_timers) {
        timer = std::fmax(0.0f, timer - dt);
    }
    game.freeze_time = std::fmax(0.0f, game.freeze_time - dt);
    game.shield_time = std::fmax(0.0f, game.shield_time - dt);
    game.overdrive_time = std::fmax(0.0f, game.overdrive_time - dt);
    if (game.state != "wave") {
        game.targeting = false;
    }

    // 2. Ұшақтар: нысананың үстіне жеткенде бомба тастайды
    for (int i = 0; i < (int)game.planes.size();) {
        Plane& plane = game.planes[i];
        plane.x += PLANE_SPEED * dt;
        Enemy* target = find_enemy(game, plane.target);
        if (target != nullptr) {
            plane.target_x = target->x;
        }
        if (!plane.dropped && plane.x >= plane.target_x) {
            plane.dropped = true;
            game.bombs.push_back({plane.x, PLANE_Y + 12, 0.0f, plane.target});
            play("bomb_fall");
        }
        if (plane.x > WIDTH + 100) {
            game.planes.erase(game.planes.begin() + i);
            continue;
        }
        i++;
    }

    // 3. Бомбалар: құлап, нысанаға қарай ауытқиды (жіберіп алмау үшін), жеткенде жарылады
    for (int i = 0; i < (int)game.bombs.size();) {
        Bomb& bomb = game.bombs[i];
        bomb.vy += 900 * dt;
        bomb.y += bomb.vy * dt;
        float target_y = GROUND_Y - 20;
        Enemy* target = find_enemy(game, bomb.target);
        if (target != nullptr) {
            glm::vec2 center = enemy_center(*target);
            target_y = center.y;
            bomb.x += (center.x - bomb.x) * std::fmin(1.0f, 8 * dt);
        }
        if (bomb.y >= target_y) {
            float x = bomb.x, y = bomb.y;
            game.bombs.erase(game.bombs.begin() + i);
            explode(game, x, y);
            continue;
        }
        i++;
    }
}


void draw_plane(float x) {
    float y = PLANE_Y;
    draw_ellipse({x - 36, y - 10, 72, 22}, OUTLINE);  // денесі
    draw_ellipse({x - 33, y - 7, 66, 16}, {110, 130, 90});
    draw_rect({x - 28, y - 6, 50, 4}, {150, 170, 120});  // жарық жиек
    draw_polygon({{x - 34, y - 4}, {x - 44, y - 22}, {x - 26, y - 4}}, OUTLINE);  // құйрығы
    draw_polygon({{x - 6, y + 2}, {x + 14, y + 2}, {x - 10, y + 20}}, OUTLINE);  // қанаты
    draw_polygon({{x - 4, y + 4}, {x + 10, y + 4}, {x - 8, y + 16}}, {90, 108, 72});
    draw_circle(x + 18, y - 2, 5, {170, 220, 240});  // кабина
    draw_line(x + 37, y - 10, x + 37, y + 12, OUTLINE, 3);  // пропеллер
    draw_circle(x - 8, y, 4, {200, 60, 50});  // белгі
}


void draw_bomb(const Bomb& bomb) {
    float x = (int)bomb.x, y = (int)bomb.y;
    draw_polygon({{x - 6, y - 14}, {x + 6, y - 14}, {x, y - 6}}, OUTLINE);  // қанатшалары
    draw_ellipse({x - 7, y - 10, 14, 20}, OUTLINE);
    draw_ellipse({x - 5, y - 8, 10, 16}, {70, 72, 80});
}


// Әлемдегі навык эффектілері: қалқан күмбезі, ұшақтар, бомбалар
void draw_skill_world(const Game& game) {
    if (game.shield_time > 0) {
        float height = TOWER_LEVELS[(int)game.stats.at("cannons") - 1] * TILE + 70;
        Rect dome = {TOWER_X - 30, GROUND_Y - height, TOWER_RIGHT - TOWER_X + 120, height * 2};
        set_clip({dome.x, dome.y, dome.w, height});  // тек жоғарғы жартысы
        draw_ellipse(dome, {90, 200, 255, 60});
        draw_ellipse(dome, {150, 230, 255, 200}, 4);
        reset_clip();
    }
    for (const Plane& plane : game.planes) {
        draw_plane((int)plane.x);
    }
    for (const Bomb& bomb : game.bombs) {
        draw_bomb(bomb);
    }
}


// Навык суреттері (64x64 шаршы ішінде, ортасы — cx, cy)
void draw_icon(const std::string& key) {
    float cx = ICON_BASE / 2, cy = ICON_BASE / 2;
    if (key == "volley") {
        for (int k = 0; k < 3; k++) {
            float a = -0.5f + k * 0.5f;
            float end_x = cx - 14 + std::cos(a) * 30, end_y = cy + 10 + std::sin(a) * 30;
            draw_line(cx - 14, cy + 10, end_x, end_y, {255, 200, 120}, 2);
            draw_circle(end_x, end_y, 5, {255, 120, 40});
        }
    } else if (key == "freeze") {
        for (int k = 0; k < 3; k++) {
            float a = k * PI / 3;
            float dx = std::cos(a) * 20, dy = std::sin(a) * 20;
            draw_line(cx - dx, cy - dy, cx + dx, cy + dy, {190, 235, 255}, 4);
        }
        draw_circle(cx, cy, 5, {255, 255, 255});
    } else if (key == "airstrike") {
        draw_polygon({{cx - 22, cy - 10}, {cx + 18, cy - 10}, {cx + 22, cy - 6}, {cx + 18, cy - 2}, {cx - 22, cy - 2}},
                     {150, 170, 120});
        draw_polygon({{cx - 4, cy - 4}, {cx + 6, cy - 4}, {cx - 8, cy + 6}}, {120, 140, 95});
        draw_ellipse({cx - 4, cy + 6, 9, 14}, {60, 60, 70});
    } else if (key == "shield") {
        std::vector<glm::vec2> points = {{cx, cy - 22}, {cx + 18, cy - 14}, {cx + 15, cy + 8},
                                         {cx, cy + 22}, {cx - 15, cy + 8}, {cx - 18, cy - 14}};
        draw_polygon(points, {90, 200, 255});
        draw_polygon(points, {220, 245, 255}, 3);
    } else if (key == "overdrive") {
        draw_polygon({{cx, cy - 24}, {cx + 14, cy - 2}, {cx + 10, cy + 18}, {cx, cy + 22}, {cx - 10, cy + 18}, {cx - 14, cy - 2}},
                     {255, 110, 40});
        draw_polygon({{cx, cy - 6}, {cx + 7, cy + 8}, {cx, cy + 18}, {cx - 7, cy + 8}}, {255, 220, 90});
    }
}


// Төменгі навык тақтасы: шаршы, сурет, перне нөмірі, сағат тілі сияқты перезарядка
void draw_skill_bar(const Font& font, const Game& game) {
    std::vector<std::string> owned = owned_skills(game);
    std::map<std::string, float> active = {
        {"freeze", game.freeze_time}, {"shield", game.shield_time}, {"overdrive", game.overdrive_time}};
    for (int i = 0; i < (int)owned.size(); i++) {
        std::string key = owned[i];
        Rect rect = skill_rect(i, (int)owned.size());
        draw_rect(rect, {35, 38, 60}, 0, 8);
        // Суретті 64x64 өлшемде салып, модель матрицасымен шаршы өлшеміне үлкейтеміз
        set_model(translate(rect.x, rect.y) * scale(SQUARE / ICON_BASE));
        draw_icon(key);
        reset_model();

        // Перезарядка: қалған бөлігі күңгірт, «сағат тілі» бір айналым жасағанда дайын
        float timer = game.skill_timers.at(key);
        if (timer > 0) {
            float progress = 1 - timer / skill_cooldown(game, key);  // 0 → 1
            float hand = -PI / 2 + progress * 2 * PI;  // сағат тілінің бұрышы (12-ден бастап)
            float c_x = rect.x + SQUARE / 2, c_y = rect.y + SQUARE / 2;
            std::vector<glm::vec2> points = {{c_x, c_y}};
            int steps = 24;
            for (int s = 0; s <= steps; s++) {
                float a = hand + (2 * PI * (1 - progress)) * s / steps;
                points.push_back({c_x + std::cos(a) * SQUARE, c_y + std::sin(a) * SQUARE});
            }
            set_clip(rect);  // шаршыдан шықпасын
            draw_polygon(points, {0, 0, 0, 150});
            draw_line(c_x, c_y, c_x + std::cos(hand) * SQUARE, c_y + std::sin(hand) * SQUARE, {255, 255, 255}, 2);
            reset_clip();
        }

        // Жиегі: әрекет етіп тұрса көгілдір, жау таңдап жатса сары, әйтпесе дайын/дайын емес
        Color border;
        if (active.count(key) && active[key] > 0) {
            border = {120, 220, 255};
        } else if (key == "airstrike" && game.targeting) {
            border = {255, 210, 60};
        } else if (timer == 0) {
            border = {230, 230, 240};
        } else {
            border = {90, 90, 110};
        }
        draw_rect(rect, border, 3, 8);

        draw_text(font, std::to_string(i + 1), rect.x + 5, rect.y + 2, {255, 255, 255});
    }

    if (game.targeting) {
        std::string hint = "Выберите врага для авиаудара  (ПКМ — отмена)";
        draw_text(font, hint, WIDTH / 2 + 2, BAR_Y - 22 + 2, {40, 30, 20}, 0.5f, 0.5f);
        draw_text(font, hint, WIDTH / 2, BAR_Y - 22, {255, 230, 120}, 0.5f, 0.5f);
    }
}
