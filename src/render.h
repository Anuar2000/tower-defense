#pragma once
#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// Түс: 0..255 (Python-дағы (r, g, b) немесе (r, g, b, a) кортежі сияқты)
struct Color {
    int r, g, b;
    int a = 255;
};

// Тіктөртбұрыш: сол жақ жоғарғы бұрышы, ені, биіктігі
struct Rect {
    float x, y, w, h;
};

bool rect_contains(Rect rect, float px, float py);
Rect inflate(Rect rect, float dw, float dh);  // dw, dh: енін/биіктігін қаншаға үлкейту (ортасы орнында)

// GPU-дағы сурет: id — OpenGL текстурасы, width/height — экранға салынатын өлшемі
struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
};

// Қаріптің бір таңбасы: атластағы орны және сызыққа қатысты ығысуы
struct Glyph {
    float x0, y0, x1, y1;  // атластағы пиксельдер
    float xoff, yoff, xoff2, yoff2;
    float advance;  // келесі таңбаға дейінгі қашықтық
};

struct Font {
    Texture atlas;
    std::map<int, Glyph> glyphs;  // кілт: таңбаның unicode коды
    float ascent;  // жазу сызығынан жоғарғы шетке дейін
    float height;  // жолдың толық биіктігі
};

// Терезе мен шейдерді дайындау, әр кадрдың басы
void render_init();
void begin_frame(int framebuffer_width, int framebuffer_height);
void end_frame();  // жинақта қалған вершиналарды салу (кадрдың соңында)

// Модель матрицасы: келесі суреттердің бәрі осы түрлендіру арқылы салынады (жылжыту, бұру, масштаб)
void set_model(glm::mat4 model);
void reset_model();
glm::mat4 translate(float x, float y);
glm::mat4 rotate(float angle);  // angle: радиан, экранда сағат тілімен
glm::mat4 scale(float factor);

// Тек экранның осы бөлігіне салу (glScissor), модель матрицасы әсер етпейді
void set_clip(Rect rect);
void reset_clip();

// Фигуралар: width = 0 — толық бояу, әйтпесе ішке қарай қалыңдығы width жиек (pygame.draw сияқты)
void draw_rect(Rect rect, Color color, float width = 0, float radius = 0);  // radius: бұрыштардың дөңгелегі
void draw_circle(float x, float y, float radius, Color color, float width = 0);
void draw_ellipse(Rect rect, Color color, float width = 0);
void draw_polygon(const std::vector<glm::vec2>& points, Color color, float width = 0);  // томпақ көпбұрыш
void draw_line(float x1, float y1, float x2, float y2, Color color, float width = 1);

// Текстура: tint — түске көбейту, add — түске қосу (ағарту/көгерту), flip_x — көлденең айналдыру
void draw_texture(const Texture& texture, float x, float y, float w, float h,
                  Color tint = {255, 255, 255}, Color add = {0, 0, 0}, bool flip_x = false);
void draw_texture_part(const Texture& texture, Rect source, Rect target);  // source: текстура ішіндегі бөлік
void draw_silhouette(const Texture& texture, float x, float y, float w, float h, Color color, bool flip_x);

// Мәтін: (x, y) нүктесі мәтіннің қай жеріне сәйкес — anchor_x/anchor_y: 0 — сол/жоғары, 0.5 — орта, 1 — оң/төмен
Font load_font(const std::string& path, int size);
float text_width(const Font& font, const std::string& text);
void draw_text(const Font& font, const std::string& text, float x, float y, Color color,
               float anchor_x = 0, float anchor_y = 0);
