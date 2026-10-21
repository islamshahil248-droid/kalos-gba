// render.c - screen drawing, UI frames, party view, and battle rendering
#include "game.h"
int dlg_open = 0;
int battle_open = 0;
static const char* dlg_text = 0;
static int dlg_page = 0;
void wait_vblank(void) {
while (REG_VCOUNT >= 160);
while (REG_VCOUNT < 160);
}
static void draw_glyph5x8(int x, int y, char c, unsigned short color) {
int col, row;
const unsigned char* g;
if (c < 32 || c > 122) return;
g = font5x8[c - 32];
for (col = 0; col < 5; col++) {
unsigned char line = g[col];
for (row = 0; row < 8; row++) {
if (line & (1 << row)) {
int px = x + col;
int py = y + row;
if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
VRAM[py * SCREEN_W + px] = color;
}
}
}
}
void draw_text(int x, int y, const char* text, unsigned short color) {
while (*text) {
draw_glyph5x8(x, y, *text, color);
x += 6;
text++;
}
}
void draw_fill_rect(int x1, int y1, int x2, int y2, unsigned short color) {
int x, y;
if (x1 < 0) x1 = 0;
if (y1 < 0) y1 = 0;
if (x2 > SCREEN_W) x2 = SCREEN_W;
if (y2 > SCREEN_H) y2 = SCREEN_H;
for (y = y1; y < y2; y++) {
for (x = x1; x < x2; x++) {
VRAM[y * SCREEN_W + x] = color;
}
}
}
static void draw_frame(int x1, int y1, int x2, int y2, unsigned short bg, unsigned short border) {
draw_fill_rect(x1, y1, x2, y2, bg);
draw_fill_rect(x1, y1, x2, y1 + 1, border);
draw_fill_rect(x1, y2 - 1, x2, y2, border);
draw_fill_rect(x1, y1, x1 + 1, y2, border);
draw_fill_rect(x2 - 1, y1, x2, y2, border);
}
// Map Rendering
static void draw_tile(int screen_x, int screen_y, int kind) {
const unsigned short* src = tile_gfx[kind];
int y;
for (y = 0; y < TILE; y++) {
int py = screen_y + y;
if (py < 0 || py >= SCREEN_H) continue;
int x;
for (x = 0; x < TILE; x++) {
int px = screen_x + x;
if (px >= 0 && px < SCREEN_W)
VRAM[py * SCREEN_W + px] = src[y * TILE + x];
}
}
}
static void draw_sprite(int px, int py, int dir, int frame) {
const char (*art)[17] = (dir == DIR_DOWN) ? art_down : (dir == DIR_UP) ? art_up : art_left;
int flip = (dir == DIR_RIGHT);
int x, y;
for (y = 0; y < TILE; y++) {
int sy = py + y;
if (sy < 0 || sy >= SCREEN_H) continue;
for (x = 0; x < TILE; x++) {
int sx = px + x;
if (sx < 0 || sx >= SCREEN_W) continue;
char c = art[y][flip ? (15 - x) : x];
unsigned short col = CLEAR;
switch (c) {
case 'K': col = OUTLINE; break;
case 'B': col = RGB15(5, 11, 27); break;
case 'S': col = RGB15(31, 24, 18); break;
case 'H': col = RGB15(12, 7, 3); break;
case 'Y': col = RGB15(31, 27, 5); break;
case 'J': col = RGB15(6, 9, 20); break;
case 'R': col = RGB15(14, 8, 4); break;
}
if (col != CLEAR) VRAM[sy * SCREEN_W + sx] = col;
}
}
}
void paint_box(int left, int top, int right, int bottom, int px, int py, int dir) {
int tx, ty;
int min_tx = left >> 4, max_tx = (right + 15) >> 4;
int min_ty = top >> 4, max_ty = (bottom + 15) >> 4;
for (ty = min_ty; ty <= max_ty; ty++) {
for (tx = min_tx; tx <= max_tx; tx++) {
if (tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H) {
draw_tile(tx * TILE, ty * TILE, map_kind[cur_room][ty][tx]);
}
}
}
draw_sprite(px, py, dir, walk_frame);
}
void show_map(int x, int y, int dir) {
int tx, ty;
for (ty = 0; ty < MAP_H; ty++)
for (tx = 0; tx < MAP_W; tx++)
draw_tile(tx * TILE, ty * TILE, map_kind[cur_room][ty][tx]);
draw_sprite(x, y, dir, 0);
}
// Dialogue Box Logic
static void render_dialog_box(void) {
int page = 0;
const char* p = dlg_text;
draw_frame(10, 110, 230, 155, WHITE, OUTLINE);
while (page < dlg_page && *p) {
if (*p == '|') page++;
p++;
}
char line[36];
int idx = 0;
while (*p && *p != '|' && idx < 34) { line[idx++] = *p++; }
line[idx] = '\0';
draw_text(16, 122, line, OUTLINE);
}
void dialog_open(int n) {
dlg_open = 1;
dlg_text = npcs[n].text;
dlg_page = 0;
render_dialog_box();
}
void dialog_advance(int x, int y, int dir) {
const char* p = dlg_text;
int total = 0;
while (*p) { if (*p == '|') total++; p++; }
if (dlg_page < total) {
dlg_page++;
render_dialog_box();
} else {
dlg_open = 0;
show_map(x, y, dir);
}
}
// Battle Screen Rendering
static void draw_hp_bar(int x, int y, int hp, int max_hp) {
draw_frame(x, y, x + 60, y + 6, RGB15(10, 10, 10), OUTLINE);
int fill = (max_hp > 0) ? (hp * 58 / max_hp) : 0;
if (fill < 0) fill = 0;
if (fill > 58) fill = 58;
unsigned short col = GREEN;
if (hp * 4 < max_hp) col = RED;
else if (hp * 2 < max_hp) col = RGB15(31, 20, 5);
draw_fill_rect(x + 1, y + 1, x + 1 + fill, y + 5, col);
}
static void draw_mon_sprite(int x, int y, int sp) {
const char* art = species[sp].art;
int row, col;
for (row = 0; row < 8; row++) {
for (col = 0; col < 8; col++) {
char c = art[row * 8 + col];
unsigned short color = CLEAR;
if (c == 'K') color = OUTLINE;
else if (c == 'B') color = species[sp].body;
else if (c == 'A') color = species[sp].accent;
else if (c == 'E') color = WHITE;
else if (c == 'Y') color = RGB15(31, 27, 5);
if (color != CLEAR) {
draw_fill_rect(x + col * 4, y + row * 4, x + (col + 1) * 4, y + (row + 1) * 4, color);
}
}
}
}
void battle_screen_open(void) {
battle_open = 1;
draw_fill_rect(0, 0, SCREEN_W, SCREEN_H, RGB15(28, 28, 28));
bt_redraw = BT_HUD | BT_BOX;
battle_redraw();
}
void battle_close(void) {
battle_open = 0;
}
void battle_redraw(void) {
if (bt_redraw & BT_HUD) {
draw_fill_rect(0, 0, SCREEN_W, 105, RGB15(26, 29, 26));
// Enemy HUD (Top Left)
draw_frame(10, 8, 120, 42, WHITE, OUTLINE);
draw_text(15, 12, species[wild_mon.species].name, OUTLINE);
draw_text(85, 12, "Lv", OUTLINE);
char buf[10];
buf[0] = '\0'; str_add_num(buf, 10, wild_mon.level);
draw_text(98, 12, buf, OUTLINE);
draw_hp_bar(15, 26, wild_mon.hp, wild_mon.max_hp);
draw_mon_sprite(160, 10, wild_mon.species);
// Player HUD (Bottom Right)
draw_frame(120, 55, 230, 98, WHITE, OUTLINE);
draw_text(125, 59, species[player_mon.species].name, OUTLINE);
draw_text(195, 59, "Lv", OUTLINE);
buf[0] = '\0'; str_add_num(buf, 10, player_mon.level);
draw_text(208, 59, buf, OUTLINE);
draw_hp_bar(125, 72, player_mon.hp, player_mon.max_hp);
buf[0] = '\0';
str_add_num(buf, 10, player_mon.hp);
str_add(buf, 10, "/");
str_add_num(buf, 10, player_mon.max_hp);
draw_text(125, 82, buf, OUTLINE);
draw_mon_sprite(30, 60, player_mon.species);
}
if (bt_redraw & BT_BOX) {
draw_frame(5, 105, 235, 155, WHITE, OUTLINE);
if (bt_state == BS_MESSAGE) {
draw_text(12, 115, bt_line1, OUTLINE);
draw_text(12, 132, bt_line2, OUTLINE);
} else if (bt_state == BS_MENU) {
draw_text(12, 115, "What will you do?", OUTLINE);
draw_frame(120, 108, 232, 152, WHITE, OUTLINE);
draw_text(135, 114, "FIGHT", OUTLINE);
draw_text(185, 114, "PKMN", OUTLINE);
draw_text(135, 132, "BALL", OUTLINE);
draw_text(185, 132, "RUN", OUTLINE);
int cx = 126, cy = 114;
if (bt_cursor == MENU_POKEMON_SWAP) { cx = 176; cy = 114; }
else if (bt_cursor == MENU_CATCH) { cx = 126; cy = 132; }
else if (bt_cursor == MENU_RUN) { cx = 176; cy = 132; }
draw_text(cx, cy, ">", RED);
} else if (bt_state == BS_MOVES) {
int i;
for (i = 0; i < player_mon.num_moves; i++) {
int mx = 18 + (i % 2) * 105;
int my = 114 + (i / 2) * 18;
draw_text(mx, my, moves[player_mon.move[i]].name, OUTLINE);
}
int cx = 10 + (bt_cursor % 2) * 105;
int cy = 114 + (bt_cursor / 2) * 18;
draw_text(cx, cy, ">", RED);
}
}
bt_redraw = 0;
}
// MAIN MENU & SUB-SCREENS
void render_main_menu(void) {
draw_frame(140, 5, 235, 155, WHITE, OUTLINE);
const char* labels[MM_COUNT] = {
"POKEDEX", "POKEMON", "BAG", "TRAINER", "SAVE", "OPTION", "CLOSE"
};
int i;
for (i = 0; i < MM_COUNT; i++) {
draw_text(158, 12 + i * 20, labels[i], OUTLINE);
}
draw_text(146, 12 + main_menu_cursor * 20, ">", RED);
}
void render_party_screen(int is_battle) {
draw_fill_rect(0, 0, SCREEN_W, SCREEN_H, RGB15(10, 20, 28));
draw_frame(5, 5, 235, 22, WHITE, OUTLINE);
draw_text(12, 9, is_battle ? "CHOOSE A POKEMON TO SWITCH" : "POKEMON PARTY", OUTLINE);
int i;
for (i = 0; i < party_count; i++) {
int y = 28 + i * 21;
unsigned short border = (i == party_menu_cursor) ? RED : OUTLINE;
draw_frame(10, y, 230, y + 19, WHITE, border);
if (i == active_mon_idx) draw_text(14, y + 5, "*", BLUE);
draw_text(22, y + 5, species[party[i].species].name, OUTLINE);
char buf[12];
buf[0] = '\0'; str_add(buf, 12, "Lv"); str_add_num(buf, 12, party[i].level);
draw_text(90, y + 5, buf, OUTLINE);
if (party[i].hp <= 0) {
draw_text(150, y + 5, "FAINTED", RED);
} else {
draw_hp_bar(140, y + 6, party[i].hp, party[i].max_hp);
buf[0] = '\0';
str_add_num(buf, 12, party[i].hp);
str_add(buf, 12, "/");
str_add_num(buf, 12, party[i].max_hp);
draw_text(188, y + 5, buf, OUTLINE);
}
}
draw_frame(5, 138, 235, 155, WHITE, OUTLINE);
draw_text(10, 142, "A: SELECT   B: CANCEL", OUTLINE);
}
void render_sub_screen(int type) {
draw_fill_rect(0, 0, SCREEN_W, SCREEN_H, RGB15(26, 26, 26));
draw_frame(10, 10, 230, 150, WHITE, OUTLINE);
char buf[30];
if (type == MM_POKEDEX) {
draw_text(20, 20, "POKEDEX STATUS", OUTLINE);
draw_text(20, 50, "SEEN: 4", OUTLINE);
draw_text(20, 70, "CAUGHT: ", OUTLINE);
buf[0] = '\0'; str_add_num(buf, 30, party_count);
draw_text(70, 70, buf, OUTLINE);
} else if (type == MM_BAG) {
draw_text(20, 20, "TRAINER BAG", OUTLINE);
draw_text(20, 50, "POKE BALLS:", OUTLINE);
buf[0] = '\0'; str_add_num(buf, 30, pokeballs);
draw_text(100, 50, buf, OUTLINE);
draw_text(20, 70, "POTION: x0", OUTLINE);
} else if (type == MM_TRAINER) {
draw_text(20, 20, "TRAINER CARD", OUTLINE);
draw_text(20, 50, "NAME: RED", OUTLINE);
draw_text(20, 70, "MONEY: $1000", OUTLINE);
draw_text(20, 90, "BADGES: 0", OUTLINE);
} else if (type == MM_SAVE) {
draw_text(20, 20, "SAVE GAME", OUTLINE);
draw_text(20, 60, "NO EEPROM STORAGE DETECTED.", RED);
draw_text(20, 80, "SAVING IS UNAVAILABLE.", RED);
} else if (type == MM_OPTIONS) {
draw_text(20, 20, "SETTINGS", OUTLINE);
draw_text(20, 50, "TEXT SPEED: FAST", OUTLINE);
draw_text(20, 70, "BATTLE ANIM: ON", OUTLINE);
draw_text(20, 90, "SOUND: MONO", OUTLINE);
}
draw_text(20, 130, "PRESS B TO RETURN", OUTLINE);
}
