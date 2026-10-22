#include "menu.h"
#include <cmath>
#include <format>
#include <map>
#include "settings.h"
#include "sounds.h"

// Пауза мәзірінің батырмалары: (мәтін, тіктөртбұрыш)
const std::vector<std::pair<std::string, Rect>> PAUSE_BUTTONS = {
    {"Продолжить", {WIDTH / 2 - 150, HEIGHT / 2 - 110, 300, 60}},
    {"Параметры", {WIDTH / 2 - 150, HEIGHT / 2 - 30, 300, 60}},
    {"Выход", {WIDTH / 2 - 150, HEIGHT / 2 + 50, 300, 60}},
};

// Параметрлер терезесі
const Rect PANEL = {WIDTH / 2 - 380, 110, 760, 500};

struct Tab {
    std::string key, text;
    Rect rect;
};

const std::vector<Tab> TABS = {
    {"sound", "Звук", {WIDTH / 2 - 190, 180, 180, 44}},
    {"screen", "Экран", {WIDTH / 2 + 10, 180, 180, 44}},
};
const Rect BACK_BUTTON = {WIDTH / 2 - 100, 530, 200, 50};
const float CONTROL_X = 560;  // сырғытпа мен ауыстырғыштардың сол жақ шеті
const float SLIDER_WIDTH = 300;

// Қойындының бір жолы: түрі (slider/toggle), options ішіндегі кілт, жазуы
struct Row {
    std::string kind, key, label;
};

const std::map<std::string, std::vector<Row>> TAB_ROWS = {
    {"sound", {
        {"slider", "master", "Общая громкость"},
        {"slider", "music", "Музыка"},
        {"slider", "sfx", "Эффекты"},
        {"toggle", "sound_on", "Звук"},
    }},
    {"screen", {
        {"toggle", "fullscreen", "Полный экран"},
        {"toggle", "show_fps", "Показывать FPS"},
    }},
};

// Жол экранда: жолдың өзі, тіктөртбұрышы, жолдың y ортасы
struct PlacedRow {
    Row row;
    Rect rect;
    float y;
};


// Қойындының жолдарын орындарымен қайтарамыз
std::vector<PlacedRow> tab_rows(const std::string& tab) {
    std::vector<PlacedRow> rows;
    const std::vector<Row>& list = TAB_ROWS.at(tab);
    for (int i = 0; i < (int)list.size(); i++) {
        float y = 290 + i * 60;
        Rect rect;
        if (list[i].kind == "slider") {
            rect = {CONTROL_X, y - 8, SLIDER_WIDTH, 16};
        } else {
            rect = {CONTROL_X, y - 20, 120, 40};
        }
        rows.push_back({list[i], rect, y});
    }
    return rows;
}


// ESC: ойын → пауза → ойын; параметрлерден → паузаға қайту; дүкенді жабу
void press_escape(Game& game) {
    if (game.menu == "") {
        game.menu = "pause";
        play("menu_open");
    } else if (game.menu == "pause" || game.menu == "shop") {
        game.menu = "";
        play("menu_close");
    } else {
        game.menu = "pause";
        game.drag = "";
        play("menu_close");
    }
}


// Сырғытпа мәнін тышқанның x орнынан есептейміз (0..1)
void set_slider(const std::string& key, float x) {
    float value = (x - CONTROL_X) / SLIDER_WIDTH;
    options[key] = std::round(std::fmin(1.0f, std::fmax(0.0f, value)) * 100) / 100;
    apply_volumes();
}


void toggle_option(const std::string& key) {
    options[key] = options[key] ? 0 : 1;
    if (key == "sound_on") {
        if (!options["sound_on"]) {
            stop_effects();
        }
        apply_volumes();
    } else if (key == "fullscreen") {
        toggle_fullscreen();
    }
}


std::string click_pause(Game& game, float x, float y) {
    for (const auto& [text, rect] : PAUSE_BUTTONS) {
        if (rect_contains(rect, x, y)) {
            play("click");
            if (text == "Продолжить") {
                game.menu = "";
            } else if (text == "Параметры") {
                game.menu = "options";
            } else if (text == "Выход") {
                return "quit";
            }
        }
    }
    return "";
}


void click_options(Game& game, float x, float y) {
    for (const Tab& tab : TABS) {
        if (rect_contains(tab.rect, x, y)) {
            game.options_tab = tab.key;
            play("click");
            return;
        }
    }
    if (rect_contains(BACK_BUTTON, x, y)) {
        game.menu = "pause";
        play("click");
        return;
    }
    for (const PlacedRow& placed : tab_rows(game.options_tab)) {
        // Сырғытпаны ұстау аймағы сызықтан сәл кеңірек
        if (placed.row.kind == "slider" && rect_contains(inflate(placed.rect, 20, 30), x, y)) {
            game.drag = placed.row.key;
            set_slider(placed.row.key, x);
            return;
        }
        if (placed.row.kind == "toggle" && rect_contains(placed.rect, x, y)) {
            toggle_option(placed.row.key);
            play("click");
            return;
        }
    }
}


// Мәзір ашық кездегі тышқан оқиғалары; "quit" қайтса — ойыннан шығамыз
std::string handle_menu_event(Game& game, const Event& event) {
    if (event.type == "mousedown" && event.button == 1) {
        if (game.menu == "pause") {
            return click_pause(game, event.x, event.y);
        }
        click_options(game, event.x, event.y);
    } else if (event.type == "motion" && game.drag != "") {
        set_slider(game.drag, event.x);
    } else if (event.type == "mouseup" && event.button == 1 && game.drag != "") {
        game.drag = "";
        play("click");  // жаңа деңгейді бірден естиміз
    }
    return "";
}


// Тышқан басуға болатын нәрселердің бәрі: (атауы, тіктөртбұрыш)
std::vector<std::pair<std::string, Rect>> clickable_rects(const Game& game) {
    if (game.menu == "pause") {
        return PAUSE_BUTTONS;
    }
    std::vector<std::pair<std::string, Rect>> rects;
    for (const Tab& tab : TABS) {
        rects.push_back({tab.key, tab.rect});
    }
    rects.push_back({"back", BACK_BUTTON});
    for (const PlacedRow& placed : tab_rows(game.options_tab)) {
        if (placed.row.kind == "toggle") {
            rects.push_back({placed.row.key, placed.rect});
        }
    }
    return rects;
}


// Тышқан жаңа батырмаға келсе — «тық» дыбысы
void update_menu_hover(Game& game, const std::vector<std::pair<std::string, Rect>>& rects) {  // rects: (атауы, тіктөртбұрыш) тізімі
    std::string hover = "";
    for (const auto& [name, rect] : rects) {
        if (rect_contains(rect, mouse_x, mouse_y)) {
            hover = name;
        }
    }
    if (hover != "" && hover != game.menu_hover) {
        play("hover");
    }
    game.menu_hover = hover;
}


void draw_button(const Font& font, const std::string& text, Rect rect, bool active = false) {
    bool hover = rect_contains(rect, mouse_x, mouse_y);
    Color color;
    if (active) {
        color = {230, 170, 40};
    } else if (hover) {
        color = {90, 140, 220};
    } else {
        color = {60, 90, 160};
    }
    draw_rect(rect, color, 0, 8);
    draw_rect(rect, {20, 30, 60}, 3, 8);
    draw_text(font, text, rect.x + rect.w / 2, rect.y + rect.h / 2, {255, 255, 255}, 0.5f, 0.5f);
}


void draw_slider(const Font& font, Rect rect, float value) {
    draw_rect(rect, {40, 45, 70}, 0, 8);
    draw_rect({rect.x, rect.y, (float)(int)(rect.w * value), rect.h}, {90, 140, 220}, 0, 8);
    float knob_x = rect.x + (int)(rect.w * value);
    float center_y = rect.y + rect.h / 2;
    draw_circle(knob_x, center_y, 13, {255, 255, 255});
    draw_circle(knob_x, center_y, 13, {20, 30, 60}, 3);
    draw_text(font, std::format("{}%", (int)std::round(value * 100)), rect.x + rect.w + 25, center_y, {255, 255, 255}, 0, 0.5f);
}


void draw_toggle(const Font& font, Rect rect, bool value) {
    Color color = value ? Color{60, 170, 90} : Color{110, 110, 120};
    draw_rect(rect, color, 0, 8);
    draw_rect(rect, {20, 30, 60}, 3, 8);
    draw_text(font, value ? "Вкл" : "Выкл", rect.x + rect.w / 2, rect.y + rect.h / 2, {255, 255, 255}, 0.5f, 0.5f);
}


void draw_options(const Font& font, const Font& big_font, const Game& game) {
    draw_rect(PANEL, {30, 35, 60}, 0, 12);
    draw_rect(PANEL, {90, 110, 170}, 3, 12);
    draw_text(big_font, "Параметры", WIDTH / 2, 145, {255, 255, 255}, 0.5f, 0.5f);

    // 1. Қойындылар: таңдалғаны сары
    for (const Tab& tab : TABS) {
        draw_button(font, tab.text, tab.rect, tab.key == game.options_tab);
    }

    // 2. Таңдалған қойындының жолдары
    for (const PlacedRow& placed : tab_rows(game.options_tab)) {
        draw_text(font, placed.row.label, PANEL.x + 50, placed.y, {255, 255, 255}, 0, 0.5f);
        if (placed.row.kind == "slider") {
            draw_slider(font, placed.rect, options[placed.row.key]);
        } else {
            draw_toggle(font, placed.rect, options[placed.row.key] > 0);
        }
    }

    draw_button(font, "Назад", BACK_BUTTON);
}


void draw_menu(const Font& font, const Font& big_font, const Game& game) {
    // Ойынды күңгірттейтін жартылай мөлдір қабат
    draw_rect({0, 0, WIDTH, HEIGHT}, {0, 0, 0, 140});

    if (game.menu == "options") {
        draw_options(font, big_font, game);
        return;
    }

    draw_text(big_font, "Пауза", WIDTH / 2, HEIGHT / 2 - 180, {255, 255, 255}, 0.5f, 0.5f);
    for (const auto& [text, rect] : PAUSE_BUTTONS) {
        draw_button(font, text, rect);
    }
}
