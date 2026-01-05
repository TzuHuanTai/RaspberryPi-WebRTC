//
// Created by palich on 21.12.2025.
//

#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "yuyv_clock.h"
#include <time.h>
#include <unistd.h>

static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void yuv422_fill_rect_black(uint8_t *buf,
                                   int width, int height, int stride,
                                   int x, int y, int w, int h,
                                   yuv422_fmt_t fmt)
{
    if (!buf || width <= 0 || height <= 0 || stride <= 0) return;
    if (w <= 0 || h <= 0) return;

    int x0 = clampi(x, 0, width);
    int y0 = clampi(y, 0, height);
    int x1 = clampi(x + w, 0, width);
    int y1 = clampi(y + h, 0, height);
    if (x1 <= x0 || y1 <= y0) return;

    int ax0 = x0 & ~1;
    int ax1 = (x1 + 1) & ~1;
    if (ax1 > width) ax1 = width;

    const uint8_t Y = 0;
    const uint8_t U = 128;
    const uint8_t V = 128;

    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = buf + yy * stride;
        for (int xx = ax0; xx < ax1; xx += 2) {
            uint8_t *p = row + xx * 2; // 4 bytes per 2 pixels

            if (fmt == FMT_YUYV) {
                // [Y0 U Y1 V]
                p[0] = Y; p[1] = U; p[2] = Y; p[3] = V;
            } else { // FMT_UYVY
                // [U Y0 V Y1]
                p[0] = U; p[1] = Y; p[2] = V; p[3] = Y;
            }
        }
    }
}

// 6x8 bitmap font for digits and ':' only.
// Each glyph: 8 rows, 6 bits used (LSB on right or left — мы выбрали MSB->left для удобства).
static const uint8_t font6x8[128][8] = {
    ['0'] = {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x63, 0x3E}, // stylized 0 (6 bits used)
    ['1'] = {0x0C, 0x1C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E},
    ['2'] = {0x3E, 0x63, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x7F},
    ['3'] = {0x3E, 0x63, 0x03, 0x1E, 0x03, 0x03, 0x63, 0x3E},
    ['4'] = {0x06, 0x0E, 0x1E, 0x36, 0x66, 0x7F, 0x06, 0x06},
    ['5'] = {0x7F, 0x60, 0x7E, 0x03, 0x03, 0x03, 0x63, 0x3E},
    ['6'] = {0x1E, 0x30, 0x60, 0x7E, 0x63, 0x63, 0x63, 0x3E},
    ['7'] = {0x7F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30},
    ['8'] = {0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x63, 0x3E},
    ['9'] = {0x3E, 0x63, 0x63, 0x63, 0x3F, 0x03, 0x06, 0x3C},
    [':'] = {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

// Clamp helper
static inline uint8_t clamp_u8_int(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t) v;
}

// Write one pixel luma (Y) at (x,y) for packed YUV422.
// Optionally neutralize chroma (U/V=128) for the whole 2-pixel pair.
static inline void yuv422_putY(uint8_t *base, int stride,
                               int x, int y,
                               yuv422_fmt_t fmt,
                               uint8_t Y, int neutral_uv) {
    uint8_t *row = base + y * stride;

    int pair_x = x & ~1; // even x of the pair
    int pair_off = pair_x * 2; // 4 bytes per pair => 2 bytes per pixel in packed layout

    if (fmt == FMT_YUYV) {
        // [Y0 U Y1 V]
        int yoff = pair_off + (x & 1 ? 2 : 0);
        row[yoff] = Y;

        if (neutral_uv) {
            row[pair_off + 1] = 128; // U
            row[pair_off + 3] = 128; // V
        }
    } else {
        // UYVY: [U Y0 V Y1]
        int yoff = pair_off + (x & 1 ? 3 : 1);
        row[yoff] = Y;

        if (neutral_uv) {
            row[pair_off + 0] = 128; // U
            row[pair_off + 2] = 128; // V
        }
    }
}

// Blend text into existing image by modifying Y only:
// Y_new = (1-a)*Y_old + a*Y_text, where a in [0..255].
static inline void yuv422_blendY(uint8_t *base, int stride,
                                 int x, int y,
                                 yuv422_fmt_t fmt,
                                 uint8_t Y_text, uint8_t a,
                                 int neutral_uv) {
    uint8_t *row = base + y * stride;
    int pair_x = x & ~1;
    int pair_off = pair_x * 2;

    int yoff;
    if (fmt == FMT_YUYV) {
        yoff = pair_off + (x & 1 ? 2 : 0);
    } else {
        yoff = pair_off + (x & 1 ? 3 : 1);
    }

    uint8_t Y_old = row[yoff];
    int Yn = ((int) (255 - a) * (int) Y_old + (int) a * (int) Y_text + 127) / 255;
    row[yoff] = (uint8_t) Yn;

    if (neutral_uv && a > 0) {
        if (fmt == FMT_YUYV) {
            row[pair_off + 1] = 128;
            row[pair_off + 3] = 128;
        } else {
            row[pair_off + 0] = 128;
            row[pair_off + 2] = 128;
        }
    }
}

// Draw one glyph from font6x8 at (x,y). scale >= 1.
// colorY: desired luma of text (e.g. 235 for "white-ish" in video range).
// alpha: 0..255. 255 = overwrite, <255 = blend.
// neutral_uv: 1 => force U/V=128 in touched pairs (text looks gray/white).
static void draw_glyph6x8(uint8_t *img, int w, int h, int stride,
                                 int x0, int y0, yuv422_fmt_t fmt,
                                 char ch, int scale,
                                 uint8_t colorY, uint8_t alpha,
                                 int neutral_uv) {
    if (scale < 1) scale = 1;
    const uint8_t *g = font6x8[(unsigned char) ch];
    // font uses up to 7 bits here; we treat only 6 leftmost bits of each byte.
    // We map bits 5..0 to columns 0..5.
    for (int ry = 0; ry < 8; ry++) {
        uint8_t bits = g[ry];
        for (int rx = 0; rx < 6; rx++) {
            int on = (bits >> (5 - rx)) & 1;
            if (!on) continue;

            int px = x0 + rx * scale;
            int py = y0 + ry * scale;

            // scaled block
            for (int sy = 0; sy < scale; sy++) {
                int y = py + sy;
                if ((unsigned) y >= (unsigned) h) continue;
                for (int sx = 0; sx < scale; sx++) {
                    int x = px + sx;
                    if ((unsigned) x >= (unsigned) w) continue;
                    if (alpha == 255) {
                        yuv422_putY(img, stride, x, y, fmt, colorY, neutral_uv);
                    } else {
                        yuv422_blendY(img, stride, x, y, fmt, colorY, alpha, neutral_uv);
                    }
                }
            }
        }
    }
}

static void draw_glyph8x8(uint8_t *img, int w, int h, int stride,
                          int x0, int y0, yuv422_fmt_t fmt,
                          char ch, int scale,
                          uint8_t colorY, uint8_t alpha,
                          int neutral_uv) {
    if (scale < 1) scale = 1;
    const uint8_t *g = font6x8[(unsigned char) ch];

    for (int ry = 0; ry < 8; ry++) {
        uint8_t bits = g[ry];
        for (int rx = 0; rx < 8; rx++) {
            int on = (bits >> (7 - rx)) & 1; // ВАЖНО: 8 бит!
            if (!on) continue;

            int px = x0 + rx * scale;
            int py = y0 + ry * scale;

            for (int sy = 0; sy < scale; sy++) {
                int y = py + sy;
                if ((unsigned) y >= (unsigned) h) continue;
                for (int sx = 0; sx < scale; sx++) {
                    int x = px + sx;
                    if ((unsigned) x >= (unsigned) w) continue;
                    if (alpha == 255) yuv422_putY(img, stride, x, y, fmt, colorY, neutral_uv);
                    else yuv422_blendY(img, stride, x, y, fmt, colorY, alpha, neutral_uv);
                }
            }
        }
    }
}

static void draw_text(uint8_t *img, int w, int h, int stride,
                      int x, int y, yuv422_fmt_t fmt,
                      const char *s, int scale,
                      uint8_t colorY, uint8_t alpha,
                      int neutral_uv) {
    int cx = x;
    for (; *s; s++) {
        draw_glyph8x8(img, w, h, stride, cx, y, fmt, *s, scale, colorY, alpha, neutral_uv);
        cx += (8 * scale) + (1 * scale); // glyph width + spacing
    }
}

// Format HH:MM:SS:XXX from CLOCK_MONOTONIC or real time.
// Здесь сделаем “часы” по realtime (localtime), а миллисекунды из CLOCK_REALTIME.
static void format_clock(char out[16]) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    time_t t = ts.tv_sec;
    struct tm tmv;
    localtime_r(&t, &tmv);

    int ms = (int) (ts.tv_nsec / 1000000);
    // HH:MM:SS:XXX
    snprintf(out, 16, "%02d:%02d:%02d:%03d",
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms);
}

// Call this for each frame
void overlay_clock_yuv422(uint8_t *img, int width, int height, int bytesperline,
                          yuv422_fmt_t fmt) {
    char txt[16];
    format_clock(txt);

    // Видеодиапазон “белого” Y обычно ~235 (для limited range). Можно и 255.
    uint8_t Y_text = 235;

    // alpha=255 -> жёстко рисуем, alpha=200 -> мягче.
    uint8_t alpha = 255;

    // neutral_uv=0: меняем только Y (минимально вмешиваемся, но текст может быть "цветным" из-за фона)
    // neutral_uv=1: делаем U/V=128 в затронутых парах (текст ближе к бело-серому)
    int neutral_uv = 1;

    int scale = 2; // увеличь до 3..4 если надо
    int x = 150, y = 10;

    int pad = 10;

    int bx = x - pad;
    if (bx < 0) bx = 0;
    int by = y - pad;
    if (by < 0) by = 0;
    int bw = (11 * (8 + 1) + 8) * scale + 2 * pad;
    int bh = 8 * scale + 2 * pad;

    yuv422_fill_rect_black(img, width, height, bytesperline, bx, by, bw, bh, fmt);
    draw_text(img, width, height, bytesperline, x, y, fmt, txt, scale, Y_text, alpha, neutral_uv);
}
