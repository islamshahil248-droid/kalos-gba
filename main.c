// ============================================================
//  My GBA game - Step 2: a trainer walking on a grassy map
//  No libraries needed. Works with devkitARM + gba.specs.
// ============================================================

// ---------- GBA hardware addresses ----------
// Think of these as "mailboxes": writing to them talks to the GBA.
#define REG_DISPCNT  (*(volatile unsigned short*)0x04000000) // screen mode
#define REG_VCOUNT   (*(volatile unsigned short*)0x04000006) // which screen line is drawing
#define REG_KEYINPUT (*(volatile unsigned short*)0x04000130) // buttons
#define VRAM ((volatile unsigned short*)0x06000000)          // screen memory

// Make a color from red, green, blue. Each goes from 0 (none) to 31 (max).
#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))

// Button bits (a 1 means "pressed" after we flip the bits in the loop)
#define KEY_RIGHT 0x0010
#define KEY_LEFT  0x0020
#define KEY_UP    0x0040
#define KEY_DOWN  0x0080

// ---------- Sizes ----------
#define SCREEN_W 240
#define SCREEN_H 160
#define TILE     16      // each map square is 16x16 pixels
#define MAP_W    15      // 15 tiles * 16 = 240 pixels wide (whole screen)
#define MAP_H    10      // 10 tiles * 16 = 160 pixels tall (whole screen)
#define SPEED    2       // how many pixels you move each frame

// Where the trainer starts: map square column 6, row 8 (a path tile)
#define START_TX 6
#define START_TY 8

// The "body box" used for bumping into things. These are offsets inside the
// 16x16 sprite: the trainer is blocked when this box touches a tree.
#define HB_LEFT   2
#define HB_RIGHT  13
#define HB_TOP    1
#define HB_BOTTOM 15

// A special value that means "see-through pixel".
// Real colors never use the top bit, so this can't clash with them.
#define CLEAR 0x8000

// Directions the trainer can face
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };

// Kinds of map tiles
enum { T_GRASS, T_PATH, T_FLOWER, T_TREE };

// ---------- The map ----------
// G = grass, P = path, F = flowers, T = tree (trees block you).
// Each row is 15 letters, and there are 10 rows. Edit it and have fun!
static const char map_rows[MAP_H][MAP_W + 1] = {
  "TTTTTTTTTTTTTTT",
  "TGGGGGGGGGGGGGT",
  "TGFGGGGGTGGGFGT",
  "TGGGGGPPPPPPPGT",
  "TGGGGGPGGGGGGGT",
  "TGGFGGPGGGGFGGT",
  "TGGGGGPGGGTGGGT",
  "TGTGGGPGGGGGGGT",
  "TGGGFGPGGGGGGGT",
  "TTTTTTPTTTTTTTT"
};

// ---------- Pixel art ----------
// Each letter is a color:
//   .  see-through      K  dark outline    B  blue cap
//   S  skin             H  brown hair      Y  yellow shirt
//   J  blue jeans       R  brown shoes
// The sprite is 16 letters wide and 16 rows tall.

static const char art_down[16][17] = {
  "................",
  "....KKKKKKKK....",
  "...KBBBBBBBBK...",
  "..KBBBBBBBBBBK..",
  "..KBBBBBBBBBBK..",
  "..KKKKKKKKKKKK..",
  "..KSSSSSSSSSSK..",
  "..KSSKSSSSKSSK..",
  "..KSSSSSSSSSSK..",
  "...KSSSSSSSSK...",
  "..KKYYYYYYYYKK..",
  ".KSKYYYYYYYYKSK.",
  ".KSKYYYYYYYYKSK.",
  ".KKKJJJJJJJJKKK.",
  "...KJJJKKJJJK...",
  "...KRRRK.KRRRK.."
};

static const char art_up[16][17] = {
  "................",
  "....KKKKKKKK....",
  "...KBBBBBBBBK...",
  "..KBBBBBBBBBBK..",
  "..KBBBBBBBBBBK..",
  "..KBBBBBBBBBBK..",
  "..KHHHHHHHHHHK..",
  "..KHHHHHHHHHHK..",
  "..KHHHHHHHHHHK..",
  "...KHHHHHHHHK...",
  "..KKYYYYYYYYKK..",
  ".KSKYYYYYYYYKSK.",
  ".KSKYYYYYYYYKSK.",
  ".KKKJJJJJJJJKKK.",
  "...KJJJKKJJJK...",
  "...KRRRK.KRRRK.."
};

// Facing left. Facing right is made by flipping this one.
static const char art_left[16][17] = {
  "................",
  "....KKKKKKK.....",
  "...KBBBBBBBBK...",
  "..KBBBBBBBBBBK..",
  ".KBBBBBBBBBBBK..",
  "KBBBKKKKKKKKK...",
  ".KKKSSSSSSHHK...",
  "...KSKSSSSHHK...",
  "...KSSSSSSHHK...",
  "....KSSSSSHK....",
  "...KKYYYYYYKK...",
  "...KSYYYYYYYK...",
  "...KSYYYYYYYK...",
  "...KKJJJJJJKK...",
  "....KJJJJJJK....",
  "...KRRRJJKRRK..."
};

// A tree (the '.' parts show the grass behind it).
//   K outline   L leaf   D dark leaf   R trunk
static const char art_tree[16][17] = {
  "................",
  "....KKKKKKKK....",
  "...KLLLLLLLLK...",
  "..KLLLDLLLLLLK..",
  "..KLLLLLLDLLLK..",
  ".KLLDLLLLLLLLLK.",
  ".KLLLLLLLLDLLLK.",
  ".KLLLLDLLLLLLLK.",
  "..KLLLLLLLLDLK..",
  "..KDLLLLLLLLLK..",
  "...KKLLLLLLKK...",
  "....KKKRRKKK....",
  ".......KRRK.....",
  ".......KRRK.....",
  "......KKRRKK....",
  "................"
};

// ---------- Memory for ready-made pictures ----------
// We build each picture once at the start, then just copy from these
// boxes while the game runs (fast!).
static unsigned short tile_gfx[4][TILE * TILE];
static unsigned short sprite_gfx[4][TILE * TILE];
static unsigned char  map_kind[MAP_H][MAP_W];   // the map as numbers

// Turn a letter from the pixel art into a color.
static unsigned short letter_color(char c) {
  switch (c) {
    case 'K': return RGB15(2, 2, 5);
    case 'B': return RGB15(5, 11, 27);
    case 'S': return RGB15(31, 24, 18);
    case 'H': return RGB15(12, 7, 3);
    case 'Y': return RGB15(31, 27, 5);
    case 'J': return RGB15(6, 9, 20);
    case 'R': return RGB15(14, 8, 4);
    case 'L': return RGB15(6, 20, 6);
    case 'D': return RGB15(3, 14, 5);
    default:  return CLEAR;   // '.' and anything else = see-through
  }
}

// ---------- Building the pictures ----------
// Turn the map letters into tile numbers.
static void build_map(void) {
  int tx, ty;
  for (ty = 0; ty < MAP_H; ty++) {
    for (tx = 0; tx < MAP_W; tx++) {
      switch (map_rows[ty][tx]) {
        case 'P': map_kind[ty][tx] = T_PATH;   break;
        case 'F': map_kind[ty][tx] = T_FLOWER; break;
        case 'T': map_kind[ty][tx] = T_TREE;   break;
        default:  map_kind[ty][tx] = T_GRASS;  break;
      }
    }
  }
}

static void build_tiles(void) {
  unsigned short grass      = RGB15(9, 22, 8);
  unsigned short grass_dark = RGB15(6, 17, 6);
  unsigned short dirt       = RGB15(26, 22, 14);
  unsigned short dirt_dark  = RGB15(21, 17, 10);
  int x, y;

  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      // A little scrambled pattern so it doesn't look flat
      int scatter = (x * 37 + y * 91 + x * y * 7) & 15;

      // grass: mostly green with a few darker dots
      tile_gfx[T_GRASS][i] = (scatter == 0) ? grass_dark : grass;

      // path: tan with a few darker pebbles
      tile_gfx[T_PATH][i] = (scatter == 3 || scatter == 9) ? dirt_dark : dirt;

      // flowers: grass with three little flowers
      tile_gfx[T_FLOWER][i] = tile_gfx[T_GRASS][i];
    }
  }

  // Flowers: pink, white and yellow dots
  tile_gfx[T_FLOWER][4 * TILE + 4]   = RGB15(31, 12, 20);
  tile_gfx[T_FLOWER][5 * TILE + 4]   = RGB15(31, 12, 20);
  tile_gfx[T_FLOWER][10 * TILE + 11] = RGB15(31, 31, 31);
  tile_gfx[T_FLOWER][11 * TILE + 11] = RGB15(31, 31, 31);
  tile_gfx[T_FLOWER][11 * TILE + 5]  = RGB15(31, 28, 6);
  tile_gfx[T_FLOWER][12 * TILE + 5]  = RGB15(31, 28, 6);

  // Tree: draw the art on top of grass
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      unsigned short c = letter_color(art_tree[y][x]);
      tile_gfx[T_TREE][y * TILE + x] =
          (c == CLEAR) ? tile_gfx[T_GRASS][y * TILE + x] : c;
    }
  }
}

static void build_sprites(void) {
  int x, y;
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      sprite_gfx[DIR_DOWN][i]  = letter_color(art_down[y][x]);
      sprite_gfx[DIR_UP][i]    = letter_color(art_up[y][x]);
      sprite_gfx[DIR_LEFT][i]  = letter_color(art_left[y][x]);
      // Facing right = the left picture read backwards
      sprite_gfx[DIR_RIGHT][i] = letter_color(art_left[y][TILE - 1 - x]);
    }
  }
}

// ---------- Drawing ----------
// Paint a rectangle of the screen: left/top are included, right/bottom are not.
// For EVERY pixel we first work out the final color (map, with the trainer
// on top if the trainer is there) and then write it ONCE.
// Nothing is ever "erased" first, so the trainer can never blink or get
// covered up by an old repaint.
static void paint_box(int left, int top, int right, int bottom,
                      int px, int py, int dir) {
  int sx, sy;

  // Never paint outside the screen
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > SCREEN_W) right = SCREEN_W;
  if (bottom > SCREEN_H) bottom = SCREEN_H;

  for (sy = top; sy < bottom; sy++) {
    volatile unsigned short* row = VRAM + sy * SCREEN_W;
    int in_rows = (sy >= py && sy < py + TILE);   // trainer covers this line?

    for (sx = left; sx < right; sx++) {
      // 1) the map color at this pixel (>> 4 divides by 16, & 15 is the rest)
      unsigned short color =
          tile_gfx[map_kind[sy >> 4][sx >> 4]][((sy & 15) << 4) | (sx & 15)];

      // 2) the trainer's color on top, unless that pixel is see-through
      if (in_rows && sx >= px && sx < px + TILE) {
        unsigned short c = sprite_gfx[dir][(sy - py) * TILE + (sx - px)];
        if (c != CLEAR) color = c;
      }

      // 3) write it once
      row[sx] = color;
    }
  }
}

// ---------- Walking rules ----------
// Is the pixel (px, py) blocked? Trees and the screen edge block you.
static int is_blocked(int px, int py) {
  if (px < 0 || py < 0 || px >= SCREEN_W || py >= SCREEN_H) return 1;
  return map_kind[py >> 4][px >> 4] == T_TREE;
}

// Can the trainer stand with its top-left corner at (px, py)?
// The body box is smaller than a tile, so checking its 4 corners is enough
// to know whether it touches a tree.
static int can_stand(int px, int py) {
  return !is_blocked(px + HB_LEFT,  py + HB_TOP) &&
         !is_blocked(px + HB_RIGHT, py + HB_TOP) &&
         !is_blocked(px + HB_LEFT,  py + HB_BOTTOM) &&
         !is_blocked(px + HB_RIGHT, py + HB_BOTTOM);
}

// Start on START_TX/START_TY. If that spot were ever blocked (for example
// after you edit the map), use the first free square we can find instead.
static void pick_start(int* out_x, int* out_y) {
  int tx, ty;
  *out_x = START_TX * TILE;
  *out_y = START_TY * TILE;
  if (can_stand(*out_x, *out_y)) return;

  for (ty = 0; ty < MAP_H; ty++) {
    for (tx = 0; tx < MAP_W; tx++) {
      if (can_stand(tx * TILE, ty * TILE)) {
        *out_x = tx * TILE;
        *out_y = ty * TILE;
        return;
      }
    }
  }
}

// Wait until the screen has finished drawing a frame, so we change the
// picture while the GBA is "between frames". This stops tearing.
static void wait_vblank(void) {
  while (REG_VCOUNT >= 160) {}   // if we're already between frames, wait it out
  while (REG_VCOUNT < 160) {}    // then wait for the next "between frames"
}

// ---------- The game ----------
int main(void) {
  int x, y;
  int dir = DIR_UP;    // which way the trainer faces
  int n;

  // Forced blank: the screen stays blank while we paint the first picture,
  // so you never see it being built.
  REG_DISPCNT = 0x0080;

  build_map();
  build_tiles();
  build_sprites();
  pick_start(&x, &y);

  // Paint the whole first picture
  paint_box(0, 0, SCREEN_W, SCREEN_H, x, y, dir);

  REG_DISPCNT = 0x0403;   // Mode 3 (simple bitmap screen) + show it

  while (1) {
    unsigned short keys;
    int old_x = x, old_y = y, old_dir = dir;
    int dx = 0, dy = 0;

    wait_vblank();

    keys = ~REG_KEYINPUT;   // flip so that 1 = pressed

    if (keys & KEY_LEFT)  { dx = -1; dir = DIR_LEFT;  }
    if (keys & KEY_RIGHT) { dx =  1; dir = DIR_RIGHT; }
    if (keys & KEY_UP)    { dy = -1; dir = DIR_UP;    }
    if (keys & KEY_DOWN)  { dy =  1; dir = DIR_DOWN;  }

    // Move one pixel at a time so we stop exactly at a wall.
    // Left/right and up/down are checked separately so you slide along walls.
    for (n = 0; n < SPEED; n++) {
      if (dx != 0 && can_stand(x + dx, y)) x += dx;
      if (dy != 0 && can_stand(x, y + dy)) y += dy;
    }

    // Only repaint if something changed. We repaint one box that covers both
    // the old and the new trainer position (at most 18x18 pixels).
    if (x != old_x || y != old_y || dir != old_dir) {
      int left   = (x < old_x) ? x : old_x;
      int top    = (y < old_y) ? y : old_y;
      int right  = ((x > old_x) ? x : old_x) + TILE;
      int bottom = ((y > old_y) ? y : old_y) + TILE;
      paint_box(left, top, right, bottom, x, y, dir);
    }
  }
}
