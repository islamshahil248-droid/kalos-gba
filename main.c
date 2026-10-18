// GBA town with 4 enterable buildings. Mode 3, 240x160, devkitARM, no libraries.
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
#define START_TX 6    // where the trainer starts in town
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

// Rooms. Each room has its OWN tile numbers (0 = first tile of that room)
// and only the current room's pictures are kept in memory.
enum { R_TOWN, R_CENTER, R_MART, R_HOUSE1, R_HOUSE2, NUM_ROOMS };

// Town tiles (3 and up are SOLID). MAX_TILES = biggest tile set of any room.
enum {
  TW_GRASS, TW_PATH, TW_FLOWER, TW_TREE,
  TW_ROOF_H1, TW_ROOF_H2, TW_WALL_H, TW_DOOR_H,
  TW_ROOF_C, TW_SIGN_C, TW_WALL_C, TW_DOOR_C,
  TW_ROOF_M, TW_SIGN_M, TW_WALL_M, TW_DOOR_M,
  MAX_TILES
};
// Inside tiles: 0,1 = checker floor, 2 = rug/mat (walkable); 3 and up SOLID.
enum { IC_FA, IC_FB, IC_MAT, IC_WALL, IC_SIGN, IC_CNT, IC_CNTF, IC_MCH_T,
       IC_MCH_B, IC_PLANT, IC_DOOR };
enum { IM_FA, IM_FB, IM_MAT, IM_WALL, IM_SIGN, IM_DOOR, IM_CNT, IM_CNTF,
       IM_SHELF_A, IM_SHELF_B, IM_NPC, IM_PLANT };
enum { H1_FA, H1_FB, H1_RUG, H1_WALL, H1_WIN, H1_DOOR, H1_BED_H, H1_BED_F,
       H1_TABLE, H1_CHAIR, H1_PLANT, H1_NPC };
enum { H2_FA, H2_FB, H2_RUG, H2_WALL, H2_WIN, H2_DOOR, H2_SHELF_A, H2_SHELF_B,
       H2_BED_H, H2_BED_F, H2_TABLE, H2_CHAIR, H2_PLANT, H2_NPC };
#define SOLID_INSIDE 3

typedef struct {
  const char (*rows)[MAP_W + 1];     // the map letters
  const char* keys;                  // letters we know ...
  const unsigned char* kinds;        // ... and the tile each one means
  unsigned char solid_from;          // tile numbers >= this block you
  unsigned char checker;             // 1: '.' alternates tile 0 / tile 1
  void (*build)(void);               // paints this room's tile pictures
} Room;

typedef struct {                     // one door: where it is, where it leads
  unsigned char from, tx, ty;        // room and tile of the door
  unsigned char to, sx, sy, sdir;    // room, tile and facing after using it
} Door;

// ---------- Town ----------
// G grass, P path, F flowers, T tree. House: H/J roof, h wall, D door.
// Center: R roof, r sign, C wall, c door. Mart: U roof, u sign, M wall, m door.
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
static const char town_keys[] = "GPFTHJhDRrCcUuMm";
static const unsigned char town_kinds[] = {
  TW_GRASS, TW_PATH, TW_FLOWER, TW_TREE, TW_ROOF_H1, TW_ROOF_H2, TW_WALL_H,
  TW_DOOR_H, TW_ROOF_C, TW_SIGN_C, TW_WALL_C, TW_DOOR_C, TW_ROOF_M,
  TW_SIGN_M, TW_WALL_M, TW_DOOR_M
};

// ---------- Interiors ----------
// Common: . floor, W wall, S wall+sign, w wall+window, c exit door, _ rug/mat,
// p plant, n NPC. Row 0 and the side walls are always solid (HUD lamps there).
// Objects drawn on the floor (p, n, h) must sit where (column+row) is EVEN.

// Pokemon Center: K/k counter top/front, M/N healing machine top/bottom.
static const char center_rows[MAP_H][MAP_W + 1] = {
"WWWWWWWSWWWWWWW",
"Wp.........MM.W",
"W..........NN.W",
"W..KKKKKKK....W",
"W..kkkkkkk....W",
"W.............W",
"W...........p.W",
"W.............W",
"W.....___.....W",
"WWWWWWWcWWWWWWW"
};
static const char center_keys[] = ".pWSKkMN_c";
static const unsigned char center_kinds[] = {
  IC_FA, IC_PLANT, IC_WALL, IC_SIGN, IC_CNT, IC_CNTF, IC_MCH_T, IC_MCH_B,
  IC_MAT, IC_DOOR
};

// Poke Mart: a/b shelves, K/k counter, shopkeeper n behind the counter.
static const char mart_rows[MAP_H][MAP_W + 1] = {
"WWWWWWWSWWWWWWW",
"Wabab.....babaW",
"Wabab.n...babaW",
"W....KKKKK....W",
"W....kkkkk....W",
"W.............W",
"W..aa.....bb..W",
"Wp...........pW",
"W.....___.....W",
"WWWWWWWcWWWWWWW"
};
static const char mart_keys[] = ".pWSKkabn_c";
static const unsigned char mart_kinds[] = {
  IM_FA, IM_PLANT, IM_WALL, IM_SIGN, IM_CNT, IM_CNTF, IM_SHELF_A, IM_SHELF_B,
  IM_NPC, IM_MAT, IM_DOOR
};

// House 1: B/b bed head/foot, T table, h chair, n mother.
static const char house1_rows[MAP_H][MAP_W + 1] = {
"WWWwWWWWWWWwWWW",
"WB...........pW",
"Wb............W",
"W......h...n..W",
"W.....TTh.....W",
"W......h......W",
"W.............W",
"W............pW",
"W.....___.....W",
"WWWWWWWcWWWWWWW"
};
static const char house1_keys[] = ".WwcBbThpn_";
static const unsigned char house1_kinds[] = {
  H1_FA, H1_WALL, H1_WIN, H1_DOOR, H1_BED_H, H1_BED_F, H1_TABLE, H1_CHAIR,
  H1_PLANT, H1_NPC, H1_RUG
};

// House 2: k/l bookshelves, B/b bed, T table, h chair, n grandfather.
static const char house2_rows[MAP_H][MAP_W + 1] = {
"WWWWWwWWWwWWWWW",
"Wklk.........BW",
"Wlkl........pbW",
"W.............W",
"W.....hTT.....W",
"W......h......W",
"W.........n...W",
"W............pW",
"W.___.........W",
"WWWcWWWWWWWWWWW"
};
static const char house2_keys[] = ".WwcklBbThpn_";
static const unsigned char house2_kinds[] = {
  H2_FA, H2_WALL, H2_WIN, H2_DOOR, H2_SHELF_A, H2_SHELF_B, H2_BED_H, H2_BED_F,
  H2_TABLE, H2_CHAIR, H2_PLANT, H2_NPC, H2_RUG
};

// ---------- Doors ----------
// Entering lands on the mat just inside; leaving lands just below the door.
static const Door doors[] = {
  { R_TOWN,   7, 2, R_CENTER, 7, 8, DIR_UP   },
  { R_CENTER, 7, 9, R_TOWN,   7, 3, DIR_DOWN },
  { R_TOWN,  12, 2, R_MART,   7, 8, DIR_UP   },
  { R_MART,   7, 9, R_TOWN,  12, 3, DIR_DOWN },
  { R_TOWN,   2, 2, R_HOUSE1, 7, 8, DIR_UP   },
  { R_HOUSE1, 7, 9, R_TOWN,   2, 3, DIR_DOWN },
  { R_TOWN,  10, 6, R_HOUSE2, 3, 8, DIR_UP   },
  { R_HOUSE2, 3, 9, R_TOWN,  10, 7, DIR_DOWN }
};
#define NUM_DOORS ((int)(sizeof(doors) / sizeof(doors[0])))

// ---------- Trainer art ----------
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

// ---------- Memory (about 12 KB in total) ----------
static unsigned short tile_gfx[MAX_TILES][TILE * TILE];   // current room only
static unsigned short sprite_gfx[4][2][TILE * TILE];
static unsigned char map_kind[NUM_ROOMS][MAP_H][MAP_W];
static int cur_room = R_TOWN;
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

// ---------- Tile drawing helpers ----------
// Fill a rectangle inside one tile (x1/y1 not included).
static void tile_box(unsigned short* t, int x0, int y0, int x1, int y1,
                     unsigned short c) {
  int x, y;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      t[y * TILE + x] = c;
}

static void copy_tile(unsigned short* d, const unsigned short* s) {
  int i;
  for (i = 0; i < TILE * TILE; i++) d[i] = s[i];
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

static void make_floor(unsigned short* t, unsigned short base,
                       unsigned short edge) {
  int x, y;
  for (y = 0; y < TILE; y++)
    for (x = 0; x < TILE; x++)
      t[y * TILE + x] = (x == TILE - 1 || y == TILE - 1) ? edge : base;
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

// wall + door; split = 1 adds a line down the middle (sliding glass doors)
static void make_door_tile(unsigned short* t, unsigned short wall,
                           unsigned short shade, unsigned short door,
                           unsigned short frame, int split) {
  make_wall(t, wall, shade);
  add_door(t, door, frame);
  if (split) tile_box(t, 7, 2, 9, TILE, frame);
}

static void make_rug(unsigned short* t, unsigned short base,
                     unsigned short border) {
  tile_box(t, 0, 0, TILE, TILE, base);
  tile_box(t, 2, 2, 14, 14, border);
  tile_box(t, 3, 3, 13, 13, base);
}

static void make_counter(unsigned short* top, unsigned short* front,
                         unsigned short hi, unsigned short surf,
                         unsigned short face, unsigned short panel,
                         unsigned short dark) {
  tile_box(top, 0, 0, TILE, TILE, face);
  tile_box(top, 0, 0, TILE, 8, surf);
  tile_box(top, 0, 1, TILE, 2, hi);
  tile_box(top, 0, 8, TILE, 9, dark);
  tile_box(front, 0, 0, TILE, TILE, face);
  tile_box(front, 1, 2, 15, 13, panel);
  tile_box(front, 0, 14, TILE, TILE, dark);
}

// Shelf with 6 colored items (c[0..2] top row, c[3..5] bottom row)
static void make_shelf(unsigned short* t, const unsigned short* c) {
  int i;
  tile_box(t, 0, 0, TILE, TILE, RGB15(14, 8, 4));
  tile_box(t, 1, 1, 15, 15, RGB15(7, 5, 4));
  tile_box(t, 1, 7, 15, 8, RGB15(20, 12, 6));
  tile_box(t, 1, 14, 15, 15, RGB15(20, 12, 6));
  for (i = 0; i < 3; i++) {
    tile_box(t, 2 + i * 4, 3, 5 + i * 4, 7, c[i]);
    tile_box(t, 2 + i * 4, 10, 5 + i * 4, 14, c[i + 3]);
    t[3 * TILE + 3 + i * 4] = WHITE;
    t[10 * TILE + 3 + i * 4] = WHITE;
  }
}

// Bookshelf: two rows of colored books
static void make_books(unsigned short* t, int off) {
  static const unsigned short bc[4] = {
    RGB15(27, 5, 7), RGB15(6, 15, 29), RGB15(31, 27, 5), RGB15(4, 22, 10)
  };
  int i, r;
  tile_box(t, 0, 0, TILE, TILE, RGB15(14, 8, 4));
  tile_box(t, 1, 1, 15, 15, RGB15(7, 5, 4));
  tile_box(t, 1, 7, 15, 8, RGB15(20, 12, 6));
  tile_box(t, 1, 14, 15, 15, RGB15(20, 12, 6));
  for (r = 0; r < 2; r++)
    for (i = 0; i < 6; i++)
      tile_box(t, 2 + i * 2, 2 + r * 7 + ((i + r) & 1), 4 + i * 2,
               7 + r * 7, bc[(i + off + r * 2) & 3]);
}

static void make_bed_head(unsigned short* t, unsigned short frame,
                          unsigned short blanket) {
  unsigned short sheet = RGB15(29, 29, 31);
  tile_box(t, 0, 0, TILE, TILE, frame);
  tile_box(t, 1, 1, 15, TILE, sheet);
  tile_box(t, 3, 2, 13, 7, WHITE);
  tile_box(t, 3, 6, 13, 7, RGB15(24, 24, 27));
  tile_box(t, 1, 8, 15, TILE, blanket);
  tile_box(t, 1, 8, 15, 9, sheet);
}

static void make_bed_foot(unsigned short* t, unsigned short frame,
                          unsigned short blanket) {
  tile_box(t, 0, 0, TILE, TILE, frame);
  tile_box(t, 1, 0, 15, 13, blanket);
  tile_box(t, 1, 3, 15, 4, RGB15(29, 29, 31));
}

static void make_table(unsigned short* t, unsigned short top,
                       unsigned short edge, unsigned short light) {
  tile_box(t, 0, 0, TILE, TILE, top);
  tile_box(t, 0, 0, TILE, 2, edge);
  tile_box(t, 0, 14, TILE, TILE, edge);
  tile_box(t, 6, 6, 10, 10, light);
}

// Furniture that stands on the floor copies floor tile 0 first.
static void make_chair(unsigned short* t, unsigned short wood,
                       unsigned short dark) {
  copy_tile(t, tile_gfx[0]);
  tile_box(t, 4, 3, 12, 5, dark);
  tile_box(t, 4, 5, 12, 12, wood);
}

static void make_plant(unsigned short* t) {
  copy_tile(t, tile_gfx[0]);
  tile_box(t, 3, 2, 13, 10, RGB15(3, 16, 7));
  tile_box(t, 5, 3, 11, 8, RGB15(6, 24, 10));
  tile_box(t, 4, 10, 12, 15, RGB15(17, 10, 5));
  tile_box(t, 3, 10, 13, 11, RGB15(12, 6, 3));
}

// Small person; hat = CLEAR for none.
static void make_npc(unsigned short* t, unsigned short hair,
                     unsigned short shirt, unsigned short pants,
                     unsigned short hat) {
  unsigned short skin = RGB15(31, 24, 18);
  copy_tile(t, tile_gfx[0]);
  tile_box(t, 5, 2, 11, 4, hair);
  tile_box(t, 5, 4, 11, 8, skin);
  tile_box(t, 6, 5, 7, 6, OUTLINE);
  tile_box(t, 9, 5, 10, 6, OUTLINE);
  if (hat != CLEAR) tile_box(t, 4, 1, 12, 4, hat);
  tile_box(t, 4, 8, 12, 13, shirt);
  tile_box(t, 2, 8, 4, 12, shirt);
  tile_box(t, 12, 8, 14, 12, shirt);
  tile_box(t, 2, 12, 4, 13, skin);
  tile_box(t, 12, 12, 14, 13, skin);
  tile_box(t, 5, 13, 11, 16, pants);
  tile_box(t, 5, 15, 8, 16, OUTLINE);
  tile_box(t, 8, 15, 11, 16, OUTLINE);
}

// ---------- One builder per room ----------
static void build_town(void) {
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
      tile_gfx[TW_GRASS][i] = grass;
      if (pattern == 0 || pattern == 5)
        tile_gfx[TW_GRASS][i] = grass_dark;
      if (pattern == 10 && (y & 1))
        tile_gfx[TW_GRASS][i] = grass_light;
      tile_gfx[TW_PATH][i] = dirt;
      if (pattern == 2 || pattern == 11)
        tile_gfx[TW_PATH][i] = dirt_dark;
      if (pattern == 7 && (x & 1))
        tile_gfx[TW_PATH][i] = dirt_light;
      tile_gfx[TW_FLOWER][i] = tile_gfx[TW_GRASS][i];
    }
  }

  // Flowers: pink, white, yellow
  tile_box(tile_gfx[TW_FLOWER], 4, 4, 6, 6, RGB15(31, 10, 22));
  tile_gfx[TW_FLOWER][5 * TILE + 5] = yel;
  tile_box(tile_gfx[TW_FLOWER], 10, 9, 12, 11, WHITE);
  tile_gfx[TW_FLOWER][10 * TILE + 11] = yel;
  tile_box(tile_gfx[TW_FLOWER], 5, 12, 7, 14, yel);

  // Tree over grass
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;
      unsigned short c = letter_color(art_tree[y][x]);
      tile_gfx[TW_TREE][i] = (c == CLEAR) ? tile_gfx[TW_GRASS][i] : c;
    }
  }

  // Houses: orange and purple roofs
  make_roof(tile_gfx[TW_ROOF_H1], RGB15(24, 10, 6), RGB15(28, 14, 9), RGB15(16, 6, 4));
  make_roof(tile_gfx[TW_ROOF_H2], RGB15(18, 10, 22), RGB15(22, 14, 26), RGB15(12, 6, 15));
  make_wall(tile_gfx[TW_WALL_H], RGB15(30, 26, 18), RGB15(25, 21, 13));
  add_window(tile_gfx[TW_WALL_H], RGB15(10, 6, 3));
  make_door_tile(tile_gfx[TW_DOOR_H], RGB15(30, 26, 18), RGB15(25, 21, 13),
                 RGB15(17, 10, 4), RGB15(9, 5, 2), 0);

  // Pokemon Center: red roof, P sign
  make_roof(tile_gfx[TW_ROOF_C], RGB15(29, 5, 5), RGB15(31, 10, 9), RGB15(20, 2, 3));
  make_roof(tile_gfx[TW_SIGN_C], RGB15(29, 5, 5), RGB15(31, 10, 9), RGB15(20, 2, 3));
  add_sign(tile_gfx[TW_SIGN_C], glyph_p, RGB15(29, 3, 3));
  make_wall(tile_gfx[TW_WALL_C], RGB15(31, 27, 26), RGB15(27, 22, 22));
  add_window(tile_gfx[TW_WALL_C], RGB15(12, 12, 14));
  make_door_tile(tile_gfx[TW_DOOR_C], RGB15(31, 27, 26), RGB15(27, 22, 22),
                 RGB15(18, 26, 30), RGB15(12, 12, 14), 1);

  // Poke Mart: blue roof, M sign
  make_roof(tile_gfx[TW_ROOF_M], RGB15(4, 10, 26), RGB15(8, 15, 30), RGB15(2, 5, 16));
  make_roof(tile_gfx[TW_SIGN_M], RGB15(4, 10, 26), RGB15(8, 15, 30), RGB15(2, 5, 16));
  add_sign(tile_gfx[TW_SIGN_M], glyph_m, RGB15(4, 10, 26));
  make_wall(tile_gfx[TW_WALL_M], RGB15(24, 28, 31), RGB15(19, 23, 28));
  add_window(tile_gfx[TW_WALL_M], RGB15(4, 6, 14));
  make_door_tile(tile_gfx[TW_DOOR_M], RGB15(24, 28, 31), RGB15(19, 23, 28),
                 RGB15(10, 18, 28), RGB15(4, 6, 14), 1);
}

static void build_center(void) {
  make_floor(tile_gfx[IC_FA], RGB15(25, 22, 16), RGB15(21, 18, 12));
  make_floor(tile_gfx[IC_FB], RGB15(28, 25, 19), RGB15(21, 18, 12));
  make_rug(tile_gfx[IC_MAT], RGB15(27, 5, 7), WHITE);
  make_wall(tile_gfx[IC_WALL], RGB15(31, 27, 26), RGB15(27, 22, 22));
  make_wall(tile_gfx[IC_SIGN], RGB15(31, 27, 26), RGB15(27, 22, 22));
  add_sign(tile_gfx[IC_SIGN], glyph_p, RGB15(29, 3, 3));
  make_door_tile(tile_gfx[IC_DOOR], RGB15(31, 27, 26), RGB15(27, 22, 22),
                 RGB15(18, 26, 30), RGB15(12, 12, 14), 1);
  make_counter(tile_gfx[IC_CNT], tile_gfx[IC_CNTF], RGB15(30, 20, 12),
               RGB15(26, 15, 8), RGB15(18, 10, 5), RGB15(22, 12, 6),
               RGB15(12, 6, 3));
  tile_box(tile_gfx[IC_MCH_T], 0, 0, TILE, TILE, RGB15(25, 25, 28));
  tile_box(tile_gfx[IC_MCH_T], 1, 1, 15, TILE, WHITE);
  tile_box(tile_gfx[IC_MCH_T], 3, 3, 13, 12, RGB15(6, 15, 29));
  tile_box(tile_gfx[IC_MCH_T], 5, 5, 11, 10, RGB15(4, 24, 13));
  tile_box(tile_gfx[IC_MCH_T], 7, 6, 9, 8, WHITE);
  tile_box(tile_gfx[IC_MCH_B], 0, 0, TILE, TILE, WHITE);
  tile_box(tile_gfx[IC_MCH_B], 0, 13, TILE, TILE, RGB15(19, 19, 23));
  tile_box(tile_gfx[IC_MCH_B], 2, 3, 6, 8, RGB15(27, 5, 7));
  tile_box(tile_gfx[IC_MCH_B], 10, 3, 14, 8, RGB15(27, 5, 7));
  tile_box(tile_gfx[IC_MCH_B], 3, 4, 5, 5, WHITE);
  tile_box(tile_gfx[IC_MCH_B], 11, 4, 13, 5, WHITE);
  make_plant(tile_gfx[IC_PLANT]);
}

static void build_mart(void) {
  static const unsigned short sa[6] = {
    RGB15(27, 5, 7), RGB15(6, 15, 29), RGB15(31, 27, 5),
    RGB15(4, 22, 10), RGB15(31, 14, 22), RGB15(20, 8, 26)
  };
  static const unsigned short sb[6] = {
    RGB15(31, 27, 5), RGB15(31, 31, 31), RGB15(27, 5, 7),
    RGB15(6, 15, 29), RGB15(27, 5, 7), RGB15(31, 31, 31)
  };
  make_floor(tile_gfx[IM_FA], RGB15(24, 27, 31), RGB15(19, 23, 28));
  make_floor(tile_gfx[IM_FB], RGB15(27, 29, 31), RGB15(19, 23, 28));
  make_rug(tile_gfx[IM_MAT], RGB15(6, 12, 26), WHITE);
  make_wall(tile_gfx[IM_WALL], RGB15(24, 28, 31), RGB15(19, 23, 28));
  make_wall(tile_gfx[IM_SIGN], RGB15(24, 28, 31), RGB15(19, 23, 28));
  add_sign(tile_gfx[IM_SIGN], glyph_m, RGB15(4, 10, 26));
  make_door_tile(tile_gfx[IM_DOOR], RGB15(24, 28, 31), RGB15(19, 23, 28),
                 RGB15(10, 18, 28), RGB15(4, 6, 14), 1);
  make_counter(tile_gfx[IM_CNT], tile_gfx[IM_CNTF], WHITE, RGB15(22, 26, 31),
               RGB15(6, 12, 26), RGB15(9, 16, 29), RGB15(2, 5, 16));
  make_shelf(tile_gfx[IM_SHELF_A], sa);
  make_shelf(tile_gfx[IM_SHELF_B], sb);
  make_npc(tile_gfx[IM_NPC], RGB15(12, 7, 3), RGB15(6, 15, 29),
           RGB15(4, 6, 14), RGB15(5, 11, 27));
  make_plant(tile_gfx[IM_PLANT]);
}

static void build_house1(void) {
  unsigned short wall = RGB15(31, 24, 20), shade = RGB15(26, 18, 15);
  make_floor(tile_gfx[H1_FA], RGB15(22, 14, 7), RGB15(16, 9, 4));
  make_floor(tile_gfx[H1_FB], RGB15(24, 16, 8), RGB15(16, 9, 4));
  make_rug(tile_gfx[H1_RUG], RGB15(27, 8, 8), RGB15(31, 24, 10));
  make_wall(tile_gfx[H1_WALL], wall, shade);
  make_wall(tile_gfx[H1_WIN], wall, shade);
  add_window(tile_gfx[H1_WIN], RGB15(10, 6, 3));
  make_door_tile(tile_gfx[H1_DOOR], wall, shade, RGB15(17, 10, 4),
                 RGB15(9, 5, 2), 0);
  make_bed_head(tile_gfx[H1_BED_H], RGB15(12, 7, 3), RGB15(27, 6, 8));
  make_bed_foot(tile_gfx[H1_BED_F], RGB15(12, 7, 3), RGB15(27, 6, 8));
  make_table(tile_gfx[H1_TABLE], RGB15(25, 15, 8), RGB15(14, 8, 4),
             RGB15(30, 22, 14));
  make_chair(tile_gfx[H1_CHAIR], RGB15(20, 11, 5), RGB15(12, 6, 3));
  make_plant(tile_gfx[H1_PLANT]);
  make_npc(tile_gfx[H1_NPC], RGB15(22, 12, 4), RGB15(31, 16, 20),
           RGB15(20, 10, 12), CLEAR);
}

static void build_house2(void) {
  unsigned short wall = RGB15(19, 27, 19), shade = RGB15(14, 22, 14);
  make_floor(tile_gfx[H2_FA], RGB15(13, 16, 25), RGB15(9, 11, 20));
  make_floor(tile_gfx[H2_FB], RGB15(15, 18, 27), RGB15(9, 11, 20));
  make_rug(tile_gfx[H2_RUG], RGB15(28, 17, 6), RGB15(31, 27, 12));
  make_wall(tile_gfx[H2_WALL], wall, shade);
  make_wall(tile_gfx[H2_WIN], wall, shade);
  add_window(tile_gfx[H2_WIN], RGB15(9, 6, 4));
  make_door_tile(tile_gfx[H2_DOOR], wall, shade, RGB15(12, 12, 16),
                 RGB15(6, 6, 9), 0);
  make_books(tile_gfx[H2_SHELF_A], 0);
  make_books(tile_gfx[H2_SHELF_B], 2);
  make_bed_head(tile_gfx[H2_BED_H], RGB15(9, 6, 4), RGB15(7, 11, 27));
  make_bed_foot(tile_gfx[H2_BED_F], RGB15(9, 6, 4), RGB15(7, 11, 27));
  make_table(tile_gfx[H2_TABLE], RGB15(17, 10, 6), RGB15(9, 5, 3),
             RGB15(24, 16, 10));
  make_chair(tile_gfx[H2_CHAIR], RGB15(14, 9, 6), RGB15(8, 5, 3));
  make_plant(tile_gfx[H2_PLANT]);
  make_npc(tile_gfx[H2_NPC], RGB15(26, 26, 26), RGB15(6, 18, 9),
           RGB15(10, 8, 4), CLEAR);
}

static const Room rooms[NUM_ROOMS] = {
  { town_rows,   town_keys,   town_kinds,   TW_TREE,     0, build_town   },
  { center_rows, center_keys, center_kinds, SOLID_INSIDE, 1, build_center },
  { mart_rows,   mart_keys,   mart_kinds,   SOLID_INSIDE, 1, build_mart   },
  { house1_rows, house1_keys, house1_kinds, SOLID_INSIDE, 1, build_house1 },
  { house2_rows, house2_keys, house2_kinds, SOLID_INSIDE, 1, build_house2 }
};

// Turn map letters into tile numbers (unknown letters: tile 0).
static void build_map(int m) {
  const Room* r = &rooms[m];
  int tx, ty, i;
  for (ty = 0; ty < MAP_H; ty++) {
    for (tx = 0; tx < MAP_W; tx++) {
      int k = 0;
      for (i = 0; r->keys[i]; i++)
        if (r->keys[i] == r->rows[ty][tx]) { k = r->kinds[i]; break; }
      if (r->checker && k == 0 && ((tx + ty) & 1)) k = 1;
      map_kind[m][ty][tx] = (unsigned char)k;
    }
  }
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
// so the trainer is never painted over and nothing is erased first.
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
          tile_gfx[map_kind[cur_room][sy >> 4][sx >> 4]][((sy & 15) << 4) | (sx & 15)];
      if (in_rows && sx >= px && sx < px + TILE) {
        unsigned short c = sprite_gfx[dir][walk_frame][(sy - py) * TILE + (sx - px)];
        if (c != CLEAR) color = c;
      }
      row[sx] = color;
    }
  }
}

static void wait_vblank(void) {
  while (REG_VCOUNT >= 160) {}
  while (REG_VCOUNT < 160) {}
}

// Full-screen repaint (start, entering, leaving). Mode 3 has ONE framebuffer,
// so a long repaint would be seen half-finished. Forced blank hides it.
static void show_map(int x, int y, int dir) {
  wait_vblank();
  REG_DISPCNT = 0x0080;
  paint_box(0, 0, SCREEN_W, SCREEN_H, x, y, dir);
  REG_DISPCNT = 0x0403;
}

// Tile number at a pixel of the current room, or -1 outside the screen.
static int tile_at(int px, int py) {
  if (px < 0 || py < 0 || px >= SCREEN_W || py >= SCREEN_H) return -1;
  return map_kind[cur_room][py >> 4][px >> 4];
}

// Solid tiles and the screen edge block you.
static int is_blocked(int px, int py) {
  int k = tile_at(px, py);
  return k < 0 || k >= rooms[cur_room].solid_from;
}

// Body box is smaller than a tile, so its 4 corners are enough.
static int can_stand(int px, int py) {
  return !is_blocked(px + HB_LEFT,  py + HB_TOP) &&
         !is_blocked(px + HB_RIGHT, py + HB_TOP) &&
         !is_blocked(px + HB_LEFT,  py + HB_BOTTOM) &&
         !is_blocked(px + HB_RIGHT, py + HB_BOTTOM);
}

// Index of the door at this pixel of the current room, or -1.
static int door_at(int px, int py) {
  int i, tx, ty;
  if (px < 0 || py < 0 || px >= SCREEN_W || py >= SCREEN_H) return -1;
  tx = px >> 4;
  ty = py >> 4;
  for (i = 0; i < NUM_DOORS; i++)
    if (doors[i].from == cur_room && doors[i].tx == tx && doors[i].ty == ty)
      return i;
  return -1;
}

// The door right in front of the trainer (within 3 px), or -1.
// Two probe points across the facing edge make it forgiving to line up.
static int front_door(int x, int y, int dir) {
  int ax, ay, bx, by, d;
  if (dir == DIR_UP)        { ay = by = y + HB_TOP - 3;    ax = x + HB_LEFT + 2; bx = x + HB_RIGHT - 2; }
  else if (dir == DIR_DOWN) { ay = by = y + HB_BOTTOM + 3; ax = x + HB_LEFT + 2; bx = x + HB_RIGHT - 2; }
  else if (dir == DIR_LEFT) { ax = bx = x + HB_LEFT - 3;   ay = y + HB_TOP + 2;  by = y + HB_BOTTOM - 2; }
  else                      { ax = bx = x + HB_RIGHT + 3;  ay = y + HB_TOP + 2;  by = y + HB_BOTTOM - 2; }
  d = door_at(ax, ay);
  if (d < 0) d = door_at(bx, by);
  return d;
}

// Put the trainer on tile (stx, sty); if that is blocked use the first free tile.
static void pick_start(int stx, int sty, int* out_x, int* out_y) {
  int tx, ty;
  *out_x = stx * TILE;
  *out_y = sty * TILE;
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
// Lamps live in the top row (always solid), where the trainer can't walk.
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

int main(void) {
  int x, y;
  int dir = DIR_UP;
  int n, m;
  unsigned int frame = 0;
  unsigned short prev_keys = 0;

  REG_DISPCNT = 0x0080;   // forced blank while we build everything
  for (m = 0; m < NUM_ROOMS; m++) build_map(m);
  build_sprites();
  cur_room = R_TOWN;
  rooms[cur_room].build();
  pick_start(START_TX, START_TY, &x, &y);
  show_map(x, y, dir);

#if DEBUG_HUD
  lamp(0, 1, RGB15(0, 31, 0));
#endif

  while (1) {
    unsigned short keys;
    int old_x = x, old_y = y, old_dir = dir;
    int old_walk_frame = walk_frame;
    int dx = 0, dy = 0;
    int moved, door;
    wait_vblank();
    frame++;
    keys = (unsigned short)(~REG_KEYINPUT) & 0x03FF;

    // A button: only on the press (not while held), only facing a door.
    door = ((keys & KEY_A) && !(prev_keys & KEY_A)) ? front_door(x, y, dir) : -1;
    if (door >= 0) {
      cur_room = doors[door].to;
      rooms[cur_room].build();          // this room's pictures
      pick_start(doors[door].sx, doors[door].sy, &x, &y);
      dir = doors[door].sdir;
      walk_frame = 0;
      walk_timer = 0;
      show_map(x, y, dir);
      prev_keys = keys;
      continue;
    }
    prev_keys = keys;

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

    // Repaint only one box covering the old and new trainer position.
    if (moved || dir != old_dir || walk_frame != old_walk_frame) {
      int left   = (x < old_x) ? x : old_x;
      int top    = (y < old_y) ? y : old_y;
      int right  = ((x > old_x) ? x : old_x) + TILE;
      int bottom = ((y > old_y) ? y : old_y) + TILE;
      paint_box(left, top, right, bottom, x, y, dir);
    }

#if DEBUG_HUD
    show_lamps(frame, keys, moved, ((dx != 0 || dy != 0) && !moved));
#endif
  }
}
// END OF FILE
