#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <string>
#include <vector>
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "assets.h"
#include "enemies.h"
#include "game.h"
#include "menu.h"
#include "render.h"
#include "settings.h"
#include "shop.h"
#include "skills.h"
#include "sounds.h"
#include "tower.h"
#include "turret.h"
#include "waves.h"

GLFWwindow* window;
GLFWmonitor* monitor;  // ойын ашылатын экран
std::vector<Event> events;  // соңғы кадрдан бері жиналған оқиғалар
float mouse_x = 0, mouse_y = 0;
std::mt19937 rng(std::random_device{}());


int get_ticks() {
    return (int)(glfwGetTime() * 1000);
}

int random_int(int a, int b) {
    return std::uniform_int_distribution<int>(a, b)(rng);
}

float random_float(float a, float b) {
    return std::uniform_real_distribution<float>(a, b)(rng);
}


// Терезе нүктесін ойын координатасына аударамыз (ойын терезеде ортаға созылып тұр)
void window_to_game(double x, double y, float& game_x, float& game_y) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    float factor = std::fmin((float)width / WIDTH, (float)height / HEIGHT);
    game_x = (float)((x - (width - WIDTH * factor) / 2) / factor);
    game_y = (float)((y - (height - HEIGHT * factor) / 2) / factor);
}


// GLFW оқиғаларын pygame сияқты тізімге жинаймыз, ойын циклі оларды ретімен өңдейді
void key_callback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        events.push_back({"keydown", key, 0, mouse_x, mouse_y});
    }
}

void mouse_button_callback(GLFWwindow* w, int button, int action, int mods) {
    int number = button == GLFW_MOUSE_BUTTON_LEFT ? 1 : (button == GLFW_MOUSE_BUTTON_RIGHT ? 3 : 2);  // pygame нөмірлері
    std::string type = action == GLFW_PRESS ? "mousedown" : "mouseup";
    events.push_back({type, 0, number, mouse_x, mouse_y});
}

void cursor_callback(GLFWwindow* w, double x, double y) {
    window_to_game(x, y, mouse_x, mouse_y);
    events.push_back({"motion", 0, 0, mouse_x, mouse_y});
}


void toggle_fullscreen() {
    if (options["fullscreen"]) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        int mx, my;
        glfwGetMonitorPos(monitor, &mx, &my);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, nullptr, mx + (mode->width - WIDTH) / 2, my + (mode->height - HEIGHT) / 2,
                             WIDTH, HEIGHT, 0);
    }
}


// Жаңа ойынның бастапқы күйі: барлық өзгеретін деректер осы құрылымда
Game new_game() {
    Game game;
    game.state = "pause";
    game.pause_timer = WAVE_PAUSE;
    game.wave_index = 0;
    game.wave_time = 0.0f;
    game.tower_hp = TOWER_MAX_HP;
    game.coins = 0;
    game.coin_pulse = 0.0f;
    // Дүкенде жақсартылатын көрсеткіштер
    game.stats = {
        {"damage", TURRET_DAMAGE},
        {"fire_rate", TURRET_FIRE_RATE},
        {"max_hp", TOWER_MAX_HP},
        {"volley_cooldown", VOLLEY_COOLDOWN},
        {"volley_bullets", VOLLEY_BULLETS},
        {"cannons", 1},  // зеңбірек саны = мұнара деңгейі
        // Сатып алынған навыктар (0 — жоқ, 1 — бар)
        {"freeze", 0},
        {"airstrike", 0},
        {"shield", 0},
        {"overdrive", 0},
    };
    for (const Upgrade& upgrade : UPGRADES) {
        game.levels[upgrade.key] = 0;
    }
    game.next_enemy_id = 1;
    game.priority = 0;
    game.modules = {{"cannon", 0.0f, 0.0f, 0.0f}};
    game.cannon_glow = 0.0f;
    game.tower_glow = 0.0f;
    game.commander_jump = 0.0f;
    game.launcher_recoil = 0.0f;
    for (const std::string& key : SKILL_ORDER) {
        game.skill_timers[key] = 0.0f;
    }
    game.freeze_time = 0.0f;
    game.shield_time = 0.0f;
    game.overdrive_time = 0.0f;
    game.targeting = false;
    game.menu = "";
    game.options_tab = "sound";
    game.shop_tab = "cannon";
    game.drag = "";
    game.menu_hover = "";
    return game;
}


// Тышқанмен басылған жауды басымдықты нысана етеміз
void select_priority(Game& game, float x, float y) {
    game.priority = enemy_under(game, x, y);
    if (game.priority != 0) {
        play("target");
    }
}


void update_effects(Game& game, float dt) {
    for (int i = 0; i < (int)game.effects.size();) {
        game.effects[i].time += dt;
        if (game.effects[i].time > 0.3f) {
            game.effects.erase(game.effects.begin() + i);
            continue;
        }
        i++;
    }
    // «+N» жазулары жоғары көтеріліп, 0.9 секундта жоғалады
    for (int i = 0; i < (int)game.popups.size();) {
        game.popups[i].time += dt;
        game.popups[i].y -= 45 * dt;
        if (game.popups[i].time > 0.9f) {
            game.popups.erase(game.popups.begin() + i);
            continue;
        }
        i++;
    }
    game.coin_pulse = std::fmax(0.0f, game.coin_pulse - dt);
    game.cannon_glow = std::fmax(0.0f, game.cannon_glow - dt);
    game.tower_glow = std::fmax(0.0f, game.tower_glow - dt);
    game.commander_jump = std::fmax(0.0f, game.commander_jump - dt);
}


void draw_world(const Images& images) {
    // 1. Фон: аспан мен көкжиек — жолдың артқы шетіне дейін (бір текстуралы тайлдар қатарынан салынады)
    int size = images.sky.width;
    int horizon_y = ROAD_TOP - size;
    for (int x = 0; x < WIDTH; x += size) {
        for (int y = horizon_y - size; y > -size; y -= size) {
            draw_texture(images.sky, x, y, size, size);
        }
    }
    for (int x = 0; x < WIDTH; x += size) {
        draw_texture(images.horizon[(x / size) % 4], x, horizon_y, size, size);
    }

    // 2. Жол беті (сәл жоғарыдан көрінеді) — жаулар осының үстімен үш жолақпен жүреді
    draw_texture(images.road, 0, ROAD_TOP, images.road.width, images.road.height);

    // 3. Жолдың алдыңғы шеті: шөп жиегі, астында жер жиегі (топырақ)
    for (int x = 0; x < WIDTH; x += TILE) {
        draw_texture(images.grass, x, ROAD_BOTTOM, TILE, TILE);
    }
    for (int x = 0; x < WIDTH; x += TILE) {
        for (int y = ROAD_BOTTOM + TILE; y < HEIGHT; y += TILE) {
            draw_texture(images.dirt, x, y, TILE, TILE);
        }
    }
}


void draw_effects(const std::vector<Effect>& effects) {
    for (const Effect& effect : effects) {
        float radius = (int)(10 + effect.time * effect.grow);
        draw_circle(effect.x, effect.y, radius, effect.color, 4);
    }
}


void draw_popups(const Font& font, const std::vector<Popup>& popups) {
    for (const Popup& popup : popups) {
        int alpha = (int)(255 * (1 - popup.time / 0.9f));  // біртіндеп мөлдір болады
        draw_text(font, popup.text, popup.x + 2, popup.y + 2, {70, 40, 10, alpha}, 0.5f, 0.5f);
        draw_text(font, popup.text, popup.x, popup.y, {255, 215, 70, alpha}, 0.5f, 0.5f);
    }
}


void draw_bar(float x, float y, float width, float height, double value, double max_value, Color color) {
    draw_rect({x - 2, y - 2, width + 4, height + 4}, {30, 30, 40});
    draw_rect({x, y, width, height}, {70, 70, 80});
    double fill = std::fmin(1.0, std::fmax(0.0, value) / max_value);  // жолақ рамкадан шықпасын
    draw_rect({x, y, (float)(int)(width * fill), height}, color);
}


void draw_hud(const Images& images, const Font& font, const Font& big_font, const Game& game) {
    // Мұнара HP
    draw_bar(20, 20, 260, 22, game.tower_hp, game.stats.at("max_hp"), {220, 60, 60});
    // HP саны жолақтың ортасында, оқылуы үшін көлеңкесімен
    std::string hp_text = std::format("{} / {}", (int)std::fmax(0.0, game.tower_hp), (int)game.stats.at("max_hp"));
    draw_text(font, hp_text, 151, 32, {30, 30, 40}, 0.5f, 0.5f);
    draw_text(font, hp_text, 150, 31, {255, 255, 255}, 0.5f, 0.5f);

    // Толқын мен тиындар
    int wave_number = std::min(game.wave_index + 1, (int)WAVES.size());
    draw_text(font, std::format("Волна {}/{}", wave_number, WAVES.size()), WIDTH / 2 - 50, 18, {40, 40, 60});

    // Босс тірі болса — жоғарыда оның үлкен HP жолағы
    for (const Enemy& enemy : game.enemies) {
        if (enemy.kind == "boss") {
            draw_bar(WIDTH / 2 - 250, 56, 500, 18, enemy.hp, enemy.max_hp, {200, 40, 160});
            draw_text(font, "БОСС", WIDTH / 2, 65, {255, 255, 255}, 0.5f, 0.5f);
        }
    }
    draw_coin_counter(font, images.coin, game.coins, WIDTH - 20, 10, game.coin_pulse);

    // Ортадағы хабарламалар
    std::string message = "";
    if (game.state == "pause") {
        message = std::format("Волна {} через {}", game.wave_index + 1, (int)game.pause_timer + 1);
    } else if (game.state == "shop") {
        message = std::format("Волна {} пройдена!", game.wave_index);
    } else if (game.state == "win") {
        message = std::format("Победа! Все {} волн пройдены.  R — заново", WAVES.size());
    } else if (game.state == "lose") {
        message = "Башня разрушена.  R — заново";
    }
    if (message != "") {
        draw_text(big_font, message, WIDTH / 2 + 3, HEIGHT / 2 - 120 + 3, {30, 30, 50}, 0.5f, 0.5f);
        draw_text(big_font, message, WIDTH / 2, HEIGHT / 2 - 120, {255, 255, 255}, 0.5f, 0.5f);
    }
}


void update_game(Game& game, float dt) {
    update_waves(game, dt);
    update_enemies(game, dt);
    update_turret(game, dt);
    update_bullets(game, dt);
    update_skills(game, dt);
    update_effects(game, dt);
    if (game.tower_hp <= 0 && game.state != "lose") {
        game.state = "lose";
        play("lose");
    }
}


void draw_game(const Images& images, const Font& font, const Font& big_font, Game& game) {
    draw_world(images);
    draw_tower(images, game);
    // Тышқан астындағы жау контурмен (авиасоққыға таңдап жатса — сары)
    int hovered = 0;
    if (game.menu == "" && game.state == "wave") {
        hovered = enemy_under(game, mouse_x, mouse_y);
    }
    Color hover_color = game.targeting ? Color{255, 210, 60} : Color{255, 255, 255};
    // Алыстағысы бірінші, жақыны үстінен
    std::vector<Enemy> sorted = game.enemies;
    std::stable_sort(sorted.begin(), sorted.end(), [](const Enemy& a, const Enemy& b) { return a.y < b.y; });
    for (const Enemy& enemy : sorted) {
        draw_enemy(images, enemy, enemy.id == game.priority, game.freeze_time > 0, enemy.id == hovered, hover_color);
    }
    draw_skill_world(game);
    draw_bullets(game.bullets);
    draw_effects(game.effects);
    draw_popups(font, game.popups);
    draw_hud(images, font, big_font, game);
    draw_skill_bar(font, game);
    draw_hud_buttons(font, game);
}


int main(int argc, char** argv) {
    // Ассеттер exe-нің жанында: қай папкадан іске қосылса да, exe папкасына көшеміз
    std::filesystem::path exe_folder = std::filesystem::path(argv[0]).parent_path();
    if (!exe_folder.empty()) {
        std::filesystem::current_path(exe_folder);
    }

    // 1. Терезе: OpenGL 3.3 core, 4x MSAA (фигуралардың жиегі тегіс болуы үшін)
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);  // macOS core profile тек осы белгімен ашылады
#endif
    // Екі монитор қосулы болса — ноутбук экраны (1), біреу болса — сол экран (0)
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    monitor = count >= 2 ? monitors[1] : monitors[0];
    window = glfwCreateWindow(WIDTH, HEIGHT, "Tower Defense — прототип", nullptr, nullptr);
    options["fullscreen"] = 0;
    toggle_fullscreen();  // терезені таңдалған экранның ортасына қоямыз
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(0);  // 0: vsync өшірулі, кадрлар экранның жиілігін күтпейді
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_callback);

    // 2. Шейдер, суреттер, дыбыстар, қаріптер
    render_init();
    Images images = load_images();
    load_sounds();
    start_music();
    Font font = load_font(std::string(ASSET_DIR) + "/fonts/LiberationSans-Bold.ttf", 22);
    Font big_font = load_font(std::string(ASSET_DIR) + "/fonts/LiberationSans-Bold.ttf", 48);
    Game game = new_game();

    // Тексеру үшін: ./game 14 [10000] — 14-толқын алдындағы дүкеннен бастау (тиын саны, әдепкі 50000)
    if (argc > 1) {
        game.wave_index = std::stoi(argv[1]) - 1;
        game.state = "shop";
        game.menu = "shop";
        game.coins = argc > 2 ? std::stoi(argv[2]) : 50000;
    }

    double last_time = glfwGetTime();
    std::vector<float> frame_times;  // FPS санағышы үшін соңғы 10 кадр
    bool running = true;
    while (running && !glfwWindowShouldClose(window)) {
        // 3. Кадрлар арасындағы уақыт, секунд (FPS шектелмейді)
        double now = glfwGetTime();
        float dt = (float)(now - last_time);
        last_time = now;
        frame_times.push_back(dt);
        if (frame_times.size() > 10) {
            frame_times.erase(frame_times.begin());
        }

        // 4. Оқиғалар
        events.clear();
        glfwPollEvents();
        for (const Event& event : events) {
            if (event.type == "keydown" && event.key == GLFW_KEY_ESCAPE) {
                // Авиасоққыға жау таңдап жатсақ — ESC тек соны тоқтатады
                if (game.targeting) {
                    game.targeting = false;
                } else {
                    press_escape(game);
                }
            // Мәзір ашық болса, тек оның батырмалары жұмыс істейді
            } else if (game.menu == "shop") {
                handle_shop_event(game, event);
            } else if (game.menu != "") {
                if (handle_menu_event(game, event) == "quit") {
                    running = false;
                }
            } else if (event.type == "mousedown" && event.button == 3) {
                game.targeting = false;  // ПКМ — авиасоққыны болдырмау
            } else if (event.type == "mousedown" && event.button == 1) {
                // Ретімен: навык шаршысы → авиасоққы нысанасы → «Магазин»/«Следующая волна» → басым нысана
                if (skill_bar_click(game, event.x, event.y)) {
                } else if (game.targeting) {
                    choose_airstrike_target(game, event.x, event.y);
                } else if (!click_hud(game, event.x, event.y)) {
                    select_priority(game, event.x, event.y);
                }
            } else if (event.type == "keydown") {
                // 1-5 пернелері — тақтадағы навыктар
                std::vector<std::string> owned = owned_skills(game);
                int index = event.key - GLFW_KEY_1;
                if (index >= 0 && index < 5 && index < (int)owned.size()) {
                    cast_skill(game, owned[index]);
                } else if (event.key == GLFW_KEY_R && (game.state == "win" || game.state == "lose")) {
                    game = new_game();
                }
            }
        }

        // 5. Ойын күйін жаңарту
        if (game.menu == "shop") {
            update_menu_hover(game, shop_rects(game));
        } else if (game.menu != "") {
            update_menu_hover(game, clickable_rects(game));
        } else if (game.state == "pause" || game.state == "wave" || game.state == "shop") {
            update_game(game, dt);
        }

        // 6. Сурет салу
        int framebuffer_width, framebuffer_height;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        begin_frame(framebuffer_width, framebuffer_height);
        draw_game(images, font, big_font, game);
        if (game.menu == "shop") {
            draw_shop(images, font, big_font, game);
        } else if (game.menu != "") {
            draw_menu(font, big_font, game);
        }
        // FPS санағышы оң жақ төменгі бұрышта
        if (options["show_fps"]) {
            float total = 0;
            for (float t : frame_times) {
                total += t;
            }
            int fps = (int)(frame_times.size() / total);
            draw_text(font, std::format("FPS: {}", fps), WIDTH - 12, HEIGHT - 10, {255, 255, 255}, 1, 1);
        }
        end_frame();
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
