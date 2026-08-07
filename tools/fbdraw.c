#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb_image.h"

int main(int argc, char **argv) {
    const char *img_path = (argc > 1) ? argv[1] : "/root/xdjunx/assets/splash.png";
    int w, h, channels;
    unsigned char *img = stbi_load(img_path, &w, &h, &channels, 4);
    if (!img) {
        fprintf(stderr, "Failed to load image %s\n", img_path);
        return 1;
    }

    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        stbi_image_free(img);
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 || ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(fb_fd);
        stbi_image_free(img);
        return 1;
    }

    long screensize = finfo.line_length * vinfo.yres;
    uint8_t *fbp = (uint8_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        close(fb_fd);
        stbi_image_free(img);
        return 1;
    }

    uint32_t fb_w = vinfo.xres;
    uint32_t fb_h = vinfo.yres;
    uint32_t bpp = vinfo.bits_per_pixel;

    memset(fbp, 0, screensize);

    float scale_x = (float)fb_w / (float)w;
    float scale_y = (float)fb_h / (float)h;
    float scale = (scale_x < scale_y ? scale_x : scale_y);
    if (scale > 1.0f) scale = 1.0f;

    int render_w = (int)(w * scale);
    int render_h = (int)(h * scale);

    int start_x = (int)(fb_w - render_w) / 2;
    int start_y = (int)(fb_h - render_h) / 2;
    if (start_x < 0) start_x = 0;
    if (start_y < 0) start_y = 0;

    for (int dy = 0; dy < render_h && (dy + start_y) < (int)fb_h; dy++) {
        int y = (int)(dy / scale);
        if (y >= h) y = h - 1;
        for (int dx = 0; dx < render_w && (dx + start_x) < (int)fb_w; dx++) {
            int x = (int)(dx / scale);
            if (x >= w) x = w - 1;
            int src_idx = (y * w + x) * 4;
            uint8_t r = img[src_idx];
            uint8_t g = img[src_idx + 1];
            uint8_t b = img[src_idx + 2];

            long dst_idx = (dy + start_y) * finfo.line_length + (dx + start_x) * (bpp / 8);

            if (bpp == 32) {
                fbp[dst_idx + vinfo.red.offset / 8] = r;
                fbp[dst_idx + vinfo.green.offset / 8] = g;
                fbp[dst_idx + vinfo.blue.offset / 8] = b;
                if (vinfo.transp.length > 0) {
                    fbp[dst_idx + vinfo.transp.offset / 8] = 0xFF;
                }
            } else if (bpp == 16) {
                uint16_t r5 = (r >> 3) & 0x1F;
                uint16_t g6 = (g >> 2) & 0x3F;
                uint16_t b5 = (b >> 3) & 0x1F;
                uint16_t pixel = (r5 << 11) | (g6 << 5) | b5;
                *((uint16_t *)(fbp + dst_idx)) = pixel;
            }
        }
    }

    munmap(fbp, screensize);
    close(fb_fd);
    stbi_image_free(img);
    return 0;
}
