/ ============================================================
//  My GBA game - Step 3: a little Pokemon-style town
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
// 16x16 sprite: the trainer is blocked when this box touches a solid tile.
#define HB_LEFT   2
#define HB_RIGHT  13
#define HB_TOP    1
#define HB_BOTTOM 15

// A special value that means "see-through pixel".
// Real colors never use the top bit, so this can't clash with them.
#define CLEAR 0x8000

// Colors used by the building art
#define OUTLINE RGB15(2, 2, 5)
#define WHITE   RGB15(31, 31, 31)

// Directions the trainer can face
enum { DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT };

// Kinds of map tiles.
// Everything from T_TREE onwards is SOLID (the trainer can't walk on it).
// Keep T_TREE as the first solid one: is_blocked() relies on that order.
enum {
  T_GRASS, T_PATH, T_FLOWER,          // walkable
  T_TREE,                             // solid from here on
  T_ROOF_H1, T_ROOF_H2,               // roofs of the two houses
  T_WALL_H,  T_DOOR_H,                // house wall (with window) and door
  T_ROOF_C,  T_SIGN_C, T_WALL_C, T_DOOR_C,   // Pokemon Center
  T_ROOF_M,  T_SIGN_M, T_WALL_M, T_DOOR_M,   // Poke Mart
  NUM_TILES
};

// ---------- The map ----------
// Walkable:
//   G grass   P path   F flowers
// Solid:
//   T tree
//   House:   H roof (orange)  J roof (purple)  h wall+window  D door
//   Center:  R roof (red)     r roof with "P" sign  C wall  c door
//   Mart:    U roof (blue)    u roof with "M" sign  M wall  m door
// Each row is 15 letters, and there are 10 rows. Edit it and have fun!
// Keep the top row all trees: the test lamps are drawn there.
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

// Little 5x7 letters for the shop signs. X = painted pixel.
static const char glyph_p[7][6] = {
"XXXX.",
"X...X",
"X...X",
"XXXX.",
"X....",
"X....",
"X...."
};

static const char glyph_m[7][6] = {
"X...X",
"XX.XX",
"X.X.X",
"X...X",
"X...X",
"X...X",
"X...X"
};

// ---------- Memory for ready-made pictures ----------
// We build each picture once at the start, then just copy from these
// boxes while the game runs (fast!). 16 tiles * 512 bytes = 8 KB.
static unsigned short tile_gfx[NUM_TILES][TILE * TILE];

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
      int kind;
      switch (map_rows[ty][tx]) {
        case 'P': kind = T_PATH;    break;
        case 'F': kind = T_FLOWER;  break;
        case 'T': kind = T_TREE;    break;
        case 'H': kind = T_ROOF_H1; break;
        case 'J': kind = T_ROOF_H2; break;
        case 'h': kind = T_WALL_H;  break;
        case 'D': kind = T_DOOR_H;  break;
        case 'R': kind = T_ROOF_C;  break;
        case 'r': kind = T_SIGN_C;  break;
        case 'C': kind = T_WALL_C;  break;
        case 'c': kind = T_DOOR_C;  break;
        case 'U': kind = T_ROOF_M;  break;
        case 'u': kind = T_SIGN_M;  break;
        case 'M': kind = T_WALL_M;  break;
        case 'm': kind = T_DOOR_M;  break;
        default:  kind = T_GRASS;   break;
      }
      map_kind[ty][tx] = (unsigned char)kind;
    }
  }
}

// Fill a rectangle inside one tile (x1/y1 are NOT included).
static void tile_box(unsigned short* t, int x0, int y0, int x1, int y1,
                     unsigned short c) {
  int x, y;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      t[y * TILE + x] = c;
}

// A roof tile: staggered shingles with a dark outline on top and bottom.
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

// A white sign plaque with a big letter, painted on top of a roof tile.
static void add_sign(unsigned short* t, const char glyph[7][6],
                     unsigned short color) {
  int gx, gy;
  tile_box(t, 2, 2, 14, 14, OUTLINE);
  tile_box(t, 3, 3, 13, 13, WHITE);
  for (gy = 0; gy < 7; gy++)
    for (gx = 0; gx < 5; gx++)
      if (glyph[gy][gx] == 'X')
        t[(5 + gy) * TILE + (6 + gx)] = color;
}

// A wall tile: plain siding with a darker strip along the bottom.
static void make_wall(unsigned short* t, unsigned short wall,
                      unsigned short shade) {
  int x, y;
  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      unsigned short c = wall;
      if ((y & 3) == 3) c = shade;
      if (y >= 14)      c = shade;
      t[y * TILE + x] = c;
    }
  }
}

// A window with a cross-shaped frame.
static void add_window(unsigned short* t, unsigned short frame) {
  unsigned short glass = RGB15(14, 24, 31);
  tile_box(t, 3, 2, 13, 12, frame);
  tile_box(t, 4, 3, 12, 11, glass);
  tile_box(t, 7, 3, 9, 11, frame);
  tile_box(t, 4, 6, 12, 8, frame);
  t[4 * TILE + 5] = WHITE;     // little shine in the top-left pane
  t[4 * TILE + 10] = WHITE;
}

// A door with a frame and a golden knob.
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
      tile_gfx[T_TREE][i] = (c == CLEAR) ? tile_gfx[T_GRASS][i] : c;
    }
  }

  // ----- Houses: cream walls, brown doors, orange and purple roofs -----
  make_roof(tile_gfx[T_ROOF_H1], RGB15(24, 10, 6), RGB15(28, 14, 9),
            RGB15(16, 6, 4));
  make_roof(tile_gfx[T_ROOF_H2], RGB15(18, 10, 22), RGB15(22, 14, 26),
            RGB15(12, 6, 15));

  make_wall(tile_gfx[T_WALL_H], RGB15(30, 26, 18), RGB15(25, 21, 13));
  add_window(tile_gfx[T_WALL_H], RGB15(10, 6, 3));

  make_wall(tile_gfx[T_DOOR_H], RGB15(30, 26, 18), RGB15(25, 21, 13));
  add_door(tile_gfx[T_DOOR_H], RGB15(17, 10, 4), RGB15(9, 5, 2));

  // ----- Pokemon Center: red roof, "P" sign, pink-white walls -----
  make_roof(tile_gfx[T_ROOF_C], RGB15(29, 5, 5), RGB15(31, 10, 9),
            RGB15(20, 2, 3));
  make_roof(tile_gfx[T_SIGN_C], RGB15(29, 5, 5), RGB15(31, 10, 9),
            RGB15(20, 2, 3));
  add_sign(tile_gfx[T_SIGN_C], glyph_p, RGB15(29, 3, 3));

  make_wall(tile_gfx[T_WALL_C], RGB15(31, 27, 26), RGB15(27, 22, 22));
  add_window(tile_gfx[T_WALL_C], RGB15(12, 12, 14));

  make_wall(tile_gfx[T_DOOR_C], RGB15(31, 27, 26), RGB15(27, 22, 22));
  add_door(tile_gfx[T_DOOR_C], RGB15(18, 26, 30), RGB15(12, 12, 14));
  tile_box(tile_gfx[T_DOOR_C], 7, 2, 9, TILE, RGB15(12, 12, 14)); // sliding doors

  // ----- Poke Mart: blue roof, "M" sign, light blue-grey walls -----
  make_roof(tile_gfx[T_ROOF_M], RGB15(4, 10, 26), RGB15(8, 15, 30),
            RGB15(2, 5, 16));
  make_roof(tile_gfx[T_SIGN_M], RGB15(4, 10, 26), RGB15(8, 15, 30),
            RGB15(2, 5, 16));
  add_sign(tile_gfx[T_SIGN_M], glyph_m, RGB15(4, 10, 26));

  make_wall(tile_gfx[T_WALL_M], RGB15(24, 28, 31), RGB15(19, 23, 28));
  add_window(tile_gfx[T_WALL_M], RGB15(4, 6, 14));

  make_wall(tile_gfx[T_DOOR_M], RGB15(24, 28, 31), RGB15(19, 23, 28));
  add_door(tile_gfx[T_DOOR_M], RGB15(10, 18, 28), RGB15(4, 6, 14));
  tile_box(tile_gfx[T_DOOR_M], 7, 2, 9, TILE, RGB15(4, 6, 14));
}

static void build_sprites(void) {
  int x, y;
  int dir;

  for (y = 0; y < TILE; y++) {
    for (x = 0; x < TILE; x++) {
      int i = y * TILE + x;

      sprite_gfx[DIR_DOWN][0][i]  = letter_color(art_down[y][x]);
      sprite_gfx[DIR_UP][0][i]    = letter_color(art_up[y][x]);
      sprite_gfx[DIR_LEFT][0][i]  = letter_color(art_left[y][x]);
      sprite_gfx[DIR_RIGHT][0][i] = letter_color(art_left[y][TILE - 1 - x]);
    }
  }

  // Create the second walking frame.
  for (dir = 0; dir < 4; dir++) {
    for (y = 0; y < TILE; y++) {
      for (x = 0; x < TILE; x++) {
        int i = y * TILE + x;
        int source_x = x;

        // Move the two feet apart slightly in the alternate frame.
        if (y >= 14) {
          if (x < 8)
            source_x = x + 1;
          else
            source_x = x - 1;
        }

        if (source_x >= 0 && source_x < TILE) {
          sprite_gfx[dir][1][i] = sprite_gfx[dir][0][y * TILE + source_x];
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
// Is the pixel (px, py) blocked? Trees, buildings (every tile from T_TREE
// onwards) and the screen edge block you.
static int is_blocked(int px, int py) {
  if (px < 0 || py < 0 || px >= SCREEN_W || py >= SCREEN_H) return 1;
  return map_kind[py >> 4][px >> 4] >= T_TREE;
}

// Can the trainer stand with its top-left corner at (px, py)?
// The body box is smaller than a tile, so checking its 4 corners is enough
// to know whether it touches anything solid.
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
// Lamp 6 lights up   = a direction is pressed but something solid blocked it
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
    // Left/ri
