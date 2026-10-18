// GBA town game: Mode 3, 240x160, devkitARM, no libraries.
// Town + enterable Pokemon Center (A button at the door).
#define REG_DISPCNT  (*(volatile unsigned short*)0x04000000)
#define REG_VCOUNT   (*(volatile unsigned short*)0x04000006)
#define REG_KEYINPUT (*(volatile unsigned short*)0x04000130)
#define VRAM ((volatile unsigned short*)0x06000000)
#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#define KEY_A     0x0001
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
#define START_TX 6          // where you start in town (a path tile)
#define START_TY 8
#define TOWN_EXIT_TX   7    // where you appear in town after leaving the Center
#define TOWN_EXIT_TY   3    // (the path square right below its door)
#define INSIDE_START_TX 7   // where you appear inside the Center
#define INSIDE_START_TY 8   // (on the door mat)
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

// Tile kinds. Everything from T_TREE onwards is SOLID, so new walkable
// tiles must be listed BEFORE T_TREE.
enum {
  T_GRASS, T_PATH, T_FLOWER,
  T_FLOOR1, T_FLOOR2, T_MAT,            // inside the Center: walkable
  T_TREE,
  T_ROOF_H1, T_ROOF_H2, T_WALL_H, T_DOOR_H,
  T_ROOF_C, T_SIGN_C, T_WALL_C, T_DOOR_C,
  T_ROOF_M, T_SIGN_M, T_WALL_M, T_DOOR_M,
  // inside the Center (solid):
  T_I_WALL, T_I_WALL_BOT, T_I_SIDE_L, T_I_SIDE_R, T_I_BANNER,
  T_COUNTER_L, T_COUNTER_M, T_COUNTER_R,
  T_MACH_TL, T_MACH_TR, T_MACH_BL, T_MACH_BR,   // must stay in this order
  T_PLANT, T_EXIT_DOOR,
  NUM_TILES
};

// TOWN MAP. G grass, P path, F flowers, T tree (solid)
// House: H/J roof, h wall, D door. Center: R roof, r sign, C wall, c door.
// Mart: U roof, u sign, M wall, m door.  Keep the top row all trees.
static const char town_rows[MAP_H][MAP_W + 1] = {
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

// POKEMON CENTER INSIDE. f floor, m door mat, E exit door,
// W/V/L/R walls (top/bottom/left/right), S wall sign,
// a b c counter (left/middle/right), M N / O P healing machine
// (top-left, top-right / bottom-left, bottom-right), p plant.
// Keep the border all walls and E in the bottom row.
static const char inside_rows[MAP_H][MAP_W + 1] = {
"WWWWWWWWWWWWWWW",
"WWWWWWWSWWWWMNW",
"LfffffffffffOPR",
"LfffffffffffffR",
"LfffabbbbbcfffR",
"LfffffffffffffR",
"LpfffffffffffpR",
"LfffffffffffffR",
"LfffffmmmfffffR",
"VVVVVVVEVVVVVVV"
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
static unsigned char map_kind[MAP_H][MAP_W];   // the map you are in right now
static int inside = 0;                         // 0 = town, 1 = Pokemon Center
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

// Turn the town letters into tile kinds.
static int town_kind(char c) {
  switch (c) {
    case 'P': return T_PATH;
    case 'F': return T_FLOWER;
    case 'T': return T_TREE;
    case 'H': return T_ROOF_H1;
    case 'J': return T_ROOF_H2;
    case 'h': return T_WALL_H;
    case 'D': return T_DOOR_H;
    case 'R': return T_ROOF_C;
    case 'r': return T_SIGN_C;
    case 'C': return T_WALL_C;
    case 'c': return T_DOOR_C;
    case 'U': return T_ROOF_M;
    case 'u': return T_SIGN_M;
    case 'M': return T_WALL_M;
    case 'm': return T_DOOR_M;
    default:  return T_GRASS;
  }
}

// Turn the Pokemon Center letters into tile kinds.
// (tx, ty) are needed so the floor can alternate light and dark.
static int inside_kind(char c, int tx, int ty) {
  switch (c) {
    case 'W': return T_I_WALL;
    case 'V': return T_I_WALL_BOT;
    case 'L': return T_I_SIDE_L;
    case 'R': return T_I_SIDE_R;
    case 'S': return T_I_BANNER;
    case 'a': return T_COUNTER_L;
    case 'b': return T_COUNTER_M;
    case 'c': return T_COUNTER_R;
    case 'M': return T_MACH_TL;
    case 'N': return T_MACH_TR;
    case 'O': return T_MACH_BL;
    case 'P': return T_MACH_BR;
    case 'p': return T_PLANT;
    case 'm': return T_MAT;
    case 'E': return T_EXIT_DOOR;
    default:  return ((tx + ty) & 1) ? T_FLOOR2 : T_FLOOR1;
  }
}

// Fill map_kind from the town (0) or the Pokemon Center (1).
static void load_map(int which) {
  const char (*rows)[MAP_W + 1] = which ? inside_rows : town_rows;
  int tx, ty;
  for (ty = 0; ty < MAP_H; ty++) {
    for (tx = 0; tx < MAP_W; tx++) {
      int k = which ? inside_kind(rows[ty][tx], tx, ty)
                    : town_kind(rows[ty][tx]);
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

// Same, but on a block of 4 tiles (2 wide, 2 tall = 32x32 pixels).
// 'first' is the top-left tile; first+1 top-right, +2 bottom-left, +3 bottom-right.
static void big_box(int first, int x0, int y0, int x1, int y1,
                    unsigned short c) {
  int x, y;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      tile_gfx[first + (y >> 4) * 2 + (x >> 4)][(y & 15) * TILE + (x & 15)] = c;
}

static void copy_tile(int to, int from) {
  int i;
  for (i = 0; i < TILE * TILE; i++) tile_gfx[to][i] = tile_gfx[from][i];
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

// ---- Pokemon Center inside: tile art ----

// Tiled floor: a base color with a darker line on the right and bottom.
static void make_floor(unsigned short* t, unsigned short base) {
  unsigned short grout = RGB15(21, 17, 11);
  unsigned short shine = RGB15(31, 30, 26);
  tile_box(t, 0, 0, TILE, TILE, base);
  tile_box(t, 15, 0, TILE, TILE, grout);
  tile_box(t, 0, 15, TILE, TILE, grout);
  t[3 * TILE + 3] = shine;
  t[3 * TILE + 4] = shine;
  t[4 * TILE + 3] = shine;
}

// Pink wall with a darker strip ("baseboard") where it meets the floor.
// board_low = 1: strip at the bottom (wall above the floor).
// board_low = 0: strip at the top (wall below the floor).
static void make_room_wall(unsigned short* t, int board_low) {
  unsigned short dark  = RGB15(20, 13, 12);
  unsigned short light = RGB15(26, 19, 18);
  tile_box(t, 0, 0, TILE, TILE, RGB15(30, 24, 23));
  tile_box(t, 0, 6, TILE, 8, RGB15(27, 20, 20));
  if (board_low) {
    tile_box(t, 0, 12, TILE, TILE, dark);
    tile_box(t, 0, 12, TILE, 13, light);
  } else {
    tile_box(t, 0, 0, TILE, 4, dark);
    tile_box(t, 0, 3, TILE, 4, light);
  }
}

// Left/right wall with the strip on the side that faces the room.
static void make_side_wall(unsigned short* t, int left_side) {
  unsigned short dark  = RGB15(20, 13, 12);
  unsigned short light = RGB15(26, 19, 18);
  tile_box(t, 0, 0, TILE, TILE, RGB15(30, 24, 23));
  if (left_side) {
    tile_box(t, 12, 0, TILE, TILE, dark);
    tile_box(t, 12, 0, 13, TILE, light);
  } else {
    tile_box(t, 0, 0, 4, TILE, dark);
    tile_box(t, 3, 0, 4, TILE, light);
  }
}

// Wooden reception counter piece (left end, middle, or right end).
static void make_counter(unsigned short* t, int left_end, int right_end) {
  unsigned short edge = RGB15(10, 5, 3);
  tile_box(t, 0, 0, TILE, TILE, RGB15(19, 10, 5));           // front
  tile_box(t, 0, 0, TILE, 8, RGB15(27, 16, 8));              // top surface
  tile_box(t, 0, 1, TILE, 2, RGB15(30, 21, 12));             // shine
  tile_box(t, 0, 0, TILE, 1, edge);
  tile_box(t, 0, 15, TILE, TILE, edge);
  tile_box(t, 0, 8, TILE, 9, edge);
  tile_box(t, 2, 10, 14, 14, RGB15(23, 13, 7));              // panel
  if (left_end)  { tile_box(t, 0, 0, 1, TILE, edge); tile_box(t, 1, 1, 2, 8, RGB15(30, 21, 12)); }
  if (right_end) { tile_box(t, 15, 0, TILE, TILE, edge); }
}

// Potted plant on the floor.
static void make_plant(unsigned short* t) {
  copy_tile(T_PLANT, T_FLOOR2);
  tile_box(t, 3, 1, 13, 9, RGB15(4, 15, 8));
  tile_box(t, 4, 2, 12, 8, RGB15(4, 24, 13));
  tile_box(t, 5, 3, 8, 5, RGB15(10, 28, 16));
  tile_box(t, 4, 9, 12, 10, RGB15(21, 12, 6));
  tile_box(t, 5, 10, 11, 15, RGB15(17, 10, 5));
  tile_box(t, 5, 14, 11, 15, RGB15(10, 5, 3));
}

// The healing machine: 2x2 tiles (32x32 pixels).
static void make_machine(void) {
  int first = T_MACH_TL;
  int bx, i;
  unsigned short red = RGB15(27, 5, 7);
  // background: wall behind the top half, floor behind the bottom half
  copy_tile(T_MACH_TL, T_I_WALL);
  copy_tile(T_MACH_TR, T_I_WALL);
  copy_tile(T_MACH_BL, T_FLOOR1);
  copy_tile(T_MACH_BR, T_FLOOR1);
  big_box(first, 1, 2, 31, 32, OUTLINE);                       // outline
  big_box(first, 2, 3, 30, 31, RGB15(28, 29, 31));             // body
  big_box(first, 2, 28, 30, 31, RGB15(20, 22, 26));            // lower shade
  big_box(first, 3, 4, 29, 10, RGB15(6, 15, 29));              // blue band
  big_box(first, 3, 4, 29, 5, RGB15(12, 21, 31));
  big_box(first, 5, 12, 27, 20, RGB15(4, 6, 10));              // screen frame
  big_box(first, 6, 13, 26, 19, RGB15(4, 24, 13));             // screen
  big_box(first, 15, 14, 17, 18, WHITE);                       // white cross
  big_box(first, 13, 15, 19, 17, WHITE);
  for (i = 0; i < 3; i++) {                                    // three balls
    bx = 5 + i * 8;
    big_box(first, bx, 22, bx + 6, 24, red);
    big_box(first, bx, 24, bx + 6, 25, OUTLINE);
    big_box(first, bx, 25, bx + 6, 28, WHITE);
    big_box(first, bx + 2, 24, bx + 4, 25, WHITE);
  }
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

  // ---- Inside the Pokemon Center ----
  make_floor(tile_gfx[T_FLOOR1], RGB15(29, 26, 20));
  make_floor(tile_gfx[T_FLOOR2], RGB15(26, 22, 16));

  // door mat: red with cream stripes (3 mat tiles side by side join up)
  tile_box(tile_gfx[T_MAT], 0, 0, TILE, TILE, RGB15(27, 5, 7));
  tile_box(tile_gfx[T_MAT], 0, 0, TILE, 2, RGB15(31, 28, 22));
  tile_box(tile_gfx[T_MAT], 0, 14, TILE, TILE, RGB15(31, 28, 22));
  tile_box(tile_gfx[T_MAT], 0, 2, TILE, 4, RGB15(20, 3, 5));
  tile_box(tile_gfx[T_MAT], 0, 12, TILE, 14, RGB15(20, 3, 5));
  tile_box(tile_gfx[T_MAT], 0, 7, TILE, 9, RGB15(31, 9, 10));

  make_room_wall(tile_gfx[T_I_WALL], 1);
  make_room_wall(tile_gfx[T_I_WALL_BOT], 0);
  make_side_wall(tile_gfx[T_I_SIDE_L], 1);
  make_side_wall(tile_gfx[T_I_SIDE_R], 0);

  make_room_wall(tile_gfx[T_I_BANNER], 1);
  add_sign(tile_gfx[T_I_BANNER], glyph_p, RGB15(29, 3, 3));

  make_room_wall(tile_gfx[T_EXIT_DOOR], 0);
  add_door(tile_gfx[T_EXIT_DOOR], RGB15(18, 26, 30), RGB15(12, 12, 14));
  tile_box(tile_gfx[T_EXIT_DOOR], 7, 2, 9, TILE, RGB15(12, 12, 14));

  make_counter(tile_gfx[T_COUNTER_L], 1, 0);
  make_counter(tile_gfx[T_COUNTER_M], 0, 0);
  make_counter(tile_gfx[T_COUNTER_R], 0, 1);

  make_plant(tile_gfx[T_PLANT]);
  make_machine();
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

// Trees, buildings, walls, the counter... (kind >= T_TREE) and the screen
// edge block you.
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

// Put the trainer on map square (tx, ty). If that spot is blocked (for
// example after you edit a map), use the first free square we can find.
static void pick_spot(int tx0, int ty0, int* out_x, int* out_y) {
  int tx, ty;
  *out_x = tx0 * TILE;
  *out_y = ty0 * TILE;
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

// What kind of tile is the trainer looking at, right in front of them?
// (-1 if that is outside the screen.)
static int tile_in_front(int px, int py, int dir) {
  int fx, fy;
  if (dir == DIR_UP)        { fx = px + 8;             fy = py + HB_TOP - 2;    }
  else if (dir == DIR_DOWN) { fx = px + 8;             fy = py + HB_BOTTOM + 2; }
  else if (dir == DIR_LEFT) { fx = px + HB_LEFT - 2;   fy = py + 8;             }
  else                      { fx = px + HB_RIGHT + 2;  fy = py + 8;             }
  if (fx < 0 || fy < 0 || fx >= SCREEN_W || fy >= SCREEN_H) return -1;
  return map_kind[fy >> 4][fx >> 4];
}

// Go into the Pokemon Center (to_inside = 1) or back out to town (0).
// The screen is blanked while the new picture is painted, so you never
// see it half-drawn.
static void change_map(int to_inside, int* x, int* y, int* dir) {
  REG_DISPCNT = 0x0080;               // forced blank
  inside = to_inside;
  load_map(inside);
  if (inside) {
    pick_spot(INSIDE_START_TX, INSIDE_START_TY, x, y);
    *dir = DIR_UP;
  } else {
    pick_spot(TOWN_EXIT_TX, TOWN_EXIT_TY, x, y);
    *dir = DIR_DOWN;
  }
  walk_frame = 0;
  walk_timer = 0;
  paint_box(0, 0, SCREEN_W, SCREEN_H, *x, *y, *dir);
  REG_DISPCNT = 0x0403;               // Mode 3 + show BG2
}

#if DEBUG_HUD
// Lamps live in the top wall/tree row, where the trainer can never walk.
static void lamp(int slot, int on, unsigned short color) {
  int lx = 2 + slot * 8;
  int x, y;
  for (y = 2; y < 8; y++)
    for (x = lx; x < lx + 6; x++)
      VRAM[y * SCREEN_W + x] = on ? color : RGB15(4, 4, 4);
}

// 0 blinks = loop runs; 1-4 = LEFT/RIGHT/UP/DOWN; 5 = moved; 6 = blocked;
// 7 (orange) = A button
static void show_lamps(unsigned int frame, unsigned short keys,
                       int moved, int blocked) {
  lamp(0, (frame >> 5) & 1, RGB15(31, 0, 31));
  lamp(1, keys & KEY_LEFT,  RGB15(0, 31, 31));
  lamp(2, keys & KEY_RIGHT, RGB15(0, 31, 31));
  lamp(3, keys & KEY_UP,    RGB15(0, 31, 31));
  lamp(4, keys & KEY_DOWN,  RGB15(0, 31, 31));
  lamp(5, moved,            RGB15(31, 31, 31));
  lamp(6, blocked,          RGB15(31, 0, 0));
  lamp(7, keys & KEY_A,     RGB15(31, 20, 0));
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
  unsigned short prev_keys = 0;

  REG_DISPCNT = 0x0080;   // forced blank while we build the first picture
  build_tiles();
  load_map(0);
  build_sprites();
  pick_spot(START_TX, START_TY, &x, &y);
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

    // A button: only the moment it is pressed (holding it does nothing).
    // Town: face the Center's door -> go in.  Inside: face the exit door -> go out.
    if ((keys & KEY_A) && !(prev_keys & KEY_A)) {
      int front = tile_in_front(x, y, dir);
      if (!inside && dir == DIR_UP && front == T_DOOR_C)
        change_map(1, &x, &y, &dir);
      else if (inside && dir == DIR_DOWN && front == T_EXIT_DOOR)
        change_map(0, &x, &y, &dir);
    }
    prev_keys = keys;

#if DEBUG_HUD
    show_lamps(frame, keys, moved,
               ((dx != 0 || dy != 0) && !moved));
#endif
  }
}
// END OF FILE
