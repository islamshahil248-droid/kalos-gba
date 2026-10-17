// GBA town game: Mode 3, 240x160, devkitARM, no libraries.
#define REG_DISPCNT  (*(volatile unsigned short*)0x04000000)
#define REG_VCOUNT   (*(volatile unsigned short*)0x04000006)
#define REG_KEYINPUT (*(volatile unsigned short*)0x04000130)
#define VRAM ((volatile unsigned short*)0x06000000)
#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#define KEY_RIGHT 0x0010
#define KEY_LEFT  0x0020
#define KEY_UP    0x0040
#define KEY_DOWN  0x0080

#define DEBUG_HUD 1   // 1 = show test lamps top-left, 0 = hide

#define SCREEN_W 240
#define SCREEN_H 160
#define TILE     16
#define MAP_W    15
#define MAP_H    10
#define SPEED    2
#define START_TX 6
#define START_TY 8
#define HB_LEFT   2   // trainer body box inside the 16x16 sprite
#define HB_RIGHT  13
#define HB_TOP    1
#define HB_BOTTOM 15
#define CLEAR   0x8000   // see-through pixel
#define OUTLINE RGB15(2, 2, 5)
#define WHITE   RGB15(31, 31, 31)

#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3

// Tile kinds. Everything from T_TREE onwards is SOLID.
enum {
  T_GRASS, T_PATH, T_FLOWER,
  T_TREE,
  T_ROOF_H1, T_ROOF_H2, T_WALL_H, T_DOOR_H,
  T_ROOF_C, T_SIGN_C, T_WALL_C, T_DOOR_C,
  T_ROOF_M, T_SIGN_M, T_WALL_M, T_DOOR_M,
  NUM_TILES
};

// G grass, P path, F flowers, T tree (solid)
// House: H/J roof, h wall, D door. Center: R roof, r sign, C wall, c door.
// Mart: U roof, u sign, M wall, m door.  Keep the top row all trees.
static const char map_rows[MAP_H][MAP_W + 1] = {
"TTTTTTTTTTTTTTT",
"THHHGGRrRGGUuUT",
"ThDhGFCcCFGMmMT",
"TGPPPPPPPPPPPGT",
"TGFGGGPGGGGFGGT",
"TGTGGGPGGJJJGTT",
"TGGGFGPGGhDhGGT",
"TGTGGGPPPPPGGTT",
"TGGGFGPGGGGGFGT",
"TTTTTTPTTTTTTTT"
};

// Sprite colors: K outline, B cap, S skin, H hair, Y shirt, J jeans, R shoes
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
// Facing left (right is this one flipped)
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
// Tree: K outline, L leaf, D dark leaf, R trunk
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
// 5x7 sign letters, row by row (X = painted)
static const char glyph_p[] = "XXXX.X...XX...XXXXX.X....X....X....";
static const char glyph_m[] = "X...XXX.XXX.X.XX...XX...XX...XX...X";

static unsigned short tile_gfx[NUM_TILES][TILE * TILE];
static unsigned short sprite_gfx[4][2][TILE * TILE];
static unsigned char map_kind[MAP_H][MAP_W];
static int walk_frame = 0;
static int walk_timer = 0;

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
    default:  return CLEAR;
  }
}

static void build_map(void) {
  int tx, ty;
  for (ty = 0; ty < MAP_H; ty++) {
    for (tx = 0; tx < MAP_W; tx++) {
      int k;
      switch (map_rows[ty][tx]) {
        case 'P': k = T_PATH;    break;
        case 'F': k = T_FLOWER;  break;
        case 'T': k = T_TREE;    break;
        case 'H': k = T_ROOF_H1; break;
        case 'J': k = T_ROOF_H2; break;
        case 'h': k = T_WALL_H;  break;
        case 'D': k = T_DOOR_H;  break;
        case 'R': k = T_ROOF_C;  break;
        case 'r': k = T_SIGN_C;  break;
        case 'C': k = T_WALL_C;  break;
        case 'c': k = T_DOOR_C;  break;
        case 'U': k = T_ROOF_M;  break;
        case 'u': k = T_SIGN_M;  break;
        case 'M': k = T_WALL_M;  break;
        case 'm': k = T_DOOR_M;  break;
        default:  k = T_GRASS;   break;
      }
      map_kind[ty][tx] = (unsigned char)k;
    }
  }
}

// Fill a rectangle inside one tile (x1/y1 not included).
static void tile_box(unsigned short* t, int x0, int y0, int x1, int y1,
                     unsigned short c) {
  int x, y;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      t[y * TILE + x] = c;
}

static void make_roof(unsigned short* t, unsigned short base,
                      unsigned short light, unsigned short dark) {
  int x, y;
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      unsigned short c = base;
      if ((y & 3) == 3)      c = dark;
      else if ((y & 3) == 0) c = light;
      if ((y & 3) != 3 && (((x + ((y >> 2) & 1) * 4) & 7) == 7)) c = dark;
      if (y == 0 || y == TILE - 1) c = OUTLINE;
      t[y * TILE + x] = c;
    }
  }
}

static void add_sign(unsigned short* t, const char* glyph,
                     unsigned short color) {
  int gx, gy;
  tile_box(t, 2, 2, 14, 14, OUTLINE);
  tile_box(t, 3, 3, 13, 13, WHITE);
  for (gy = 0; gy < 7; gy++)
    for (gx = 0; gx < 5; gx++)
      if (glyph[gy * 5 + gx] == 'X')
        t[(5 + gy) * TILE + (6 + gx)] = color;
}

static void make_wall(unsigned short* t, unsigned short wall,
                      unsigned short shade) {
  int x, y;
  for (y = 0; y < TILE; y++)
    for (x = 0; x < TILE; x++)
      t[y * TILE + x] = ((y & 3) == 3 || y >= 14) ? shade : wall;
}

static void add_window(unsigned short* t, unsigned short frame) {
  tile_box(t, 3, 2, 13, 12, frame);
  tile_box(t, 4, 3, 12, 11, RGB15(14, 24, 31));
  tile_box(t, 7, 3, 9, 11, frame);
  tile_box(t, 4, 6, 12, 8, frame);
  t[4 * TILE + 5] = WHITE;
  t[4 * TILE + 10] = WHITE;
}

static void add_door(unsigned short* t, unsigned short door,
                     unsigned short frame) {
  tile_box(t, 2, 1, 14, TILE, frame);
  tile_box(t, 3, 2, 13, TILE, door);
  tile_box(t, 10, 9, 12, 11, RGB15(31, 27, 5));
}

static void build_tiles(void) {
  unsigned short grass       = RGB15(10, 24, 8);
  unsigned short grass_light = RGB15(13, 27, 10);
  unsigned short grass_dark  = RGB15(6, 18, 5);
  unsigned short dirt        = RGB15(26, 21, 12);
  unsigned short dirt_light  = RGB15(29, 24, 15);
  unsigned short dirt_dark   = RGB15(20, 15, 8);
  unsigned short yel = RGB15(31, 27, 5);
  int x, y;

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

  // Flowers: pink, white, yellow
  tile_box(tile_gfx[T_FLOWER], 4, 4, 6, 6, RGB15(31, 10, 22));
  tile_gfx[T_FLOWER][5 * TILE + 5] = yel;
  tile_box(tile_gfx[T_FLOWER], 10, 9, 12, 11, WHITE);
  tile_gfx[T_FLOWER][10 * TILE + 11] = yel;
  tile_box(tile_gfx[T_FLOWER], 5, 12, 7, 14, yel);

  // Tree over grass
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      unsigned short c = letter_color(art_tree[y][x]);
      tile_gfx[T_TREE][i] = (c == CLEAR) ? tile_gfx[T_GRASS][i] : c;
    }
  }

  // Houses: orange and purple roofs
  make_roof(tile_gfx[T_ROOF_H1], RGB15(24, 10, 6), RGB15(28, 14, 9), RGB15(16, 6, 4));
  make_roof(tile_gfx[T_ROOF_H2], RGB15(18, 10, 22), RGB15(22, 14, 26), RGB15(12, 6, 15));
  make_wall(tile_gfx[T_WALL_H], RGB15(30, 26, 18), RGB15(25, 21, 13));
  add_window(tile_gfx[T_WALL_H], RGB15(10, 6, 3));
  make_wall(tile_gfx[T_DOOR_H], RGB15(30, 26, 18), RGB15(25, 21, 13));
  add_door(tile_gfx[T_DOOR_H], RGB15(17, 10, 4), RGB15(9, 5, 2));

  // Pokemon Center: red roof, P sign
  make_roof(tile_gfx[T_ROOF_C], RGB15(29, 5, 5), RGB15(31, 10, 9), RGB15(20, 2, 3));
  make_roof(tile_gfx[T_SIGN_C], RGB15(29, 5, 5), RGB15(31, 10, 9), RGB15(20, 2, 3));
  add_sign(tile_gfx[T_SIGN_C], glyph_p, RGB15(29, 3, 3));
  make_wall(tile_gfx[T_WALL_C], RGB15(31, 27, 26), RGB15(27, 22, 22));
  add_window(tile_gfx[T_WALL_C], RGB15(12, 12, 14));
  make_wall(tile_gfx[T_DOOR_C], RGB15(31, 27, 26), RGB15(27, 22, 22));
  add_door(tile_gfx[T_DOOR_C], RGB15(18, 26, 30), RGB15(12, 12, 14));
  tile_box(tile_gfx[T_DOOR_C], 7, 2, 9, TILE, RGB15(12, 12, 14));

  // Poke Mart: blue roof, M sign
  make_roof(tile_gfx[T_ROOF_M], RGB15(4, 10, 26), RGB15(8, 15, 30), RGB15(2, 5, 16));
  make_roof(tile_gfx[T_SIGN_M], RGB15(4, 10, 26), RGB15(8, 15, 30), RGB15(2, 5, 16));
  add_sign(tile_gfx[T_SIGN_M], glyph_m, RGB15(4, 10, 26));
  make_wall(tile_gfx[T_WALL_M], RGB15(24, 28, 31), RGB15(19, 23, 28));
  add_window(tile_gfx[T_WALL_M], RGB15(4, 6, 14));
  make_wall(tile_gfx[T_DOOR_M], RGB15(24, 28, 31), RGB15(19, 23, 28));
  add_door(tile_gfx[T_DOOR_M], RGB15(10, 18, 28), RGB15(4, 6, 14));
  tile_box(tile_gfx[T_DOOR_M], 7, 2, 9, TILE, RGB15(4, 6, 14));
}

static void build_sprites(void) {
  int x, y, dir;
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      sprite_gfx[DIR_DOWN][0][i]  = letter_color(art_down[y][x]);
      sprite_gfx[DIR_UP][0][i]    = letter_color(art_up[y][x]);
      sprite_gfx[DIR_LEFT][0][i]  = letter_color(art_left[y][x]);
      sprite_gfx[DIR_RIGHT][0][i] = letter_color(art_left[y][TILE - 1 - x]);
    }
  }
  // Second walking frame: feet move apart
  for (dir = 0; dir < 4; dir++) {
    for (y = 0; y < TILE; y++) {
      for (x = 0; x < TILE; x++) {
        int i = y * TILE + x;
        int sx = x;
        if (y >= 14) sx = (x < 8) ? x + 1 : x - 1;
        if (sx >= 0 && sx < TILE)
          sprite_gfx[dir][1][i] = sprite_gfx[dir][0][y * TILE + sx];
        else
          sprite_gfx[dir][1][i] = CLEAR;
      }
    }
  }
}

// Paint a screen rectangle (right/bottom not included). Each pixel's final
// color (map, then trainer on top) is worked out first and written ONCE,
// so the trainer is never painted over.
static void paint_box(int left, int top, int right, int bottom,
                      int px, int py, int dir) {
  int sx, sy;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > SCREEN_W) right = SCREEN_W;
  if (bottom > SCREEN_H) bottom = SCREEN_H;

  for (sy = top; sy < bottom; sy++) {
    volatile unsigned short* row = VRAM + sy * SCREEN_W;
    int in_rows = (sy >= py && sy < py + TILE);
    for (sx = left; sx < right; sx++) {
      unsigned short color =
          tile_gfx[map_kind[sy >> 4][sx >> 4]][((sy & 15) << 4) | (sx & 15)];
      if (in_rows && sx >= px && sx < px + TILE) {
        unsigned short c = sprite_gfx[dir][walk_frame][(sy - py) * TILE + (sx - px)];
        if (c != CLEAR) color = c;
      }
      row[sx] = color;
    }
  }
}

// Trees, buildings (kind >= T_TREE) and the screen edge block you.
static int is_blocked(int px, int py) {
  if (px < 0 || py < 0 || px >= SCREEN_W || py >= SCREEN_H) return 1;
  return map_kind[py >> 4][px >> 4] >= T_TREE;
}

// Body box is smaller than a tile, so its 4 corners are enough.
static int can_stand(int px, int py) {
  return !is_blocked(px + HB_LEFT,  py + HB_TOP) &&
         !is_blocked(px + HB_RIGHT, py + HB_TOP) &&
         !is_blocked(px + HB_LEFT,  py + HB_BOTTOM) &&
         !is_blocked(px + HB_RIGHT, py + HB_BOTTOM);
}

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
// Lamps live in the top tree row, where the trainer can never walk.
static void lamp(int slot, int on, unsigned short color) {
  int lx = 2 + slot * 8;
  int x, y;
  for (y = 2; y < 8; y++)
    for (x = lx; x < lx + 6; x++)
      VRAM[y * SCREEN_W + x] = on ? color : RGB15(4, 4, 4);
}

// 0 blinks = loop runs; 1-4 = LEFT/RIGHT/UP/DOWN; 5 = moved; 6 = blocked
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

static void wait_vblank(void) {
  while (REG_VCOUNT >= 160) {}
  while (REG_VCOUNT < 160) {}
}

int main(void) {
  int x, y;
  int dir = DIR_UP;
  int n;
  unsigned int frame = 0;

  REG_DISPCNT = 0x0080;   // forced blank while we build the first picture
  build_map();
  build_tiles();
  build_sprites();
  pick_start(&x, &y);
  paint_box(0, 0, SCREEN_W, SCREEN_H, x, y, dir);
  REG_DISPCNT = 0x0403;   // Mode 3 + show BG2

#if DEBUG_HUD
  lamp(0, 1, RGB15(0, 31, 0));
#endif

  while (1) {
    unsigned short keys;
    int old_x = x, old_y = y, old_dir = dir;
    int old_walk_frame = walk_frame;
    int dx = 0, dy = 0;
    int moved;
    wait_vblank();
    frame++;
    keys = ~REG_KEYINPUT;

    if (keys & KEY_LEFT)  { dx = -1; dir = DIR_LEFT;  }
    if (keys & KEY_RIGHT) { dx =  1; dir = DIR_RIGHT; }
    if (keys & KEY_UP)    { dy = -1; dir = DIR_UP;    }
    if (keys & KEY_DOWN)  { dy =  1; dir = DIR_DOWN;  }

    // One pixel at a time; X and Y separately so you slide along walls.
    for (n = 0; n < SPEED; n++) {
      if (dx != 0 && can_stand(x + dx, y)) x += dx;
      if (dy != 0 && can_stand(x, y + dy)) y += dy;
    }

    moved = (x != old_x || y != old_y);
    if (moved) {
      walk_timer++;
      if (walk_timer >= 6) { walk_timer = 0; walk_frame = 1 - walk_frame; }
    } else {
      walk_timer = 0;
      walk_frame = 0;
    }

    // Repaint one box covering the old and new trainer position.
    if (moved || dir != old_dir || walk_frame != old_walk_frame) {
      int left   = (x < old_x) ? x : old_x;
      int top    = (y < old_y) ? y : old_y;
      int right  = ((x > old_x) ? x : old_x) + TILE;
      int bottom = ((y > old_y) ? y : old_y) + TILE;
      paint_box(left, top, right, bottom, x, y, dir);
    }

#if DEBUG_HUD
    show_lamps(frame, keys, moved,
               ((dx != 0 || dy != 0) && !moved));
#endif
  }
}
// END OF FILE
