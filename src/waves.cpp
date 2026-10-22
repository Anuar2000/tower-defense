#include "waves.h"
#include <algorithm>
#include "enemies.h"
#include "settings.h"
#include "sounds.h"


// Толқынның шығу кестесі: (шығатын уақыты, жау түрі), уақыт бойынша сұрыпталған
std::vector<Spawn> build_spawn_queue(int wave_index) {
    const Wave& info = WAVES[wave_index];

    // 1. Толқынның барлық жауын бір тізімге жинап, араластырамыз (босс бөлек — ол бірінші шығады)
    std::vector<Spawn> queue;
    std::vector<std::string> pool;
    for (const auto& [kind, count] : info.enemies) {
        if (kind == "boss") {
            queue.push_back({0.5f, kind});
        } else {
            for (int i = 0; i < count; i++) {
                pool.push_back(kind);
            }
        }
    }
    std::shuffle(pool.begin(), pool.end(), rng);

    // 2. Топ-тобымен шығарамыз: бір топта 1..pack жау, топтар арасындағы үзіліс кездейсоқ
    float time = 0.0f;
    std::map<std::string, float> last_spawn;  // әр түрдің соңғы шыққан уақыты
    int next = 0;  // pool ішіндегі келесі жау
    while (next < (int)pool.size()) {
        int size = random_int(1, info.pack);
        for (int j = 0; j < size && next + j < (int)pool.size(); j++) {
            std::string kind = pool[next + j];
            float spawn = time + j * 0.45f + random_float(0, 0.2f);  // топ ішінде де аздап кезекпен
            // Бір түрлі жаулар бір-бірінің үстіне шықпасын: алдыңғысы біраз қашықтыққа кетуі керек
            if (last_spawn.count(kind)) {
                float distance = enemy_size(kind) * info.spacing;
                float earliest = last_spawn[kind] + distance / ENEMY_TYPES.at(kind).speed;
                if (spawn < earliest) {
                    time += earliest - spawn;  // жау кешіксе, келесі топтар да сонша кешігеді
                    spawn = earliest;
                }
            }
            last_spawn[kind] = spawn;
            queue.push_back({spawn, kind});
        }
        next += size;
        time += size * info.gap * random_float(0.6f, 1.4f);
    }

    std::stable_sort(queue.begin(), queue.end(), [](const Spawn& a, const Spawn& b) { return a.time < b.time; });
    return queue;
}


void start_wave(Game& game) {
    game.spawn_queue = build_spawn_queue(game.wave_index);
    game.wave_time = 0.0f;
    game.state = "wave";
    game.menu = "";  // дүкен ашық болса, жабамыз
    game.skill_timers["volley"] = 0.0f;  // жаңа толқында залп бірден дайын
    play("wave_start");
}


void update_waves(Game& game, float dt) {
    // Бірінші толқынға дейінгі кері санақ
    if (game.state == "pause") {
        game.pause_timer -= dt;
        if (game.pause_timer <= 0) {
            start_wave(game);
        }
        return;
    }

    // Толқындар арасы: ойыншы «Следующая волна» басқанша күтеміз
    if (game.state == "shop") {
        return;
    }

    // Толқын: уақыты келген жауларды шығарамыз
    game.wave_time += dt;
    while (!game.spawn_queue.empty() && game.spawn_queue[0].time <= game.wave_time) {
        std::string kind = game.spawn_queue[0].kind;
        game.spawn_queue.erase(game.spawn_queue.begin());
        const Wave& info = WAVES[game.wave_index];
        float multiplier = info.hp;
        if (kind == "boss") {
            multiplier = multiplier * info.boss_hp;  // финалдық босс қосымша күшті
        }
        game.enemies.push_back(create_enemy(game, kind, multiplier));
    }

    // Барлық жау шығып, бәрі өлсе — дүкен ашылады, соңғы толқыннан кейін — жеңіс
    if (game.spawn_queue.empty() && game.enemies.empty()) {
        game.wave_index += 1;
        if (game.wave_index >= (int)WAVES.size()) {
            game.state = "win";
            play("win");
        } else {
            game.state = "shop";
            game.menu = "shop";
            play("wave_clear");
        }
    }
}
