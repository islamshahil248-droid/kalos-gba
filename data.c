// data.c - all the fixed data: maps, rooms, doors, people and their text,
// trainer/tree art, the font, and the shared game memory.
#include "game.h"

// ---------- Town ----------
// G grass, P path, F flowers, T tree. House: H/J roof, h wall, D door.
// Center: R roof, r sign, C wall, c door. Mart: U roof, u sign, M wall, m door.
// n and N are townspeople (they block your way like trees do).
static const char town_rows[MAP_H][MAP_W + 1] = {
"TTTTTTTTTTTTTTT",
"THHHGGRrRGGUuUT",
"ThDhGFCcCFGMmMT",
"TGPPPPPPPPPPPGT",
"TGFGGGPGGGGFGGT",
"TGTGGnPGGJJJNTT",
"TGGGFGPGGhDhGGT",
"TGTGGGPPPPPGGTT",
"TGGGFGPGGGGGFGT",
"TTTTTTPTTTTTTTT"
};
static const char town_keys[] = "GPFTHJhDRrCcUuMmnN";
static const unsigned char town_kinds[] = {
  TW_GRASS, TW_PATH, TW_FLOWER, TW_TREE, TW_ROOF_H1, TW_ROOF_H2, TW_WALL_H,
  TW_DOOR_H, TW_ROOF_C, TW_SIGN_C, TW_WALL_C, TW_DOOR_C, TW_ROOF_M,
  TW_SIGN_M, TW_WALL_M, TW_DOOR_M, TW_NPC_A, TW_NPC_B
};

// ---------- Interiors ----------
// Common: . floor, W wall, S wall+sign, w wall+window, c exit door, _ rug/mat,
// p plant, n NPC. Row 0 and the side walls are always solid (HUD lamps there).
// Objects drawn on the floor (p, n, h) must sit where (column+row) is EVEN.

// Pokemon Center: K/k counter top/front, M/N healing machine top/bottom,
// n nurse behind the counter.
static const char center_rows[MAP_H][MAP_W + 1] = {
"WWWWWWWSWWWWWWW",
"Wp.........MM.W",
"W.....n....NN.W",
"W..KKKKKKK....W",
"W..kkkkkkk....W",
"W.............W",
"W...........p.W",
"W.............W",
"W.....___.....W",
"WWWWWWWcWWWWWWW"
};
static const char center_keys[] = ".pWSKkMN_cn";
static const unsigned char center_kinds[] = {
  IC_FA, IC_PLANT, IC_WALL, IC_SIGN, IC_CNT, IC_CNTF, IC_MCH_T, IC_MCH_B,
  IC_MAT, IC_DOOR, IC_NPC
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
const Door doors[] = {
  { R_TOWN,   7, 2, R_CENTER, 7, 8, DIR_UP   },
  { R_CENTER, 7, 9, R_TOWN,   7, 3, DIR_DOWN },
  { R_TOWN,  12, 2, R_MART,   7, 8, DIR_UP   },
  { R_MART,   7, 9, R_TOWN,  12, 3, DIR_DOWN },
  { R_TOWN,   2, 2, R_HOUSE1, 7, 8, DIR_UP   },
  { R_HOUSE1, 7, 9, R_TOWN,   2, 3, DIR_DOWN },
  { R_TOWN,  10, 6, R_HOUSE2, 3, 8, DIR_UP   },
  { R_HOUSE2, 3, 9, R_TOWN,  10, 7, DIR_DOWN }
};
// How many doors there are (other files read this number).
const int num_doors = (int)(sizeof(doors) / sizeof(doors[0]));

// ---------- People ----------
// Each person is a solid tile on the map (so nobody can walk through them).
// This table says who stands where and what they say.
// Text: '|' starts a new page; long sentences wrap by themselves.
// Supported: letters, digits and . , ! ? ' " - : ( )
const Npc npcs[] = {
  { R_TOWN,    5, 5, 1, "The tall grass can hide wild Pokemon!" },
  { R_TOWN,   12, 5, 1, "The POKEMON CENTER on this street heals your team for free." },
  { R_CENTER,  6, 2, 3, "Welcome to the POKEMON CENTER!|We heal your POKEMON back to perfect health.|We hope to see you again!" },
  { R_MART,    6, 2, 3, "Hello! Take a look at our supplies.|Come back any time!" },
  { R_HOUSE1, 11, 3, 1, "Have you been exploring the town?|Be careful out there, and come home soon!" },
  { R_HOUSE2, 10, 6, 1, "Every great journey begins somewhere.|Take your time and enjoy the road." }
};
// How many people there are (other files read this number).
const int num_npcs = (int)(sizeof(npcs) / sizeof(npcs[0]));
// ---------- Trainer art ----------
// Sprite colors: K outline, B cap, S skin, H hair, Y shirt, J jeans, R shoes
const char art_down[16][17] = {
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
const char art_up[16][17] = {
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
const char art_left[16][17] = {
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
const char art_tree[16][17] = {
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
const char glyph_p[] = "XXXX.X...XX...XXXXX.X....X....X....";
const char glyph_m[] = "X...XXX.XXX.X.XX...XX...XX...XX...X";
// ---------- Memory (about 14 KB in total) ----------
unsigned short tile_gfx[MAX_TILES][TILE * TILE];   // current room only
unsigned short sprite_gfx[4][2][TILE * TILE];
unsigned char map_kind[NUM_ROOMS][MAP_H][MAP_W];
int cur_room = R_TOWN;
int walk_frame = 0;
int walk_timer = 0;
const Room rooms[NUM_ROOMS] = {
  { town_rows,   town_keys,   town_kinds,   TW_TREE,     0, build_town   },
  { center_rows, center_keys, center_kinds, SOLID_INSIDE, 1, build_center },
  { mart_rows,   mart_keys,   mart_kinds,   SOLID_INSIDE, 1, build_mart   },
  { house1_rows, house1_keys, house1_kinds, SOLID_INSIDE, 1, build_house1 },
  { house2_rows, house2_keys, house2_kinds, SOLID_INSIDE, 1, build_house2 }
};
// 5x8 pixel letters for ASCII 32 (space) to 122 (z). Each letter is 5
// columns; in a column, bit 0 is the top pixel. Unused symbols are blank.
const unsigned char font5x8[91][5] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* space */
  { 0x00, 0x00, 0x5F, 0x00, 0x00 },  /* punctuation 0x21 */
  { 0x00, 0x07, 0x00, 0x07, 0x00 },  /* punctuation 0x22 */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x05, 0x03, 0x00, 0x00 },  /* punctuation 0x27 */
  { 0x00, 0x1C, 0x22, 0x41, 0x00 },  /* punctuation 0x28 */
  { 0x00, 0x41, 0x22, 0x1C, 0x00 },  /* punctuation 0x29 */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x50, 0x30, 0x00, 0x00 },  /* punctuation 0x2C */
  { 0x08, 0x08, 0x08, 0x08, 0x08 },  /* punctuation 0x2D */
  { 0x00, 0x60, 0x60, 0x00, 0x00 },  /* punctuation 0x2E */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x3E, 0x51, 0x49, 0x45, 0x3E },  /* 0 */
  { 0x00, 0x42, 0x7F, 0x40, 0x00 },  /* 1 */
  { 0x42, 0x61, 0x51, 0x49, 0x46 },  /* 2 */
  { 0x21, 0x41, 0x45, 0x4B, 0x31 },  /* 3 */
  { 0x18, 0x14, 0x12, 0x7F, 0x10 },  /* 4 */
  { 0x27, 0x45, 0x45, 0x45, 0x39 },  /* 5 */
  { 0x3C, 0x4A, 0x49, 0x49, 0x30 },  /* 6 */
  { 0x01, 0x71, 0x09, 0x05, 0x03 },  /* 7 */
  { 0x36, 0x49, 0x49, 0x49, 0x36 },  /* 8 */
  { 0x06, 0x49, 0x49, 0x29, 0x1E },  /* 9 */
  { 0x00, 0x36, 0x36, 0x00, 0x00 },  /* punctuation 0x3A */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x02, 0x01, 0x51, 0x09, 0x06 },  /* punctuation 0x3F */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x7E, 0x11, 0x11, 0x11, 0x7E },  /* A */
  { 0x7F, 0x49, 0x49, 0x49, 0x36 },  /* B */
  { 0x3E, 0x41, 0x41, 0x41, 0x22 },  /* C */
  { 0x7F, 0x41, 0x41, 0x22, 0x1C },  /* D */
  { 0x7F, 0x49, 0x49, 0x49, 0x41 },  /* E */
  { 0x7F, 0x09, 0x09, 0x09, 0x01 },  /* F */
  { 0x3E, 0x41, 0x41, 0x51, 0x32 },  /* G */
  { 0x7F, 0x08, 0x08, 0x08, 0x7F },  /* H */
  { 0x00, 0x41, 0x7F, 0x41, 0x00 },  /* I */
  { 0x20, 0x40, 0x41, 0x3F, 0x01 },  /* J */
  { 0x7F, 0x08, 0x14, 0x22, 0x41 },  /* K */
  { 0x7F, 0x40, 0x40, 0x40, 0x40 },  /* L */
  { 0x7F, 0x02, 0x0C, 0x02, 0x7F },  /* M */
  { 0x7F, 0x04, 0x08, 0x10, 0x7F },  /* N */
  { 0x3E, 0x41, 0x41, 0x41, 0x3E },  /* O */
  { 0x7F, 0x09, 0x09, 0x09, 0x06 },  /* P */
  { 0x3E, 0x41, 0x51, 0x21, 0x5E },  /* Q */
  { 0x7F, 0x09, 0x19, 0x29, 0x46 },  /* R */
  { 0x46, 0x49, 0x49, 0x49, 0x31 },  /* S */
  { 0x01, 0x01, 0x7F, 0x01, 0x01 },  /* T */
  { 0x3F, 0x40, 0x40, 0x40, 0x3F },  /* U */
  { 0x1F, 0x20, 0x40, 0x20, 0x1F },  /* V */
  { 0x3F, 0x40, 0x38, 0x40, 0x3F },  /* W */
  { 0x63, 0x14, 0x08, 0x14, 0x63 },  /* X */
  { 0x07, 0x08, 0x70, 0x08, 0x07 },  /* Y */
  { 0x61, 0x51, 0x49, 0x45, 0x43 },  /* Z */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x00, 0x00, 0x00, 0x00, 0x00 },  /* unused */
  { 0x20, 0x54, 0x54, 0x54, 0x78 },  /* a */
  { 0x7F, 0x48, 0x44, 0x44, 0x38 },  /* b */
  { 0x38, 0x44, 0x44, 0x44, 0x20 },  /* c */
  { 0x38, 0x44, 0x44, 0x48, 0x7F },  /* d */
  { 0x38, 0x54, 0x54, 0x54, 0x18 },  /* e */
  { 0x08, 0x7E, 0x09, 0x01, 0x02 },  /* f */
  { 0x18, 0xA4, 0xA4, 0xA4, 0x78 },  /* g */
  { 0x7F, 0x08, 0x04, 0x04, 0x78 },  /* h */
  { 0x00, 0x44, 0x7D, 0x40, 0x00 },  /* i */
  { 0x00, 0x80, 0x80, 0xFD, 0x00 },  /* j */
  { 0x7F, 0x10, 0x28, 0x44, 0x00 },  /* k */
  { 0x00, 0x41, 0x7F, 0x40, 0x00 },  /* l */
  { 0x7C, 0x04, 0x18, 0x04, 0x78 },  /* m */
  { 0x7C, 0x08, 0x04, 0x04, 0x78 },  /* n */
  { 0x38, 0x44, 0x44, 0x44, 0x38 },  /* o */
  { 0xFC, 0x24, 0x24, 0x24, 0x18 },  /* p */
  { 0x18, 0x24, 0x24, 0x24, 0xFC },  /* q */
  { 0x7C, 0x08, 0x04, 0x04, 0x08 },  /* r */
  { 0x48, 0x54, 0x54, 0x54, 0x20 },  /* s */
  { 0x04, 0x3F, 0x44, 0x40, 0x20 },  /* t */
  { 0x3C, 0x40, 0x40, 0x20, 0x7C },  /* u */
  { 0x1C, 0x20, 0x40, 0x20, 0x1C },  /* v */
  { 0x3C, 0x40, 0x30, 0x40, 0x3C },  /* w */
  { 0x44, 0x28, 0x10, 0x28, 0x44 },  /* x */
  { 0x1C, 0xA0, 0xA0, 0xA0, 0x7C },  /* y */
  { 0x44, 0x64, 0x54, 0x4C, 0x44 },  /* z */
};
// END OF data.c
