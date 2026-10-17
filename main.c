// Pokemon-style GBA town + enterable Pokemon Center
// Target: devkitARM / gba.specs, Mode 3, 240x160
// No external libraries required.

#define REG_DISPCNT (*(volatile unsigned short*)0x04000000)
#define REG_VCOUNT  (*(volatile unsigned short*)0x04000006)
#define REG_KEYINPUT (*(volatile unsigned short*)0x04000130)
#define VRAM ((volatile unsigned short*)0x06000000)

#define SCREEN_W 240
#define SCREEN_H 160
#define KEY_A     0x0001
#define KEY_RIGHT 0x0010
#define KEY_LEFT  0x0020
#define KEY_UP    0x0040
#define KEY_DOWN  0x0080
#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

#define RGB15(r,g,b) ((unsigned short)((r) | ((g)<<5) | ((b)<<10)))

static const unsigned short C_GRASS  = RGB15(8, 21, 8);
static const unsigned short C_GRASS2 = RGB15(10, 24, 9);
static const unsigned short C_PATH   = RGB15(23, 19, 11);
static const unsigned short C_PATH2  = RGB15(27, 23, 14);
static const unsigned short C_TREE   = RGB15(3, 13, 5);
static const unsigned short C_TREE2  = RGB15(5, 18, 7);
static const unsigned short C_TRUNK  = RGB15(17, 10, 5);
static const unsigned short C_RED    = RGB15(27, 5, 7);
static const unsigned short C_RED2   = RGB15(31, 9, 10);
static const unsigned short C_WALL   = RGB15(29, 25, 18);
static const unsigned short C_WALL2  = RGB15(23, 19, 13);
static const unsigned short C_DARK   = RGB15(4, 5, 8);
static const unsigned short C_WHITE  = RGB15(31, 31, 31);
static const unsigned short C_BLUE   = RGB15(6, 15, 29);
static const unsigned short C_SKIN   = RGB15(29, 19, 12);
static const unsigned short C_HAIR   = RGB15(5, 4, 5);
static const unsigned short C_YELLOW = RGB15(31, 25, 5);
static const unsigned short C_FLOOR  = RGB15(25, 22, 16);
static const unsigned short C_FLOOR2 = RGB15(28, 25, 19);
static const unsigned short C_GREEN  = RGB15(4, 24, 13);

static int px = 112;
static int py = 48;
static int facing = DIR_UP;
static int indoors = 0;
static unsigned short previous_keys = 0;

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) { }
    while (REG_VCOUNT < 160) { }
}

static void rect(int x, int y, int w, int h, unsigned short color) {
    int xx, yy;
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    for (yy = y; yy < y + h; yy++) {
        for (xx = x; xx < x + w; xx++) VRAM[yy * SCREEN_W + xx] = color;
    }
}

static int overlaps(int x, int y, int w, int h,
                    int rx, int ry, int rw, int rh) {
    return x < rx + rw && x + w > rx && y < ry + rh && y + h > ry;
}

static int blocked(int nx, int ny) {
    // Keep the trainer inside the town or building.
    if (nx < 8 || nx > 216 || ny < 8 || ny > 136) return 1;
    if (indoors) {
        // Interior walls, counter, and healing machine.
        if (nx < 20 || nx > 204 || ny < 20 || ny > 132) return 1;
        if (overlaps(nx + 3, ny + 3, 10, 12, 54, 54, 132, 18)) return 1;
        if (overlaps(nx + 3, ny + 3, 10, 12, 176, 35, 28, 30)) return 1;
        return 0;
    }
    // Buildings: house, Pokemon Center, and shop.
    if (overlaps(nx + 2, ny + 2, 12, 13, 16, 16, 48, 32)) return 1;
    if (overlaps(nx + 2, ny + 2, 12, 13, 96, 16, 48, 32)) return 1;
    if (overlaps(nx + 2, ny + 2, 12, 13, 176, 16, 48, 32)) return 1;
    // A few trees along the grassy edges.
    if (overlaps(nx + 2, ny + 2, 12, 13, 24, 70, 16, 16)) return 1;
    if (overlaps(nx + 2, ny + 2, 12, 13, 56, 88, 16, 16)) return 1;
    if (overlaps(nx + 2, ny + 2, 12, 13, 184, 72, 16, 16)) return 1;
    if (overlaps(nx + 2, ny + 2, 12, 13, 200, 104, 16, 16)) return 1;
    return 0;
}

static void draw_tree(int x, int y) {
    rect(x + 5, y + 11, 6, 7, C_TRUNK);
    rect(x + 1, y + 4, 14, 10, C_TREE);
    rect(x + 3, y + 1, 10, 12, C_TREE2);
    rect(x + 5, y + 3, 3, 3, C_GREEN);
}

static void draw_building(int x, int y, int center) {
    // Roof and wall.
    rect(x, y, 48, 16, C_RED);
    rect(x + 3, y + 2, 42, 4, C_RED2);
    rect(x + 5, y + 8, 38, 3, RGB15(20, 3, 5));
    rect(x, y + 16, 48, 16, C_WALL);
    rect(x + 2, y + 17, 44, 13, C_WALL2);
    // Door at the center of the building.
    rect(x + 16, y + 19, 16, 13, C_DARK);
    rect(x + 18, y + 21, 12, 11, RGB15(13, 8, 5));
    rect(x + 27, y + 26, 2, 2, C_YELLOW);
    if (center) {
        // Center's white roof badge and blue windows.
        rect(x + 20, y + 3, 8, 7, C_WHITE);
        rect(x + 22, y + 5, 4, 3, C_BLUE);
    } else {
        rect(x + 5, y + 20, 8, 7, C_BLUE);
        rect(x + 35, y + 20, 8, 7, C_BLUE);
        rect(x + 6, y + 21, 6, 1, C_WHITE);
        rect(x + 36, y + 21, 6, 1, C_WHITE);
    }
}

static void draw_town(void) {
    int x, y;
    rect(0, 0, SCREEN_W, SCREEN_H, C_GRASS);
    // Soft grass patches.
    for (y = 8; y < 152; y += 16) {
        for (x = 8; x < 232; x += 24) {
            if (((x / 8) + (y / 8)) % 3 == 0) rect(x, y, 5, 3, C_GRASS2);
        }
    }
    // Main dirt road and the path to the buildings.
    rect(0, 112, 240, 32, C_PATH);
    rect(104, 40, 32, 104, C_PATH);
    rect(32, 40, 176, 16, C_PATH);
    for (x = 8; x < 232; x += 24) rect(x, 122, 10, 2, C_PATH2);
    for (y = 48; y < 112; y += 24) rect(118, y, 3, 10, C_PATH2);
    // Flowers.
    rect(76, 72, 3, 3, C_YELLOW); rect(84, 80, 3, 3, C_RED2);
    rect(152, 88, 3, 3, C_YELLOW); rect(160, 96, 3, 3, C_WHITE);
    rect(72, 96, 3, 3, C_WHITE); rect(88, 104, 3, 3, C_RED2);
    // Boundary trees.
    for (x = 0; x < 240; x += 16) { draw_tree(x, 0); draw_tree(x, 144); }
    for (y = 16; y < 144; y += 16) { draw_tree(0, y); draw_tree(224, y); }
    draw_tree(24, 70); draw_tree(56, 88); draw_tree(184, 72); draw_tree(200, 104);
    // Three buildings.
    draw_building(16, 16, 0);
    draw_building(96, 16, 1);
    draw_building(176, 16, 0);
    // Small signs.
    rect(76, 42, 8, 9, C_TRUNK); rect(77, 43, 6, 5, C_WHITE);
    rect(156, 42, 8, 9, C_TRUNK); rect(157, 43, 6, 5, C_YELLOW);
}

static void draw_center_inside(void) {
    int x, y;
    rect(0, 0, SCREEN_W, SCREEN_H, C_FLOOR);
    // Tiled floor.
    for (y = 16; y < 144; y += 16) {
        for (x = 16; x < 224; x += 16) {
            rect(x, y, 15, 15, ((x / 16 + y / 16) % 2) ? C_FLOOR2 : C_FLOOR);
        }
    }
    // Top and side walls.
    rect(0, 0, 240, 16, C_WALL2);
    rect(0, 16, 16, 128, C_WALL2);
    rect(224, 16, 16, 128, C_WALL2);
    rect(16, 144, 208, 16, C_WALL2);
    rect(20, 19, 200, 4, C_WHITE);
    // Pokemon Center sign.
    rect(92, 3, 56, 10, C_WHITE);
    rect(115, 4, 10, 8, C_RED);
    rect(118, 6, 4, 4, C_WHITE);
    // Reception counter.
    rect(54, 54, 132, 18, C_TRUNK);
    rect(54, 54, 132, 5, RGB15(25, 14, 7));
    rect(58, 60, 124, 8, RGB15(18, 10, 5));
    // Healing machine on right side.
    rect(176, 35, 28, 30, C_WHITE);
    rect(179, 38, 22, 23, C_BLUE);
    rect(182, 41, 16, 15, C_GREEN);
    rect(185, 44, 10, 9, C_WHITE);
    rect(188, 46, 4, 5, C_RED);
    // Potted plants.
    rect(25, 42, 14, 14, C_GREEN); rect(28, 56, 8, 7, C_TRUNK);
    rect(25, 91, 14, 14, C_GREEN); rect(28, 105, 8, 7, C_TRUNK);
    // Exit mat.
    rect(96, 128, 48, 12, C_RED);
    rect(100, 130, 40, 8, C_WHITE);
}

static void draw_player(void) {
    // Small 16x16 trainer sprite. The colored front stripe changes with direction.
    int x = px, y = py;
    rect(x + 3, y + 1, 10, 4, C_HAIR);
    rect(x + 2, y + 4, 12, 6, C_SKIN);
    rect(x + 3, y + 6, 2, 2, C_DARK);
    rect(x + 11, y + 6, 2, 2, C_DARK);
    rect(x + 4, y + 10, 8, 4, C_BLUE);
    rect(x + 3, y + 13, 4, 3, C_DARK);
    rect(x + 9, y + 13, 4, 3, C_DARK);
    if (facing == DIR_UP) {
        rect(x + 3, y + 2, 10, 5, C_HAIR);
        rect(x + 4, y + 8, 8, 3, C_BLUE);
    } else if (facing == DIR_LEFT) {
        rect(x + 2, y + 5, 4, 5, C_HAIR);
        rect(x + 3, y + 7, 2, 2, C_DARK);
    } else if (facing == DIR_RIGHT) {
        rect(x + 10, y + 5, 4, 5, C_HAIR);
        rect(x + 11, y + 7, 2, 2, C_DARK);
    }
}

static void draw_scene(void) {
    if (indoors) draw_center_inside(); else draw_town();
    draw_player();
    // Tiny button prompt near the bottom of the screen.
    if (indoors) {
        rect(72, 148, 96, 8, C_DARK);
        rect(76, 150, 88, 4, C_WHITE);
    } else {
        rect(72, 148, 96, 8, C_DARK);
        rect(76, 150, 88, 4, C_WHITE);
    }
}

int main(void) {
    unsigned short keys;
    int nx, ny;
    REG_DISPCNT = 0x0403; // Mode 3 + BG2 enabled

    // Start on the path just outside the Pokemon Center door.
    px = 112; py = 48; facing = DIR_UP; indoors = 0;
    draw_scene();

    while (1) {
        wait_vblank();
        keys = (unsigned short)(~REG_KEYINPUT) & 0x03FF;

        nx = px; ny = py;
        if (keys & KEY_LEFT)  { nx -= 2; facing = DIR_LEFT; }
        else if (keys & KEY_RIGHT) { nx += 2; facing = DIR_RIGHT; }
        else if (keys & KEY_UP)    { ny -= 2; facing = DIR_UP; }
        else if (keys & KEY_DOWN)  { ny += 2; facing = DIR_DOWN; }

        if (!blocked(nx, ny)) { px = nx; py = ny; }

        // A button is checked on the press edge, so holding it won't toggle repeatedly.
        if ((keys & KEY_A) && !(previous_keys & KEY_A)) {
            if (!indoors && facing == DIR_UP && px >= 106 && px <= 118 && py <= 50 && py >= 40) {
                indoors = 1;
                px = 112; py = 112; facing = DIR_DOWN;
            } else if (indoors && facing == DIR_DOWN && px >= 104 && px <= 120 && py >= 124) {
                indoors = 0;
                px = 112; py = 48; facing = DIR_UP;
            }
        }
        previous_keys = keys;
        draw_scene();
    }
    return 0;
}
