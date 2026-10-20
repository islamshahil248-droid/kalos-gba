// GBA town with 4 enterable buildings, NPCs, dialogue, Route 1 and
// wild POKEMON encounters.
// Mode 3, 240x160, devkitARM, no libraries.
//
// game.h - shared by every .c file: constants, types, and the list of what
// the files share. (Rule: if a .c file needs something from another .c file,
// it is declared here.)
#ifndef GAME_H
#define GAME_H

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
enum { R_TOWN, R_CENTER, R_MART, R_HOUSE1, R_HOUSE2, R_ROUTE1, NUM_ROOMS };

// Town tiles (3 and up are SOLID). MAX_TILES = biggest tile set of any room.
enum {
  TW_GRASS, TW_PATH, TW_FLOWER, TW_TREE,
  TW_ROOF_H1, TW_ROOF_H2, TW_WALL_H, TW_DOOR_H,
  TW_ROOF_C, TW_SIGN_C, TW_WALL_C, TW_DOOR_C,
  TW_ROOF_M, TW_SIGN_M, TW_WALL_M, TW_DOOR_M,
  TW_NPC_A, TW_NPC_B,                 // townspeople
  MAX_TILES
};
// Inside tiles: 0,1 = checker floor, 2 = rug/mat (walkable); 3 and up SOLID.
enum { IC_FA, IC_FB, IC_MAT, IC_WALL, IC_SIGN, IC_CNT, IC_CNTF, IC_MCH_T,
       IC_MCH_B, IC_PLANT, IC_DOOR, IC_NPC };
enum { IM_FA, IM_FB, IM_MAT, IM_WALL, IM_SIGN, IM_DOOR, IM_CNT, IM_CNTF,
       IM_SHELF_A, IM_SHELF_B, IM_NPC, IM_PLANT };
enum { H1_FA, H1_FB, H1_RUG, H1_WALL, H1_WIN, H1_DOOR, H1_BED_H, H1_BED_F,
       H1_TABLE, H1_CHAIR, H1_PLANT, H1_NPC };
enum { H2_FA, H2_FB, H2_RUG, H2_WALL, H2_WIN, H2_DOOR, H2_SHELF_A, H2_SHELF_B,
       H2_BED_H, H2_BED_F, H2_TABLE, H2_CHAIR, H2_PLANT, H2_NPC };
#define SOLID_INSIDE 3
// Route 1 tiles. 0-2 are the SAME pictures as the town's grass, path and
// flowers (build_route1 calls build_town first). 3 = tall grass: you can walk
// on it and wild POKEMON can appear there. 4 and up are SOLID.
enum { RT_GRASS, RT_PATH, RT_FLOWER, RT_TALL, RT_TREE, RT_SIGN };

#define NO_TILE 255           // "this room has no such tile"

// Wild POKEMON. Species numbers (index into species[] in data.c).
enum { SP_PIDGEY, SP_RATTATA, SP_CATERPIE, NUM_SPECIES };

// After an encounter, this many tall-grass steps are safe (no new encounter).
// The chance per step lives in zones[] in data.c.
#define ENC_COOLDOWN_STEPS 3

typedef struct {
  const char (*rows)[MAP_W + 1];     // the map letters
  const char* keys;                  // letters we know ...
  const unsigned char* kinds;        // ... and the tile each one means
  unsigned char solid_from;          // tile numbers >= this block you
  unsigned char checker;             // 1: '.' alternates tile 0 / tile 1
  unsigned char tall;                // tile number of tall grass (NO_TILE = none)
  void (*build)(void);               // paints this room's tile pictures
} Room;

typedef struct {                     // one door: where it is, where it leads
  unsigned char from, tx, ty;        // room and tile of the door
  unsigned char to, sx, sy, sdir;    // room, tile and facing after using it
} Door;

typedef struct {                     // one person you can talk to
  unsigned char room, tx, ty;        // room and tile where they stand
  unsigned char reach;               // 1 = must be right in front of you,
                                     // 3 = may stand behind a counter
  const char* text;                  // what they say (see "People" below)
} Npc;

typedef struct {                     // one kind of wild POKEMON
  const char* name;
  unsigned short body, accent;       // colors of the 'B' and 'A' pixels below
  const char* art;                   // placeholder picture: 8x8 = 64 letters
} Species;

typedef struct {                     // one line of an encounter table
  unsigned char species;             // SP_...
  unsigned char weight;              // bigger = more common
} WildEntry;

typedef struct {                     // the wild POKEMON of one room
  unsigned char room;
  unsigned char percent;             // chance (0-100) per NEW tall-grass tile
  unsigned char count;               // how many entries
  const WildEntry* entries;
} Zone;

// Counts come from data.c (so adding a door or person needs no edit here).
// "Warps" are exits you trigger by WALKING onto the tile (same type as Door).
#define NUM_DOORS num_doors
#define NUM_NPCS  num_npcs
#define NUM_WARPS num_warps
#define NUM_ZONES num_zones

// ===== Shared between files =====
// data.c: tables (rooms, doors, people)
extern const int num_doors;
extern const int num_npcs;
extern const int num_warps;
extern const int num_zones;
extern const Door doors[];
extern const Door warps[];
extern const Species species[NUM_SPECIES];
extern const Zone zones[];
extern const Npc npcs[];
extern const Room rooms[NUM_ROOMS];

// data.c: pixel art and font
extern const char art_down[16][17];
extern const char art_up[16][17];
extern const char art_left[16][17];
extern const char art_tree[16][17];
extern const char glyph_p[];
extern const char glyph_m[];
extern const unsigned char font5x8[91][5];

// data.c: shared game memory
extern unsigned short tile_gfx[MAX_TILES][TILE * TILE];
extern unsigned short sprite_gfx[4][2][TILE * TILE];
extern unsigned char map_kind[NUM_ROOMS][MAP_H][MAP_W];
extern int cur_room;
extern int walk_frame;
extern int walk_timer;
extern int wild_species;   // species of the current wild encounter (SP_...), -1 = none

// tiles.c: paint the pictures
void build_town(void);
void build_center(void);
void build_mart(void);
void build_house1(void);
void build_house2(void);
void build_route1(void);
void build_sprites(void);

// world.c: map, collision, doors, people
void build_map(int m);
int can_stand(int px, int py);
int front_door(int x, int y, int dir);
int front_npc(int x, int y, int dir);
void pick_start(int stx, int sty, int* out_x, int* out_y);
int warp_at_feet(int x, int y);        // warp the trainer is standing on, or -1
void steps_reset(int x, int y);        // call when arriving in a room
int step_encounter(int x, int y);      // new tall-grass step: species or -1
void rng_stir(unsigned int v);         // mix in a number (call every frame)

// render.c: drawing and dialogue
void paint_box(int left, int top, int right, int bottom, int px, int py, int dir);
void wait_vblank(void);
void show_map(int x, int y, int dir);
void dialog_open(int n);
void dialog_advance(int x, int y, int dir);
void encounter_start(int sp);                    // show the wild encounter screen
void encounter_advance(int x, int y, int dir);   // A pressed on that screen

extern int dlg_open;   // render.c: 1 while a dialogue box is on screen
extern int enc_open;   // render.c: 1 while the wild encounter screen is showing

#if DEBUG_HUD
void lamp(int slot, int on, unsigned short color);
void show_lamps(unsigned int frame, unsigned short keys, int moved, int blocked);
#endif

#endif
// END OF game.h
