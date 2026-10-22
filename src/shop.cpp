#include "shop.h"
#include <algorithm>
#include <cmath>
#include <format>
#include "settings.h"
#include "sounds.h"
#include "tower.h"
#include "waves.h"

const Rect SHOP_BUTTON = {WIDTH - 370, 12, 150, 40};  // жоғарыдағы «Магазин» батырмасы
const Rect SKIP_BUTTON = {WIDTH - 316, HEIGHT - 72, 300, 56};  // оң жақ төменгі «Следующая волна»
const Rect PANEL = {WIDTH / 2 - 400, 80, 800, 520};
const Rect CLOSE_BUTTON = {WIDTH / 2 - 100, 535, 200, 50};


// Қойынды батырмалары қатарынан, саны қанша болса да ортаға тураланады
Rect tab_rect(int i) {
    float width = 170;
    float total = SHOP_TABS.size() * width + (SHOP_TABS.size() - 1) * 16;
    return {WIDTH / 2 - (int)total / 2 + i * (width + 16), 160, width, 46};
}


const Upgrade& find_upgrade(const std::string& key) {
    for (const Upgrade& upgrade : UPGRADES) {
        if (upgrade.key == key) {
            return upgrade;
        }
    }
    return UPGRADES[0];
}


// Ашық қойындыдағы жақсартулар
std::vector<std::string> tab_upgrades(const Game& game) {
    std::vector<std::string> keys;
    for (const Upgrade& upgrade : UPGRADES) {
        if (upgrade.group == game.shop_tab) {
            keys.push_back(upgrade.key);
        }
    }
    return keys;
}


int upgrade_cost(const Game& game, const std::string& key) {
    const Upgrade& data = find_upgrade(key);
    double cost = data.base_cost * std::pow(data.cost_growth, game.levels.at(key));
    return (int)(std::nearbyint(cost / 5) * 5);  // әдемі сан болсын: 5-ке дөңгелектейміз
}


bool is_max_level(const Game& game, const std::string& key) {
    return game.levels.at(key) >= find_upgrade(key).max_level;
}


// Әр жақсартудың «Купить» батырмасы (i — қойынды ішіндегі реті)
Rect buy_rect(int i) {
    return {PANEL.x + PANEL.w - 240, 235.0f + i * 72, 200, 50};
}


// Көрсеткішті оқуға ыңғайлы түрге келтіреміз
std::string format_value(const std::string& key, double value) {
    if (key == "fire_rate") {
        return std::format("{:.1f}/с", value);
    }
    if (key == "volley_cooldown") {
        return std::format("{} с", (int)value);
    }
    if (key == "cannons" || key == "volley_bullets") {
        return std::format("{} шт", (int)value);
    }
    if (find_upgrade(key).group == "skill") {
        return value > 0 ? "есть" : "нет";
    }
    return std::to_string((int)value);
}


void buy_upgrade(Game& game, const std::string& key) {
    int cost = upgrade_cost(game, key);
    if (is_max_level(game, key) || game.coins < cost) {
        play("error");
        return;
    }
    double step = find_upgrade(key).step;
    int old_tier = cannon_tier(game);
    int old_tower_tier = tower_tier(game);
    double old_cannons = game.stats["cannons"];
    game.coins -= cost;
    game.levels[key] += 1;
    game.stats[key] = std::round((game.stats[key] + step) * 100) / 100;  // 2 таңбаға дейін дөңгелектейміз
    // HP жақсартылса, қосылған HP бірден толады
    if (key == "max_hp") {
        game.tower_hp += step;
    }
    // Жаңа зеңбірек: мұнара келесі деңгейге өседі, жаңа балконға зеңбірек қойылады
    if (key == "cannons") {
        game.modules.push_back({"cannon", 0.0f, 0.0f, 0.0f});
    }
    // Зеңбірек жаңа модельге өтсе — жарқырайды (дүкен жабылған соң көрінеді)
    if (cannon_tier(game) != old_tier) {
        game.cannon_glow = GLOW_TIME;
    }
    // Мұнара жаңа түрге өтсе (HP безендіруі немесе биіктігі) — ол да жарқырайды
    if (tower_tier(game) != old_tower_tier || game.stats["cannons"] != old_cannons) {
        game.tower_glow = GLOW_TIME;
    }
    play("buy");
}


// Дүкен жабық кезде (тек толқындар арасында) «Магазин»/«Следующая волна» батырмалары; басылса true
bool click_hud(Game& game, float x, float y) {
    if (game.state != "shop") {
        return false;
    }
    if (rect_contains(SHOP_BUTTON, x, y)) {
        game.menu = "shop";
        play("menu_open");
        return true;
    }
    if (rect_contains(SKIP_BUTTON, x, y)) {
        start_wave(game);
        return true;
    }
    return false;
}


// Дүкен ашық кездегі тышқан оқиғалары
void handle_shop_event(Game& game, const Event& event) {
    if (event.type != "mousedown" || event.button != 1) {
        return;
    }
    for (int i = 0; i < (int)SHOP_TABS.size(); i++) {
        if (rect_contains(tab_rect(i), event.x, event.y)) {
            game.shop_tab = SHOP_TABS[i].first;
            play("click");
            return;
        }
    }
    std::vector<std::string> keys = tab_upgrades(game);
    for (int i = 0; i < (int)keys.size(); i++) {
        if (rect_contains(buy_rect(i), event.x, event.y)) {
            buy_upgrade(game, keys[i]);
            return;
        }
    }
    if (rect_contains(CLOSE_BUTTON, event.x, event.y)) {
        game.menu = "";
        play("menu_close");
    } else if (rect_contains(SKIP_BUTTON, event.x, event.y)) {
        start_wave(game);
    }
}


// Тышқан тұрғанда «тық» дыбысы шығатын батырмалар
std::vector<std::pair<std::string, Rect>> shop_rects(const Game& game) {
    std::vector<std::pair<std::string, Rect>> rects;
    for (int i = 0; i < (int)SHOP_TABS.size(); i++) {
        rects.push_back({SHOP_TABS[i].first, tab_rect(i)});
    }
    std::vector<std::string> keys = tab_upgrades(game);
    for (int i = 0; i < (int)keys.size(); i++) {
        rects.push_back({"buy_" + keys[i], buy_rect(i)});
    }
    rects.push_back({"close", CLOSE_BUTTON});
    rects.push_back({"skip", SKIP_BUTTON});
    return rects;
}


// Үлкен санды бос орынмен бөлеміз: 12500 → «12 500»
std::string format_coins(int coins) {
    std::string digits = std::to_string(coins);
    std::string result;
    for (int i = 0; i < (int)digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            result += " ";
        }
        result += digits[i];
    }
    return result;
}


// Тиын санағышы: тақтайша, монета суреті және сан; right — оң жақ шеті, pulse > 0 — жаңа тиын түскенде жарқырайды
void draw_coin_counter(const Font& font, const Texture& coin, int coins, float right, float y, float pulse) {
    Color color = pulse > 0 ? Color{255, 240, 150} : Color{255, 205, 60};
    std::string text = format_coins(coins);
    float width = coin.width + text_width(font, text) + 36;
    Rect panel = {right - width, y, width, 44};
    float center_y = panel.y + panel.h / 2;
    draw_rect(panel, {45, 35, 55}, 0, 10);
    draw_rect(panel, pulse <= 0 ? Color{230, 170, 40} : Color{255, 230, 120}, 3, 10);
    draw_texture(coin, panel.x + 10, center_y - coin.height / 2, coin.width, coin.height);
    float text_x = panel.x + 20 + coin.width;
    draw_text(font, text, text_x + 2, center_y + 2, {70, 40, 10}, 0, 0.5f);
    draw_text(font, text, text_x, center_y, color, 0, 0.5f);
}


void draw_shop_button(const Font& font, const std::string& text, Rect rect, Color color) {
    if (rect_contains(rect, mouse_x, mouse_y)) {
        color = {std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30)};
    }
    draw_rect(rect, color, 0, 8);
    draw_rect(rect, {20, 30, 60}, 3, 8);
    draw_text(font, text, rect.x + rect.w / 2, rect.y + rect.h / 2, {255, 255, 255}, 0.5f, 0.5f);
}


// «Купить» батырмасы: жазу, монета суреті және бағасы
void draw_buy_button(const Font& font, const Texture& coin, int cost, Rect rect, Color color) {
    draw_shop_button(font, "", rect, color);
    std::string word = "Купить";
    std::string price = format_coins(cost);
    float coin_size = 24;
    // Үшеуін қатар қойып, батырманың ортасына туралаймыз
    float width = text_width(font, word) + 10 + coin_size + 5 + text_width(font, price);
    float x = rect.x + rect.w / 2 - (int)width / 2;
    float center_y = rect.y + rect.h / 2;
    draw_text(font, word, x, center_y, {255, 255, 255}, 0, 0.5f);
    x += text_width(font, word) + 10;
    draw_texture(coin, x, center_y - coin_size / 2, coin_size, coin_size);
    x += coin_size + 5;
    draw_text(font, price, x, center_y, {255, 225, 110}, 0, 0.5f);
}


// Жоғарғы «Магазин» және төменгі «Следующая волна» батырмалары — тек толқындар арасында
void draw_hud_buttons(const Font& font, const Game& game) {
    if (game.state == "shop") {
        draw_shop_button(font, "Магазин", SHOP_BUTTON, {60, 90, 160});
        draw_shop_button(font, "Следующая волна", SKIP_BUTTON, {60, 150, 80});
    }
}


void draw_shop(const Images& images, const Font& font, const Font& big_font, const Game& game) {
    draw_rect({0, 0, WIDTH, HEIGHT}, {0, 0, 0, 140});
    draw_rect(PANEL, {30, 35, 60}, 0, 12);
    draw_rect(PANEL, {90, 110, 170}, 3, 12);

    draw_text(big_font, "Магазин", WIDTH / 2, 120, {255, 255, 255}, 0.5f, 0.5f);
    draw_coin_counter(font, images.coin, game.coins, PANEL.x + PANEL.w - 24, 98, 0);

    // 1. Қойындылар: ашығы сары
    for (int i = 0; i < (int)SHOP_TABS.size(); i++) {
        Color color = SHOP_TABS[i].first == game.shop_tab ? Color{230, 170, 40} : Color{60, 90, 160};
        draw_shop_button(font, SHOP_TABS[i].second, tab_rect(i), color);
    }

    // 2. Ашық қойындының жақсартулары: аты мен деңгейі, «қазір → кейін», сатып алу батырмасы
    std::vector<std::string> keys = tab_upgrades(game);
    for (int i = 0; i < (int)keys.size(); i++) {
        std::string key = keys[i];
        const Upgrade& data = find_upgrade(key);
        Rect button = buy_rect(i);
        float y = button.y + button.h / 2;
        int level = game.levels.at(key);
        std::string title = data.title;
        if (data.group != "skill") {
            title = std::format("{}  (ур. {}/{})", data.title, level, data.max_level);
        }
        draw_text(font, title, PANEL.x + 40, y, {255, 255, 255}, 0, 0.5f);

        double value = game.stats.at(key);
        std::string change;
        if (is_max_level(game, key)) {
            change = format_value(key, value);
            std::string done_text = data.group == "skill" ? "Куплено" : "Макс.";
            draw_shop_button(font, done_text, button, {170, 130, 40});
        } else {
            change = format_value(key, value) + " → " + format_value(key, value + data.step);
            int cost = upgrade_cost(game, key);
            Color color = game.coins >= cost ? Color{60, 90, 160} : Color{110, 110, 120};
            draw_buy_button(font, images.coin, cost, button, color);
        }
        draw_text(font, change, PANEL.x + 400, y, {180, 220, 255}, 0, 0.5f);
    }

    draw_shop_button(font, "Закрыть", CLOSE_BUTTON, {60, 90, 160});
    draw_shop_button(font, "Следующая волна", SKIP_BUTTON, {60, 150, 80});
}
