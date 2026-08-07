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
    const char *img_path = (argc > 1) ? argv[1] : "/root/xdjunx/assets/bootlogo_os.png";
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

    uint32_t fb_w = vinfo.xres;
    uint32_t fb_h = vinfo.yres;
    uint32_t bpp = vinfo.bits_per_pixel;

    if (fb_w == 0 || fb_h == 0) {
        fb_w = 1024;
        fb_h = 600;
    }

    long screensize = finfo.line_length * fb_h;
    uint8_t *fbp = (uint8_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        close(fb_fd);
        stbi_image_free(img);
        return 1;
    }

    memset(fbp, 0, screensize);

    // Aspect Fill (Cover 1024x600 canvas perfectly centered with no black bars)
    float scale_x = (float)fb_w / (float)w;
    float scale_y = (float)fb_h / (float)h;
    float scale = (scale_x > scale_y) ? scale_x : scale_y;

    int render_w = (int)(w * scale);
    int render_h = (int)(h * scale);

    int start_x = (int)(fb_w - render_w) / 2;
    int start_y = (int)(fb_h - render_h) / 2;

    for (int dy = 0; dy < (int)fb_h; dy++) {
        int src_y = (int)((dy - start_y) / scale);
        if (src_y < 0) src_y = 0;
        if (src_y >= h) src_y = h - 1;

        long dst_row_idx = dy * finfo.line_length;

        for (int dx = 0; dx < (int)fb_w; dx++) {
            int src_x = (int)((dx - start_x) / scale);
            if (src_x < 0) src_x = 0;
            if (src_x >= w) src_x = w - 1;

            int src_idx = (src_y * w + src_x) * 4;
            uint8_t r = img[src_idx];
            uint8_t g = img[src_idx + 1];
            uint8_t b = img[src_idx + 2];

            long dst_idx = dst_row_idx + dx * (bpp / 8);

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
