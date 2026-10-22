#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "render.h"

// Процессордағы сурет (RGBA пиксельдер): мұнара мен жолды алдымен осында пиксельмен саламыз,
// содан кейін текстура етіп GPU-ға жібереміз (Python-дағы pygame.Surface сияқты)
struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;  // әр пиксельге 4 байт: r, g, b, a
    // Сызу аймағы (set_clip): осыдан тыс пиксельдер өзгермейді
    int clip_x0 = 0, clip_y0 = 0, clip_x1 = 0, clip_y1 = 0;
};

Image new_image(int width, int height, Color fill = {0, 0, 0, 0});
Image load_image(const std::string& path);
Color get_pixel(const Image& image, int x, int y);
void set_pixel(Image& image, int x, int y, Color color);

void image_set_clip(Image& image, int x, int y, int w, int h);
void image_reset_clip(Image& image);

void image_fill_rect(Image& image, int x, int y, int w, int h, Color color);
void image_rect_outline(Image& image, int x, int y, int w, int h, Color color);  // 1 пиксельдік жиек
void image_line(Image& image, int x1, int y1, int x2, int y2, Color color);  // ұштары да боялады
void image_polygon(Image& image, const std::vector<glm::ivec2>& points, Color color);
void image_outline(Image& image, Color color);  // мөлдір емес аймақтың сыртқы шетін бояу

Image crop_to_content(const Image& image);  // мөлдір жиектерді қиып тастау

// GPU-ға жіберу: scale — экранда неше есе үлкен салынады (пиксельдер анық қалады)
Texture make_texture(const Image& image, int scale);
