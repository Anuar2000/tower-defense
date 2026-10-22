#include "image.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glad/gl.h>
#include "stb_image.h"


Image new_image(int width, int height, Color fill) {
    Image image;
    image.width = width;
    image.height = height;
    image.pixels.resize(width * height * 4);
    image_reset_clip(image);
    image_fill_rect(image, 0, 0, width, height, fill);
    return image;
}

Image load_image(const std::string& path) {
    Image image;
    int channels;
    unsigned char* data = stbi_load(path.c_str(), &image.width, &image.height, &channels, 4);  // 4: әрқашан RGBA
    if (data == nullptr) {
        printf("Сурет ашылмады: %s\n", path.c_str());
        return new_image(1, 1);
    }
    image.pixels.assign(data, data + image.width * image.height * 4);
    stbi_image_free(data);
    image_reset_clip(image);
    return image;
}

Color get_pixel(const Image& image, int x, int y) {
    const unsigned char* p = &image.pixels[(y * image.width + x) * 4];
    return {p[0], p[1], p[2], p[3]};
}

void set_pixel(Image& image, int x, int y, Color color) {
    // Сызу аймағынан тыс болса, ештеңе істемейміз
    if (x < image.clip_x0 || x >= image.clip_x1 || y < image.clip_y0 || y >= image.clip_y1) {
        return;
    }
    unsigned char* p = &image.pixels[(y * image.width + x) * 4];
    p[0] = color.r;
    p[1] = color.g;
    p[2] = color.b;
    p[3] = color.a;
}


void image_set_clip(Image& image, int x, int y, int w, int h) {
    image.clip_x0 = std::max(0, x);
    image.clip_y0 = std::max(0, y);
    image.clip_x1 = std::min(image.width, x + w);
    image.clip_y1 = std::min(image.height, y + h);
}

void image_reset_clip(Image& image) {
    image_set_clip(image, 0, 0, image.width, image.height);
}


void image_fill_rect(Image& image, int x, int y, int w, int h, Color color) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            set_pixel(image, px, py, color);
        }
    }
}

void image_rect_outline(Image& image, int x, int y, int w, int h, Color color) {
    image_line(image, x, y, x + w - 1, y, color);
    image_line(image, x, y + h - 1, x + w - 1, y + h - 1, color);
    image_line(image, x, y, x, y + h - 1, color);
    image_line(image, x + w - 1, y, x + w - 1, y + h - 1, color);
}

// Брезенхэм алгоритмі: сызықтың әр қадамында ең жақын пиксельді бояймыз
void image_line(Image& image, int x1, int y1, int x2, int y2, Color color) {
    int dx = std::abs(x2 - x1), dy = -std::abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        set_pixel(image, x1, y1, color);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * error;
        if (e2 >= dy) {
            error += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            error += dx;
            y1 += sy;
        }
    }
}

// Көпбұрышты жол-жолымен бояймыз: әр y үшін қабырғалармен қиылысу нүктелерінің арасы
void image_polygon(Image& image, const std::vector<glm::ivec2>& points, Color color) {
    int top = points[0].y, bottom = points[0].y;
    for (glm::ivec2 p : points) {
        top = std::min(top, p.y);
        bottom = std::max(bottom, p.y);
    }
    for (int y = top; y <= bottom; y++) {
        float scan = y + 0.5f;
        std::vector<float> crossings;
        for (int i = 0; i < (int)points.size(); i++) {
            glm::ivec2 a = points[i];
            glm::ivec2 b = points[(i + 1) % points.size()];
            if ((a.y <= scan && b.y > scan) || (b.y <= scan && a.y > scan)) {
                crossings.push_back(a.x + (scan - a.y) * (b.x - a.x) / (float)(b.y - a.y));
            }
        }
        std::sort(crossings.begin(), crossings.end());
        for (int i = 0; i + 1 < (int)crossings.size(); i += 2) {
            for (int x = (int)std::ceil(crossings[i] - 0.5f); x <= (int)std::floor(crossings[i + 1] - 0.5f); x++) {
                set_pixel(image, x, y, color);
            }
        }
    }
    // Шеттері де боялсын (pygame сияқты)
    for (int i = 0; i < (int)points.size(); i++) {
        glm::ivec2 a = points[i];
        glm::ivec2 b = points[(i + 1) % points.size()];
        image_line(image, a.x, a.y, b.x, b.y, color);
    }
}

// Мөлдір емес пиксельдің көршісі мөлдір болса (немесе сурет шеті болса) — бұл шекара
void image_outline(Image& image, Color color) {
    Image source = image;
    int dx[4] = {1, -1, 0, 0};
    int dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            if (get_pixel(source, x, y).a == 0) {
                continue;
            }
            for (int k = 0; k < 4; k++) {
                int nx = x + dx[k], ny = y + dy[k];
                if (nx < 0 || ny < 0 || nx >= image.width || ny >= image.height || get_pixel(source, nx, ny).a == 0) {
                    set_pixel(image, x, y, color);
                    break;
                }
            }
        }
    }
}


Image crop_to_content(const Image& image) {
    int left = image.width, top = image.height, right = -1, bottom = -1;
    for (int y = 0; y < image.height; y++) {
        for (int x = 0; x < image.width; x++) {
            if (get_pixel(image, x, y).a > 0) {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x);
                bottom = std::max(bottom, y);
            }
        }
    }
    Image result = new_image(right - left + 1, bottom - top + 1);
    for (int y = top; y <= bottom; y++) {
        for (int x = left; x <= right; x++) {
            set_pixel(result, x - left, y - top, get_pixel(image, x, y));
        }
    }
    return result;
}


Texture make_texture(const Image& image, int scale) {
    Texture texture;
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.pixels.data());
    // GL_NEAREST: үлкейткенде пиксель-арт бұлыңғыр болмайды
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    texture.width = image.width * scale;
    texture.height = image.height * scale;
    return texture;
}
