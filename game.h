// game.h - shared definitions across all modules including game states,
// menu structures, party data, and rendering functions.
#ifndef GAME_H
#define GAME_H
#define REG_DISPCNT  ((volatile unsigned short)0x04000000)
#define REG_VCOUNT   ((volatile unsigned short)0x04000006)
#define REG_KEYINPUT ((volatile unsigned short)0x04000130)
#define VRAM ((volatile unsigned short*)0x06000000)
#define RGB15(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define DEBUG_HUD 0
#define SCREEN_W 240
#define SCREEN_H 160
#define TILE     16
#define MAP_W    15
#define MAP_H    10
#define SPEED    2
#define START_TX 6
#define START_TY 8
#define HB_LEFT   2
#define HB_RIGHT  13
#define HB_TOP    1
#define HB_BOTTOM 15
#define CLEAR   0x8000
#define OUTLINE RGB15(2, 2, 5)
#define WHITE   RGB15(31, 31, 31)
#define BLACK   RGB15(0, 0, 0)
#define GRAY    RGB15(15, 15, 15)
#define LIGHT_GRAY RGB15(24, 24, 24)
#define RED     RGB15(31, 5, 5)
#define GREEN   RGB15(5, 28, 5)
#define BLUE    RGB15(5, 12, 30)
#define DIR_DOWN  0
#define DIR_UP    1
#define DIR_LEFT  2
#define DIR_RIGHT 3
// Game Engine Overall States
enum {
GM_WORLD,
GM_BATTLE,
GM_MAIN_MENU,
GM_PARTY_MENU,
GM_SUB_MENU
};
// Main Menu Options
enum {
MM_POKEDEX,
MM_POKEMON,
MM_BAG,
MM_TRAINER,
MM_SAVE,
MM_OPTIONS,
MM_CLOSE,
MM_COUNT
};
// Rooms
enum { R_TOWN, R_CENTER, R_MART, R_HOUSE1, R_HOUSE2, R_ROUTE1, NUM_ROOMS };
// Town tiles
enum {
TW_GRASS, TW_PATH, TW_FLOWER, TW_TREE,
TW_ROOF_H1, TW_ROOF_H2, TW_WALL_H, TW_DOOR_H,
TW_ROOF_C, TW_SIGN_C, TW_WALL_C, TW_DOOR_C,
TW_ROOF_M, TW_SIGN_M, TW_WALL_M, TW_DOOR_M,
TW_NPC_A, TW_NPC_B,
MAX_TILES
};
enum { IC_FA, IC_FB, IC_MAT, IC_WALL, IC_SIGN, IC_CNT, IC_CNTF, IC_MCH_T, IC_MCH_B, IC_PLANT, IC_DOOR, IC_NPC };
enum { IM_FA, IM_FB, IM_MAT, IM_WALL, IM_SIGN, IM_DOOR, IM_CNT, IM_CNTF, IM_SHELF_A, IM_SHELF_B, IM_NPC, IM_PLANT };
enum { H1_FA, H1_FB, H1_RUG, H1_WALL, H1_WIN, H1_DOOR, H1_BED_H, H1_BED_F, H1_TABLE, H1_CHAIR, H1_PLANT, H1_NPC };
enum { H2_FA, H2_FB, H2_RUG, H2_WALL, H2_WIN, H2_DOOR, H2_SHELF_A, H2_SHELF_B, H2_BED_H, H2_BED_F, H2_TABLE, H2_CHAIR, H2_PLANT, H2_NPC };
#define SOLID_INSIDE 3
enum { RT_GRASS, RT_PATH, RT_FLOWER, RT_TALL, RT_TREE, RT_SIGN };
#define NO_TILE 255
enum { SP_PIDGEY, SP_RATTATA, SP_CATERPIE, SP_PIKACHU, NUM_SPECIES };
#define ENC_COOLDOWN_STEPS 3
// Moves and Party
enum { MV_THUNDER_SHOCK, MV_QUICK_ATTACK, MV_TACKLE, MV_GUST, NUM_MOVES };
#define MAX_MOVES 4
#define PARTY_MAX 6
#define MAX_LEVEL 40
#define START_BALLS 5
// Battle state
enum { MENU_FIGHT, MENU_POKEMON_SWAP, MENU_CATCH, MENU_RUN };
#define BT_LINE 36
enum { BS_MESSAGE, BS_MENU, BS_MOVES };
enum {
AFTER_MENU, AFTER_ENEMY, AFTER_WIN, AFTER_LOSE,
AFTER_END_WON, AFTER_END_RAN, AFTER_END_LOST,
AFTER_LEVELUP, AFTER_CATCH, AFTER_END_CAUGHT,
AFTER_MUST_SWITCH, AFTER_SWITCH_TURN
};
enum { BATTLE_GOING, BATTLE_WON, BATTLE_RAN, BATTLE_LOST, BATTLE_CAUGHT };
#define BT_HUD 1
#define BT_BOX 2
#define BLACKOUT_ROOM R_CENTER
#define BLACKOUT_TX   7
#define BLACKOUT_TY   8
typedef struct {
const char (*rows)[MAP_W + 1];
const char *keys;
const unsigned char* kinds;
unsigned char solid_from;
unsigned char checker;
unsigned char tall;
void (*build)(void);
} Room;
typedef struct {
unsigned char from, tx, ty;
unsigned char to, sx, sy, sdir;
} Door;
typedef struct {
unsigned char room, tx, ty;
unsigned char reach;
const char* text;
unsigned char heals;
} Npc;
typedef struct {
const char* name;
unsigned short body, accent;
const char* art;
unsigned char hp_base;
unsigned char move;
} Species;
typedef struct {
const char* name;
unsigned char power;
} Move;
typedef struct {
unsigned char species;
unsigned char level;
int hp, max_hp;
unsigned short exp;
unsigned char num_moves;
unsigned char move[MAX_MOVES];
} Mon;
typedef struct {
unsigned char species;
unsigned char weight;
} WildEntry;
typedef struct {
unsigned char room;
unsigned char percent;
unsigned char count;
const WildEntry* entries;
} Zone;
#define NUM_DOORS num_doors
#define NUM_NPCS  num_npcs
#define NUM_WARPS num_warps
#define NUM_ZONES num_zones
// Shared Declarations
extern const int num_doors;
extern const int num_npcs;
extern const int num_warps;
extern const int num_zones;
extern const Door doors[];
extern const Door warps[];
extern const Species species[NUM_SPECIES];
extern const Zone zones[];
extern const Npc npcs[];
extern const Move moves[NUM_MOVES];
extern const Room rooms[NUM_ROOMS];
extern const char art_down[16][17];
extern const char art_up[16][17];
extern const char art_left[16][17];
extern const char art_tree[16][17];
extern const char glyph_p[];
extern const char glyph_m[];
extern const unsigned char font5x8[91][5];
extern unsigned short tile_gfx[MAX_TILES][TILE * TILE];
extern unsigned short sprite_gfx[4][2][TILE * TILE];
extern unsigned char map_kind[NUM_ROOMS][MAP_H][MAP_W];
extern int cur_room;
extern int walk_frame;
extern int walk_timer;
extern int wild_species;
extern Mon party[PARTY_MAX];
extern int party_count;
extern int active_mon_idx;
#define player_mon (party[active_mon_idx])
extern int pokeballs;
extern Mon wild_mon;
extern int bt_state;
extern int bt_cursor;
extern int bt_after;
extern int bt_redraw;
extern char bt_line1[BT_LINE];
extern char bt_line2[BT_LINE];
// Game Management States
extern int game_state;
extern int main_menu_cursor;
extern int party_menu_cursor;
extern int sub_menu_type;
// Functions
void build_town(void);
void build_center(void);
void build_mart(void);
void build_house1(void);
void build_house2(void);
void build_route1(void);
void build_sprites(void);
void build_map(int m);
int can_stand(int px, int py);
int front_door(int x, int y, int dir);
int front_npc(int x, int y, int dir);
void pick_start(int stx, int sty, int* out_x, int* out_y);
int warp_at_feet(int x, int y);
void steps_reset(int x, int y);
int step_encounter(int x, int y);
void heal_party(void);
int has_alive_mon(void);
int exp_to_next(int level);
int party_add(const Mon* m);
int switch_active_mon(int new_idx);
void rng_stir(unsigned int v);
void battle_begin(int sp);
int battle_input(unsigned short pressed);
void str_set(char* dst, int size, const char* s);
void str_add(char* dst, int size, const char* s);
void str_add_num(char* dst, int size, int n);
// Rendering & UI
void paint_box(int left, int top, int right, int bottom, int px, int py, int dir);
void wait_vblank(void);
void show_map(int x, int y, int dir);
void dialog_open(int n);
void dialog_advance(int x, int y, int dir);
void battle_screen_open(void);
void battle_redraw(void);
void battle_close(void);
void render_main_menu(void);
void render_party_screen(int is_battle);
void render_sub_screen(int type);
void draw_text(int x, int y, const char* text, unsigned short color);
void draw_fill_rect(int x1, int y1, int x2, int y2, unsigned short color);
extern int dlg_open;
extern int battle_open;
#if DEBUG_HUD
void lamp(int slot, int on, unsigned short color);
void show_lamps(unsigned int frame, unsigned short keys, int moved, int blocked);
#endif
#endif
