#include "render.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb_truetype.h"
#include "settings.h"

// Бір ғана шейдер: түсті фигуралар да, суреттер де, мәтін де осымен салынады.
// Әр вершинаның өз түсі бар, сондықтан көп фигураны бір glDrawArrays шақыруымен салуға болады (батчинг)
const char* vertexSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec4 aColor;
layout (location = 3) in vec3 aAdd;
layout (location = 4) in float aMode;
uniform mat4 uProjection;
out vec2 vUV;
out vec4 vColor;
out vec3 vAdd;
out float vMode;
void main() {
    vUV = aUV;
    vColor = aColor;
    vAdd = aAdd;
    vMode = aMode;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

// vColor — көбейтілетін түс, vAdd — қосылатын түс,
// vMode: 0 — түсті фигура (текстура керек емес), 1 — сурет, 2 — суреттің пішінін бір түспен бояу (контур үшін)
const char* fragmentSrc = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec3 vAdd;
in float vMode;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    vec4 texel = vMode < 0.5 ? vec4(1.0) : texture(uTexture, vUV);
    vec3 rgb = vMode > 1.5 ? vColor.rgb : texel.rgb * vColor.rgb + vAdd;
    FragColor = vec4(min(rgb, vec3(1.0)), texel.a * vColor.a);
}
)";

const int FLOATS_PER_VERTEX = 12;  // x, y, u, v, r, g, b, a, add_r, add_g, add_b, mode
unsigned int shader, vao, vbo;
std::vector<float> batch;  // әлі салынбаған вершиналар
unsigned int batch_texture = 0;  // жинақтағы суреттердің текстурасы
glm::mat4 model_matrix(1.0f);
// Ойын экраны (1280x720) терезеге пропорциясын сақтап созылады: шеттерінде қара жолақтар
float view_x, view_y, view_scale;
int framebuffer_h;

const float PI = 3.14159265f;


bool rect_contains(Rect rect, float px, float py) {
    return px >= rect.x && px < rect.x + rect.w && py >= rect.y && py < rect.y + rect.h;
}

Rect inflate(Rect rect, float dw, float dh) {
    return {rect.x - dw / 2, rect.y - dh / 2, rect.w + dw, rect.h + dh};
}


unsigned int compile_shader(GLenum type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    int ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, 512, nullptr, log);
        printf("Шейдер қатесі: %s\n", log);
    }
    return id;
}


void render_init() {
    // 1. Шейдер бағдарламасы
    unsigned int vs = compile_shader(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compile_shader(GL_FRAGMENT_SHADER, fragmentSrc);
    shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 2. Экран координаталары: (0, 0) — сол жақ жоғарғы бұрыш, y төмен қарай өседі (pygame сияқты)
    glm::mat4 projection = glm::ortho(0.0f, (float)WIDTH, (float)HEIGHT, 0.0f, -1.0f, 1.0f);
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));

    // 3. Бір VAO/VBO: жинақталған вершиналарды әр жолы осыған жүктеп саламыз
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    int stride = FLOATS_PER_VERTEX * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);  // x, y
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));  // u, v
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));  // түс
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));  // қосылатын түс
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (void*)(11 * sizeof(float)));  // режим
    glEnableVertexAttribArray(4);

    // 4. Мөлдірлік
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
}


// Жинақталған вершиналарды бір шақырумен саламыз
void flush() {
    if (batch.empty()) {
        return;
    }
    glUseProgram(shader);
    glBindTexture(GL_TEXTURE_2D, batch_texture);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, batch.size() * sizeof(float), batch.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (int)(batch.size() / FLOATS_PER_VERTEX));
    batch.clear();
}


void begin_frame(int framebuffer_width, int framebuffer_height) {
    // Бүкіл терезені қара түспен тазалап, ойын аймағын ортаға қоямыз
    glViewport(0, 0, framebuffer_width, framebuffer_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    view_scale = std::fmin((float)framebuffer_width / WIDTH, (float)framebuffer_height / HEIGHT);
    view_x = (framebuffer_width - WIDTH * view_scale) / 2;
    view_y = (framebuffer_height - HEIGHT * view_scale) / 2;
    framebuffer_h = framebuffer_height;
    glViewport((int)view_x, (int)view_y, (int)(WIDTH * view_scale), (int)(HEIGHT * view_scale));
    reset_model();
}

void end_frame() {
    flush();
}


void set_model(glm::mat4 model) {
    model_matrix = model;
}

void reset_model() {
    model_matrix = glm::mat4(1.0f);
}

glm::mat4 translate(float x, float y) {
    return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.0f));
}

glm::mat4 rotate(float angle) {
    return glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f));
}

glm::mat4 scale(float factor) {
    return glm::scale(glm::mat4(1.0f), glm::vec3(factor, factor, 1.0f));
}


void set_clip(Rect rect) {
    flush();  // бұрын жиналғандар қиылмай салынсын
    // glScissor пиксельмен және төменнен санайды, сондықтан ойын координатасын аударамыз
    glEnable(GL_SCISSOR_TEST);
    int x = (int)(view_x + rect.x * view_scale);
    int y = (int)(framebuffer_h - view_y - (rect.y + rect.h) * view_scale);
    glScissor(x, y, (int)(rect.w * view_scale + 1), (int)(rect.h * view_scale + 1));
}

void reset_clip() {
    flush();
    glDisable(GL_SCISSOR_TEST);
}


// Бір вершинаны жинаққа қосамыз: орнын модель матрицасымен түрлендіреміз
void add_vertex(const float* v, Color color, Color add, float mode) {
    glm::vec4 p = model_matrix * glm::vec4(v[0], v[1], 0.0f, 1.0f);
    batch.insert(batch.end(), {p.x, p.y, v[2], v[3],
                               color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f,
                               add.r / 255.0f, add.g / 255.0f, add.b / 255.0f, mode});
}

// Вершиналар (x, y, u, v) → үшбұрыштар жинағы. Жаңа текстура келгенде ғана бұрынғыларын саламыз
// shape: GL_TRIANGLES / GL_TRIANGLE_FAN / GL_TRIANGLE_STRIP; mode: 0 — фигура, 1 — сурет, 2 — пішін
void draw_mesh(const std::vector<float>& vertices, GLenum shape, unsigned int texture, Color color, Color add, float mode) {
    if (mode > 0 && texture != batch_texture) {
        flush();
        batch_texture = texture;
    }
    int count = (int)(vertices.size() / 4);
    const float* v = vertices.data();
    if (shape == GL_TRIANGLES) {
        for (int i = 0; i < count; i++) {
            add_vertex(v + i * 4, color, add, mode);
        }
    } else if (shape == GL_TRIANGLE_FAN) {
        for (int i = 1; i + 1 < count; i++) {  // бірінші нүкте әр үшбұрышқа кіреді
            add_vertex(v, color, add, mode);
            add_vertex(v + i * 4, color, add, mode);
            add_vertex(v + (i + 1) * 4, color, add, mode);
        }
    } else {
        for (int i = 0; i + 2 < count; i++) {  // қатар тұрған әр үш нүкте — үшбұрыш
            add_vertex(v + i * 4, color, add, mode);
            add_vertex(v + (i + 1) * 4, color, add, mode);
            add_vertex(v + (i + 2) * 4, color, add, mode);
        }
    }
}

// Нүктелерді желпуіш (TRIANGLE_FAN) етіп бояу: томпақ пішіндер үшін
void fill_points(const std::vector<glm::vec2>& points, Color color) {
    std::vector<float> vertices;
    for (glm::vec2 p : points) {
        vertices.insert(vertices.end(), {p.x, p.y, 0.0f, 0.0f});
    }
    draw_mesh(vertices, GL_TRIANGLE_FAN, 0, color, {0, 0, 0}, 0);
}

// Сыртқы және ішкі контур арасындағы жолақ (TRIANGLE_STRIP): жиек салу үшін
void fill_ring(const std::vector<glm::vec2>& outer, const std::vector<glm::vec2>& inner, Color color) {
    std::vector<float> vertices;
    for (int i = 0; i <= (int)outer.size(); i++) {
        glm::vec2 a = outer[i % outer.size()];
        glm::vec2 b = inner[i % inner.size()];
        vertices.insert(vertices.end(), {a.x, a.y, 0.0f, 0.0f, b.x, b.y, 0.0f, 0.0f});
    }
    draw_mesh(vertices, GL_TRIANGLE_STRIP, 0, color, {0, 0, 0}, 0);
}

// Дөңгелек бұрышты тіктөртбұрыштың шеттері: әр бұрышта ширек шеңбер
std::vector<glm::vec2> rounded_points(Rect r, float radius) {
    radius = std::fmax(0.0f, std::fmin(radius, std::fmin(r.w, r.h) / 2));
    float cx[4] = {r.x + radius, r.x + r.w - radius, r.x + r.w - radius, r.x + radius};
    float cy[4] = {r.y + radius, r.y + radius, r.y + r.h - radius, r.y + r.h - radius};
    std::vector<glm::vec2> points;
    int steps = 6;
    for (int corner = 0; corner < 4; corner++) {
        float start = PI + corner * PI / 2;  // сол жақ жоғарғы бұрыштан сағат тілімен
        for (int s = 0; s <= steps; s++) {
            float a = start + PI / 2 * s / steps;
            points.push_back({cx[corner] + std::cos(a) * radius, cy[corner] + std::sin(a) * radius});
        }
    }
    return points;
}

std::vector<glm::vec2> ellipse_points(float cx, float cy, float rx, float ry) {
    std::vector<glm::vec2> points;
    int steps = 48;
    for (int s = 0; s < steps; s++) {
        float a = 2 * PI * s / steps;
        points.push_back({cx + std::cos(a) * rx, cy + std::sin(a) * ry});
    }
    return points;
}


void draw_rect(Rect rect, Color color, float width, float radius) {
    if (width == 0) {
        fill_points(rounded_points(rect, radius), color);
        return;
    }
    Rect inner = {rect.x + width, rect.y + width, std::fmax(0.0f, rect.w - 2 * width), std::fmax(0.0f, rect.h - 2 * width)};
    fill_ring(rounded_points(rect, radius), rounded_points(inner, radius - width), color);
}

void draw_ellipse(Rect rect, Color color, float width) {
    float cx = rect.x + rect.w / 2;
    float cy = rect.y + rect.h / 2;
    if (width == 0) {
        fill_points(ellipse_points(cx, cy, rect.w / 2, rect.h / 2), color);
        return;
    }
    fill_ring(ellipse_points(cx, cy, rect.w / 2, rect.h / 2),
              ellipse_points(cx, cy, std::fmax(0.0f, rect.w / 2 - width), std::fmax(0.0f, rect.h / 2 - width)), color);
}

void draw_circle(float x, float y, float radius, Color color, float width) {
    draw_ellipse({x - radius, y - radius, radius * 2, radius * 2}, color, width);
}

void draw_polygon(const std::vector<glm::vec2>& points, Color color, float width) {
    if (width == 0) {
        fill_points(points, color);
        return;
    }
    for (int i = 0; i < (int)points.size(); i++) {
        glm::vec2 a = points[i];
        glm::vec2 b = points[(i + 1) % points.size()];
        draw_line(a.x, a.y, b.x, b.y, color, width);
    }
}

// Қалың сызық — ұзын жіңішке төртбұрыш
void draw_line(float x1, float y1, float x2, float y2, Color color, float width) {
    glm::vec2 direction = glm::vec2(x2 - x1, y2 - y1);
    float length = glm::length(direction);
    if (length == 0) {
        return;
    }
    glm::vec2 normal = glm::vec2(-direction.y, direction.x) / length * (width / 2);
    fill_points({{x1 + normal.x, y1 + normal.y}, {x2 + normal.x, y2 + normal.y},
                 {x2 - normal.x, y2 - normal.y}, {x1 - normal.x, y1 - normal.y}}, color);
}


// Текстураның бір бөлігін (u0..u1, v0..v1) экрандағы тіктөртбұрышқа саламыз
void textured_quad(unsigned int texture, Rect target, float u0, float v0, float u1, float v1,
                   Color tint, Color add, bool solid) {
    float x0 = target.x, y0 = target.y, x1 = target.x + target.w, y1 = target.y + target.h;
    std::vector<float> vertices = {
        x0, y0, u0, v0,  x1, y0, u1, v0,  x1, y1, u1, v1,
        x0, y0, u0, v0,  x1, y1, u1, v1,  x0, y1, u0, v1,
    };
    draw_mesh(vertices, GL_TRIANGLES, texture, tint, add, solid ? 2 : 1);
}

void draw_texture(const Texture& texture, float x, float y, float w, float h, Color tint, Color add, bool flip_x) {
    float u0 = flip_x ? 1.0f : 0.0f;
    textured_quad(texture.id, {x, y, w, h}, u0, 0.0f, 1.0f - u0, 1.0f, tint, add, false);
}

void draw_texture_part(const Texture& texture, Rect source, Rect target) {
    textured_quad(texture.id, target, source.x / texture.width, source.y / texture.height,
                  (source.x + source.w) / texture.width, (source.y + source.h) / texture.height,
                  {255, 255, 255}, {0, 0, 0}, false);
}

void draw_silhouette(const Texture& texture, float x, float y, float w, float h, Color color, bool flip_x) {
    float u0 = flip_x ? 1.0f : 0.0f;
    textured_quad(texture.id, {x, y, w, h}, u0, 0.0f, 1.0f - u0, 1.0f, color, {0, 0, 0}, true);
}


// --- Мәтін: stb_truetype таңбаларды бір атласқа салады, әр таңба — атластан қиылған төртбұрыш ---
const int ATLAS_SIZE = 1024;

Font load_font(const std::string& path, int size) {
    std::ifstream file(path, std::ios::binary);
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 1. Керек таңбалар: латын, кириллица және бірнеше белгі (« » — →)
    std::vector<int> codes;
    for (int c = 32; c < 127; c++) codes.push_back(c);
    for (int c = 0x400; c < 0x460; c++) codes.push_back(c);
    for (int c : {0xAB, 0xBB, 0x2014, 0x2192}) codes.push_back(c);

    // 2. Таңбаларды атласқа орналастыру
    std::vector<unsigned char> alpha(ATLAS_SIZE * ATLAS_SIZE);
    std::vector<stbtt_packedchar> packed(codes.size());
    stbtt_pack_context context;
    stbtt_PackBegin(&context, alpha.data(), ATLAS_SIZE, ATLAS_SIZE, 0, 1, nullptr);
    stbtt_pack_range range = {};
    range.font_size = STBTT_POINT_SIZE((float)size);  // size: em өлшемі пиксельмен (pygame SysFont сияқты)
    range.array_of_unicode_codepoints = codes.data();
    range.num_chars = (int)codes.size();
    range.chardata_for_range = packed.data();
    stbtt_PackFontRanges(&context, data.data(), 0, &range, 1);
    stbtt_PackEnd(&context);

    Font font;
    for (int i = 0; i < (int)codes.size(); i++) {
        stbtt_packedchar p = packed[i];
        font.glyphs[codes[i]] = {(float)p.x0, (float)p.y0, (float)p.x1, (float)p.y1,
                                 p.xoff, p.yoff, p.xoff2, p.yoff2, p.xadvance};
    }

    // 3. Жол биіктігі
    stbtt_fontinfo info;
    stbtt_InitFont(&info, data.data(), 0);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    float factor = stbtt_ScaleForMappingEmToPixels(&info, (float)size);
    font.ascent = ascent * factor;
    font.height = (ascent - descent) * factor;

    // 4. Атласты текстура етеміз: ақ түс, мөлдірлігі — таңбаның пішіні
    std::vector<unsigned char> rgba(ATLAS_SIZE * ATLAS_SIZE * 4);
    for (int i = 0; i < ATLAS_SIZE * ATLAS_SIZE; i++) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha[i];
    }
    glGenTextures(1, &font.atlas.id);
    glBindTexture(GL_TEXTURE_2D, font.atlas.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    font.atlas.width = ATLAS_SIZE;
    font.atlas.height = ATLAS_SIZE;
    return font;
}

// UTF-8 жолын unicode кодтарына бөлеміз (орыс әрпі 2 байт, «→» 3 байт)
std::vector<int> decode_utf8(const std::string& text) {
    std::vector<int> codes;
    for (int i = 0; i < (int)text.size();) {
        unsigned char c = text[i];
        if (c < 0x80) {
            codes.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            codes.push_back(((c & 0x1F) << 6) | (text[i + 1] & 0x3F));
            i += 2;
        } else {
            codes.push_back(((c & 0x0F) << 12) | ((text[i + 1] & 0x3F) << 6) | (text[i + 2] & 0x3F));
            i += 3;
        }
    }
    return codes;
}

float text_width(const Font& font, const std::string& text) {
    float width = 0;
    for (int code : decode_utf8(text)) {
        if (font.glyphs.count(code)) {
            width += font.glyphs.at(code).advance;
        }
    }
    return width;
}

void draw_text(const Font& font, const std::string& text, float x, float y, Color color, float anchor_x, float anchor_y) {
    // Пиксельге дәл тұрса, жазу анық көрінеді
    float pen = std::round(x - text_width(font, text) * anchor_x);
    float baseline = std::round(y - font.height * anchor_y + font.ascent);
    std::vector<float> vertices;
    for (int code : decode_utf8(text)) {
        if (!font.glyphs.count(code)) {
            continue;
        }
        Glyph g = font.glyphs.at(code);
        float x0 = pen + g.xoff, y0 = baseline + g.yoff, x1 = pen + g.xoff2, y1 = baseline + g.yoff2;
        float u0 = g.x0 / ATLAS_SIZE, v0 = g.y0 / ATLAS_SIZE, u1 = g.x1 / ATLAS_SIZE, v1 = g.y1 / ATLAS_SIZE;
        vertices.insert(vertices.end(), {x0, y0, u0, v0, x1, y0, u1, v0, x1, y1, u1, v1,
                                         x0, y0, u0, v0, x1, y1, u1, v1, x0, y1, u0, v1});
        pen += g.advance;
    }
    if (!vertices.empty()) {
        draw_mesh(vertices, GL_TRIANGLES, font.atlas.id, color, {0, 0, 0}, 1);
    }
}
