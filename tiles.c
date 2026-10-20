// tiles.c - paints the pictures: tile helpers, one builder per room, and
// the trainer sprites.
#include "game.h"

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
void build_town(void) {
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

  // Townspeople (they stand on grass: make_npc copies tile 0 = grass)
  make_npc(tile_gfx[TW_NPC_A], RGB15(12, 7, 3), RGB15(31, 18, 6),
           RGB15(6, 9, 20), RGB15(5, 20, 8));          // boy: green cap
  make_npc(tile_gfx[TW_NPC_B], RGB15(24, 22, 28), RGB15(20, 10, 26),
           RGB15(9, 8, 16), CLEAR);                    // woman: purple top
}

void build_center(void) {
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
  // Nurse: white cap with a red cross, pink hair at the sides
  make_npc(tile_gfx[IC_NPC], RGB15(31, 18, 24), RGB15(31, 25, 27),
           RGB15(30, 22, 26), WHITE);
  tile_box(tile_gfx[IC_NPC], 3, 3, 5, 7, RGB15(31, 18, 24));
  tile_box(tile_gfx[IC_NPC], 11, 3, 13, 7, RGB15(31, 18, 24));
  tile_box(tile_gfx[IC_NPC], 7, 1, 9, 4, RGB15(27, 5, 7));
  tile_box(tile_gfx[IC_NPC], 6, 2, 10, 3, RGB15(27, 5, 7));
}

void build_mart(void) {
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

void build_house1(void) {
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

void build_house2(void) {
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
// Route 1: the town's grass, path, flowers and tree (same numbers), plus tall
// grass (slot 3) and a wooden sign (slot 5). Calling build_town first means no
// grass or tree art is written twice.
typedef char route_tiles_match_town[((int)RT_GRASS == (int)TW_GRASS &&
                                     (int)RT_PATH == (int)TW_PATH &&
                                     (int)RT_FLOWER == (int)TW_FLOWER) ? 1 : -1];
void build_route1(void) {
  unsigned short* tall = tile_gfx[RT_TALL];
  unsigned short* sign = tile_gfx[RT_SIGN];
  int x, y;

  build_town();
  copy_tile(tile_gfx[RT_TREE], tile_gfx[TW_TREE]);   // before slot 3 is replaced

  // Tall grass: darker than grass, with light blade tufts.
  tile_box(tall, 0, 0, TILE, TILE, RGB15(4, 15, 5));
  for (y = 0; y + 4 <= TILE; y += 5)
    for (x = ((y / 5) & 1) ? 4 : 0; x + 5 <= TILE; x += 8) {
      tile_box(tall, x,     y + 2, x + 1, y + 4, RGB15(8, 22, 8));
      tile_box(tall, x + 2, y,     x + 3, y + 4, RGB15(12, 28, 11));
      tile_box(tall, x + 4, y + 2, x + 5, y + 4, RGB15(8, 22, 8));
    }
  tile_box(tall, 0, 15, TILE, TILE, RGB15(2, 10, 3));

  // Sign: a wooden board on a post, standing on grass.
  copy_tile(sign, tile_gfx[RT_GRASS]);
  tile_box(sign, 7, 8, 9, 15, RGB15(14, 8, 4));
  tile_box(sign, 2, 2, 14, 9, RGB15(14, 8, 4));
  tile_box(sign, 3, 3, 13, 8, RGB15(27, 19, 10));
  tile_box(sign, 4, 4, 12, 5, RGB15(14, 8, 4));
  tile_box(sign, 4, 6, 9, 7, RGB15(14, 8, 4));
}

void build_sprites(void) {
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
// END OF tiles.c
