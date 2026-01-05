//
// Created by palich on 21.12.2025.
//

#pragma once

#include <stdint.h>
#include <linux/videodev2.h>

typedef enum {
    FMT_YUYV = V4L2_PIX_FMT_YUYV,
    FMT_UYVY = V4L2_PIX_FMT_UYVY,
} yuv422_fmt_t;

void overlay_clock_yuv422(uint8_t *img, int width, int height, int bytesperline, yuv422_fmt_t fmt);
