// main.c - system loop, user input handling, and overarching game state transitions
#include "game.h"
static void change_room(int room, int sx, int sy, int sdir, int* x, int* y, int* dir) {
cur_room = room;
rooms[cur_room].build();
pick_start(sx, sy, x, y);
*dir = sdir;
walk_frame = 0;
walk_timer = 0;
steps_reset(*x, *y);
show_map(*x, *y, *dir);
}
int main(void) {
int x, y;
int dir = DIR_UP;
int n, m;
unsigned int frame = 0;
unsigned short prev_keys = 0;
REG_DISPCNT = 0x0080;
for (m = 0; m < NUM_ROOMS; m++) build_map(m);
build_sprites();
cur_room = R_TOWN;
rooms[cur_room].build();
pick_start(START_TX, START_TY, &x, &y);
steps_reset(x, y);
show_map(x, y, dir);
while (1) {
unsigned short keys;
int old_x = x, old_y = y, old_dir = dir;
int old_walk_frame = walk_frame;
int dx = 0, dy = 0;
int moved, door, npc, a_press, start_press, b_press, warp, wild;
unsigned short pressed;
wait_vblank();
frame++;
keys = (unsigned short)(~REG_KEYINPUT) & 0x03FF;
rng_stir(frame ^ ((unsigned int)keys << 16));
pressed = (unsigned short)(keys & ~prev_keys);
a_press = (pressed & KEY_A) != 0;
b_press = (pressed & KEY_B) != 0;
start_press = (pressed & KEY_START) != 0;
// STATE: MAIN MENU
if (game_state == GM_MAIN_MENU) {
prev_keys = keys;
if (pressed & KEY_UP) {
main_menu_cursor = (main_menu_cursor + MM_COUNT - 1) % MM_COUNT;
render_main_menu();
} else if (pressed & KEY_DOWN) {
main_menu_cursor = (main_menu_cursor + 1) % MM_COUNT;
render_main_menu();
} else if (b_press || (a_press && main_menu_cursor == MM_CLOSE)) {
game_state = GM_WORLD;
show_map(x, y, dir);
} else if (a_press) {
if (main_menu_cursor == MM_POKEMON) {
game_state = GM_PARTY_MENU;
party_menu_cursor = 0;
render_party_screen(0);
} else {
game_state = GM_SUB_MENU;
sub_menu_type = main_menu_cursor;
render_sub_screen(sub_menu_type);
}
}
continue;
}
// STATE: SUB MENU
if (game_state == GM_SUB_MENU) {
prev_keys = keys;
if (b_press || a_press) {
game_state = GM_MAIN_MENU;
show_map(x, y, dir);
render_main_menu();
}
continue;
}
// STATE: PARTY MENU
if (game_state == GM_PARTY_MENU) {
prev_keys = keys;
if (pressed & KEY_UP) {
party_menu_cursor = (party_menu_cursor + party_count - 1) % party_count;
render_party_screen(battle_open);
} else if (pressed & KEY_DOWN) {
party_menu_cursor = (party_menu_cursor + 1) % party_count;
render_party_screen(battle_open);
} else if (b_press) {
if (battle_open) {
game_state = GM_BATTLE;
bt_redraw = BT_HUD | BT_BOX;
battle_redraw();
} else {
game_state = GM_MAIN_MENU;
show_map(x, y, dir);
render_main_menu();
}
} else if (a_press) {
if (battle_open) {
if (switch_active_mon(party_menu_cursor)) {
game_state = GM_BATTLE;
bt_redraw = BT_HUD | BT_BOX;
str_set(bt_line1, BT_LINE, "Go! ");
str_add(bt_line1, BT_LINE, species[player_mon.species].name);
str_add(bt_line1, BT_LINE, "!");
str_set(bt_line2, BT_LINE, "");
bt_state = BS_MESSAGE;
bt_after = AFTER_SWITCH_TURN;
battle_redraw();
}
} else {
// Field selection context
render_party_screen(0);
}
}
continue;
}
// STATE: BATTLE
if (battle_open) {
int result;
prev_keys = keys;
result = battle_input(pressed);
if (result == BATTLE_GOING) {
if (bt_redraw) battle_redraw();
} else {
battle_close();
if (result == BATTLE_LOST)
change_room(BLACKOUT_ROOM, BLACKOUT_TX, BLACKOUT_TY, DIR_UP, &x, &y, &dir);
else
show_map(x, y, dir);
}
continue;
}
// STATE: DIALOGUE
if (dlg_open) {
prev_keys = keys;
if (a_press) dialog_advance(x, y, dir);
continue;
}
// Open Main Menu in overworld with START
if (start_press) {
prev_keys = keys;
game_state = GM_MAIN_MENU;
main_menu_cursor = 0;
render_main_menu();
continue;
}
// Overworld Interaction
door = a_press ? front_door(x, y, dir) : -1;
npc  = (a_press && door < 0) ? front_npc(x, y, dir) : -1;
if (npc >= 0) {
prev_keys = keys;
walk_frame = 0;
walk_timer = 0;
paint_box(x, y, x + TILE, y + TILE, x, y, dir);
if (npcs[npc].heals) heal_party();
dialog_open(npc);
continue;
}
if (door >= 0) {
change_room(doors[door].to, doors[door].sx, doors[door].sy,
doors[door].sdir, &x, &y, &dir);
prev_keys = keys;
continue;
}
prev_keys = keys;
if (keys & KEY_LEFT)  { dx = -1; dir = DIR_LEFT;  }
if (keys & KEY_RIGHT) { dx =  1; dir = DIR_RIGHT; }
if (keys & KEY_UP)    { dy = -1; dir = DIR_UP;    }
if (keys & KEY_DOWN)  { dy =  1; dir = DIR_DOWN;  }
for (n = 0; n < SPEED; n++) {
if (dx != 0 && can_stand(x + dx, y)) x += dx;
if (dy != 0 && can_stand(x, y + dy)) y += dy;
}
moved = (x != old_x || y != old_y);
warp = moved ? warp_at_feet(x, y) : -1;
if (warp >= 0) {
change_room(warps[warp].to, warps[warp].sx, warps[warp].sy,
warps[warp].sdir, &x, &y, &dir);
continue;
}
if (moved) {
walk_timer++;
if (walk_timer >= 6) { walk_timer = 0; walk_frame = 1 - walk_frame; }
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
wild = moved ? step_encounter(x, y) : -1;
if (wild >= 0) {
walk_frame = 0;
walk_timer = 0;
battle_begin(wild);
battle_screen_open();
continue;
}
}
}
