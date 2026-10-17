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

// ---------- Test lamps ----------
// 1 = show 7 little test lamps in the top-left corner (they help us find
// problems). Change it to 0 when everything works to hide them.
#define DEBUG_HUD 1

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
  ".....KKKKKK.....",
  "...KKKLLLLKKK...",
  "..KLLLLLLLLLLK..",
  ".KLLLLDLLLLLLLK.",
  ".KLLLDLLLLDLLLK.",
  "KLLLLLLLLLLLLLLK",
  "KLLDLLLLLLLDLLLK",
  "KLLLLDLLLLLLLLLK",
  ".KLLLLLLLLLLLLK.",
  "..KKLLLLLLLLKK..",
  "....KKKRRKKK....",
  "......KRRK......",
  "......KRRK......",
  ".....KKRRKK.....",
  "................"
};

// ---------- Memory for ready-made pictures ----------
// We build each picture once at the start, then just copy from these
// boxes while the game runs (fast!).
static unsigned short tile_gfx[4][TILE * TILE];

// Two frames for each of the four directions.
static unsigned short sprite_gfx[4][2][TILE * TILE];

static unsigned char map_kind[MAP_H][MAP_W];
static int walk_frame = 0;
static int walk_timer = 0;
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
  unsigned short grass       = RGB15(10, 24, 8);
  unsigned short grass_light = RGB15(13, 27, 10);
  unsigned short grass_dark  = RGB15(6, 18, 5);

  unsigned short dirt        = RGB15(26, 21, 12);
  unsigned short dirt_light  = RGB15(29, 24, 15);
  unsigned short dirt_dark   = RGB15(20, 15, 8);

  int x, y;

  // Grass and dirt patterns.
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      int pattern = (x * 7 + y * 11 + x * y * 3) & 15;

      tile_gfx[T_GRASS][i] = grass;

      if (pattern == 0 || pattern == 5)
        tile_gfx[T_GRASS][i] = grass_dark;

      if (pattern == 10 && (y & 1))
        tile_gfx[T_GRASS][i] = grass_light;

      tile_gfx[T_PATH][i] = dirt;

      if (pattern == 2 || pattern == 11)
        tile_gfx[T_PATH][i] = dirt_dark;

      if (pattern == 7 && (x & 1))
        tile_gfx[T_PATH][i] = dirt_light;

      tile_gfx[T_FLOWER][i] = tile_gfx[T_GRASS][i];
    }
  }

  // Pink flower.
  tile_gfx[T_FLOWER][4 * TILE + 4] = RGB15(31, 10, 22);
  tile_gfx[T_FLOWER][4 * TILE + 5] = RGB15(31, 10, 22);
  tile_gfx[T_FLOWER][5 * TILE + 4] = RGB15(31, 10, 22);
  tile_gfx[T_FLOWER][5 * TILE + 5] = RGB15(31, 27, 5);

  // White flower.
  tile_gfx[T_FLOWER][9 * TILE + 10] = RGB15(31, 31, 31);
  tile_gfx[T_FLOWER][9 * TILE + 11] = RGB15(31, 31, 31);
  tile_gfx[T_FLOWER][10 * TILE + 10] = RGB15(31, 31, 31);
  tile_gfx[T_FLOWER][10 * TILE + 11] = RGB15(31, 27, 5);

  // Yellow flower.
  tile_gfx[T_FLOWER][12 * TILE + 5] = RGB15(31, 27, 5);
  tile_gfx[T_FLOWER][12 * TILE + 6] = RGB15(31, 27, 5);
  tile_gfx[T_FLOWER][13 * TILE + 5] = RGB15(31, 27, 5);
  tile_gfx[T_FLOWER][13 * TILE + 6] = RGB15(31, 27, 5);

  // Draw the existing tree art over the grass.
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      unsigned short c = letter_color(art_tree[y][x]);

      tile_gfx[T_TREE][i] =
          (c == CLEAR) ? tile_gfx[T_GRASS][i] : c;
    }
  }
}

static void build_sprites(void) {
int x, y;
int dir;

for (y = 0; y < TILE; y++) {
for (x = 0; x < TILE; x++) {
int i = y * TILE + x;

  sprite_gfx[DIR_DOWN][0][i] =
      letter_color(art_down[y][x]);

  sprite_gfx[DIR_UP][0][i] =
      letter_color(art_up[y][x]);

  sprite_gfx[DIR_LEFT][0][i] =
      letter_color(art_left[y][x]);

  sprite_gfx[DIR_RIGHT][0][i] =
      letter_color(art_left[y][TILE - 1 - x]);
}

}

// Create the second walking frame.
for (dir = 0; dir < 4; dir++) {
for (y = 0; y < TILE; y++) {
for (x = 0; x < TILE; x++) {
int i = y * TILE + x;
int source_x = x;

    // Move the two feet apart slightly in the
    // alternate frame.
    if (y >= 14) {
      if (x < 8)
        source_x = x + 1;
      else
        source_x = x - 1;
    }

    if (source_x >= 0 && source_x < TILE) {
      sprite_gfx[dir][1][i] =
          sprite_gfx[dir][0][y * TILE + source_x];
    } else {
      sprite_gfx[dir][1][i] = CLEAR;
    }
  }
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
        unsigned short c = sprite_gfx[dir][walk_frame][(sy - py) * TILE + (sx - px)];
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

#if DEBUG_HUD
// Draw one 6x6 lamp in the top-left corner (inside the top row of trees,
// where the trainer can never walk, so nothing ever paints over it).
static void lamp(int slot, int on, unsigned short color) {
  int lx = 2 + slot * 8;
  int x, y;
  for (y = 2; y < 8; y++)
    for (x = lx; x < lx + 6; x++)
      VRAM[y * SCREEN_W + x] = on ? color : RGB15(4, 4, 4);
}

// Lamp 0 blinks      = the game loop is running
// Lamps 1-4 light up = the GBA sees LEFT / RIGHT / UP / DOWN pressed
// Lamp 5 lights up   = the trainer moved this frame
// Lamp 6 lights up   = a direction is pressed but a tree/edge blocked it
static void show_lamps(unsigned int frame, unsigned short keys,
                       int moved, int blocked) {
  lamp(0, (frame >> 5) & 1, RGB15(31, 0, 31));
  lamp(1, keys & KEY_LEFT,  RGB15(0, 31, 31));
  lamp(2, keys & KEY_RIGHT, RGB15(0, 31, 31));
  lamp(3, keys & KEY_UP,    RGB15(0, 31, 31));
  lamp(4, keys & KEY_DOWN,  RGB15(0, 31, 31));
  lamp(5, moved,            RGB15(31, 31, 31));
  lamp(6, blocked,          RGB15(31, 0, 0));
}
#endif

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
  unsigned int frame = 0;

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

#if DEBUG_HUD
  lamp(0, 1, RGB15(0, 31, 0));   // GREEN = setup finished, about to start the loop
#endif

  while (1) {
    unsigned short keys;
    int old_x = x, old_y = y, old_dir = dir;
int old_walk_frame = walk_frame;
int dx = 0, dy = 0;
    wait_vblank();
    frame++;

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
    {
int moved = (x != old_x || y != old_y);

if (moved) {
walk_timer++;

if (walk_timer >= 6) {
  walk_timer = 0;
  walk_frame = 1 - walk_frame;
}

} else {
walk_timer = 0;
walk_frame = 0;
}

if (moved || dir != old_dir || walk_frame != old_walk_frame) {
int left   = (x < old_x) ? x : old_x;
int top    = (y < old_y) ? y : old_y;
int right  = ((x > old_x) ? x : old_x) + TILE;
int bottom = ((y > old_y) ? y : old_y) + TILE;

paint_box(left, top, right, bottom, x, y, dir);

}
}

#if DEBUG_HUD
    show_lamps(frame, keys,
               (x != old_x || y != old_y),
               ((dx != 0 || dy != 0) && x == old_x && y == old_y));
#endif
  }
}
