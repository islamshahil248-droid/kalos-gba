#define REG_DISPCNT (*(volatile unsigned short*)0x04000000)
#define REG_VCOUNT (*(volatile unsigned short*)0x04000006)
#define REG_KEYINPUT (*(volatile unsigned short*)0x04000130)
#define VRAM ((volatile unsigned short*)0x06000000)

#define RGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))

#define SCREEN_W 240
#define SCREEN_H 160

#define GRASS RGB15(10,24,8)
#define LIGHT_GRASS RGB15(15,29,10)
#define DIRT RGB15(22,16,8)
#define SKIN RGB15(31,22,15)
#define HAIR RGB15(8,5,3)
#define SHIRT RGB15(5,15,31)
#define PANTS RGB15(6,7,17)

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) {}
    while (REG_VCOUNT < 160) {}
}

static void fill_screen(unsigned short color) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        VRAM[i] = color;
}

static void draw_tile(int x, int y, int w, int h,
                      unsigned short color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (x+i >= 0 && x+i < SCREEN_W &&
                y+j >= 0 && y+j < SCREEN_H) {
                VRAM[(y+j)*SCREEN_W+x+i] = color;
            }
        }
    }
}

static void draw_map(void) {
    fill_screen(GRASS);

    /* Grass details */
    for (int y = 8; y < SCREEN_H; y += 16) {
        for (int x = 8; x < SCREEN_W; x += 24) {
            draw_tile(x, y, 2, 3, LIGHT_GRASS);
        }
    }

    /* Dirt path */
    draw_tile(0, 112, SCREEN_W, 28, DIRT);
    draw_tile(0, 110, SCREEN_W, 2, RGB15(25,19,10));
}

static void draw_trainer(int x, int y) {
    /* Hair */
    draw_tile(x+3, y, 10, 4, HAIR);

    /* Face */
    draw_tile(x+4, y+4, 8, 5, SKIN);

    /* Shirt */
    draw_tile(x+2, y+9, 12, 7, SHIRT);

    /* Arms */
    draw_tile(x, y+10, 2, 5, SKIN);
    draw_tile(x+14, y+10, 2, 5, SKIN);

    /* Pants */
    draw_tile(x+3, y+16, 4, 5, PANTS);
    draw_tile(x+9, y+16, 4, 5, PANTS);
}

int main(void) {
    int x = 112;
    int y = 72;

    REG_DISPCNT = 0x0403;

    draw_map();

    while (1) {
        wait_vblank();

        /* Redraw the background before moving the trainer */
        draw_map();

        unsigned short keys = (~REG_KEYINPUT) & 0x03FF;

        if ((keys & 0x10) && x < 224) x += 2;
        if ((keys & 0x20) && x > 0)   x -= 2;
        if ((keys & 0x40) && y > 0)   y -= 2;
        if ((keys & 0x80) && y < 138) y += 2;

        draw_trainer(x, y);
    }
}
