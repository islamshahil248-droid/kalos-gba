// world.c - collision, interaction, party switching, battle logic and EXP attribution
#include "game.h"
void build_map(int m) {
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
static int tile_at(int px, int py) {
if (px < 0 || py < 0 || px >= SCREEN_W || py >= SCREEN_H) return -1;
return map_kind[cur_room][py >> 4][px >> 4];
}
static int is_blocked(int px, int py) {
int k = tile_at(px, py);
return k < 0 || k >= rooms[cur_room].solid_from;
}
int can_stand(int px, int py) {
return !is_blocked(px + HB_LEFT,  py + HB_TOP) &&
!is_blocked(px + HB_RIGHT, py + HB_TOP) &&
!is_blocked(px + HB_LEFT,  py + HB_BOTTOM) &&
!is_blocked(px + HB_RIGHT, py + HB_BOTTOM);
}
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
int front_door(int x, int y, int dir) {
int ax, ay, bx, by, d;
if (dir == DIR_UP)        { ay = by = y + HB_TOP - 3;    ax = x + HB_LEFT + 2; bx = x + HB_RIGHT - 2; }
else if (dir == DIR_DOWN) { ay = by = y + HB_BOTTOM + 3; ax = x + HB_LEFT + 2; bx = x + HB_RIGHT - 2; }
else if (dir == DIR_LEFT) { ax = bx = x + HB_LEFT - 3;   ay = y + HB_TOP + 2;  by = y + HB_BOTTOM - 2; }
else                      { ax = bx = x + HB_RIGHT + 3;  ay = y + HB_TOP + 2;  by = y + HB_BOTTOM - 2; }
d = door_at(ax, ay);
if (d < 0) d = door_at(bx, by);
return d;
}
void pick_start(int stx, int sty, int* out_x, int* out_y) {
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
static int npc_at(int tx, int ty) {
int i;
for (i = 0; i < NUM_NPCS; i++)
if (npcs[i].room == cur_room && npcs[i].tx == tx && npcs[i].ty == ty)
return i;
return -1;
}
static int npc_ahead(int ax, int ay, int dx, int dy) {
int tx, ty, step, i;
if (ax < 0 || ay < 0 || ax >= SCREEN_W || ay >= SCREEN_H) return -1;
tx = ax >> 4;
ty = ay >> 4;
for (step = 1; step <= 3; step++) {
i = npc_at(tx, ty);
if (i >= 0) return (step <= npcs[i].reach) ? i : -1;
if (map_kind[cur_room][ty][tx] < rooms[cur_room].solid_from) return -1;
tx += dx;
ty += dy;
if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return -1;
}
return -1;
}
int front_npc(int x, int y, int dir) {
int ax, ay, bx, by, dx = 0, dy = 0, i;
if (dir == DIR_UP)        { ay = by = y + HB_TOP - 3;    ax = x + HB_LEFT + 2; bx = x + HB_RIGHT - 2; dy = -1; }
else if (dir == DIR_DOWN) { ay = by = y + HB_BOTTOM + 3; ax = x + HB_LEFT + 2; bx = x + HB_RIGHT - 2; dy = 1; }
else if (dir == DIR_LEFT) { ax = bx = x + HB_LEFT - 3;   ay = y + HB_TOP + 2;  by = y + HB_BOTTOM - 2; dx = -1; }
else                      { ax = bx = x + HB_RIGHT + 3;  ay = y + HB_TOP + 2;  by = y + HB_BOTTOM - 2; dx = 1; }
i = npc_ahead(ax, ay, dx, dy);
if (i < 0) i = npc_ahead(bx, by, dx, dy);
return i;
}
static int last_step_tx = -1, last_step_ty = -1;
static int enc_cooldown = 0;
static unsigned int rng_state = 0x2545F491u;
void rng_stir(unsigned int v) {
rng_state ^= v + 0x9E3779B9u + (rng_state << 6) + (rng_state >> 2);
if (rng_state == 0) rng_state = 1;
}
static unsigned int rng_next(void) {
rng_state ^= rng_state << 13;
rng_state ^= rng_state >> 17;
rng_state ^= rng_state << 5;
return rng_state;
}
static int foot_tx(int x) { return (x + 8) >> 4; }
static int foot_ty(int y) { return (y + HB_BOTTOM - 2) >> 4; }
int warp_at_feet(int x, int y) {
int i, tx = foot_tx(x), ty = foot_ty(y);
for (i = 0; i < NUM_WARPS; i++)
if (warps[i].from == cur_room && warps[i].tx == tx && warps[i].ty == ty)
return i;
return -1;
}
void steps_reset(int x, int y) {
last_step_tx = foot_tx(x);
last_step_ty = foot_ty(y);
enc_cooldown = 0;
}
int step_encounter(int x, int y) {
int tx = foot_tx(x), ty = foot_ty(y), i, total, r;
const Zone* z = 0;
if (tx == last_step_tx && ty == last_step_ty) return -1;
last_step_tx = tx;
last_step_ty = ty;
if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return -1;
if (rooms[cur_room].tall == NO_TILE || map_kind[cur_room][ty][tx] != rooms[cur_room].tall) return -1;
if (enc_cooldown > 0) { enc_cooldown--; return -1; }
for (i = 0; i < NUM_ZONES; i++)
if (zones[i].room == cur_room) z = &zones[i];
if (!z) return -1;
if ((int)(rng_next() % 100) >= z->percent) return -1;
total = 0;
for (i = 0; i < z->count; i++) total += z->entries[i].weight;
if (total <= 0) return -1;
r = (int)(rng_next() % (unsigned int)total);
for (i = 0; i < z->count; i++) {
if (r < z->entries[i].weight) {
enc_cooldown = ENC_COOLDOWN_STEPS;
return z->entries[i].species;
}
r -= z->entries[i].weight;
}
return -1;
}
void str_add(char* dst, int size, const char* s) {
int n = 0;
while (dst[n]) n++;
while (*s && n < size - 1) dst[n++] = *s++;
dst[n] = '\0';
}
void str_set(char* dst, int size, const char* s) {
dst[0] = '\0';
str_add(dst, size, s);
}
void str_add_num(char* dst, int size, int v) {
char digits[12], one[2];
int n = 0, i;
if (v < 0) v = 0;
if (v == 0) digits[n++] = '0';
while (v > 0 && n < 11) { digits[n++] = (char)('0' + v % 10); v /= 10; }
one[1] = '\0';
for (i = n - 1; i >= 0; i--) { one[0] = digits[i]; str_add(dst, size, one); }
}
static const char* mon_name(const Mon* m) { return species[m->species].name; }
void heal_party(void) {
int i;
for (i = 0; i < party_count; i++) party[i].hp = party[i].max_hp;
}
int has_alive_mon(void) {
int i;
for (i = 0; i < party_count; i++) {
if (party[i].hp > 0) return 1;
}
return 0;
}
int exp_to_next(int level) {
return 20 + 4 * level;
}
static int max_hp_for_level(int species_idx, int level) {
return species[species_idx].hp_base + 2 * level;
}
int party_add(const Mon* m) {
Mon* d;
int i;
if (party_count >= PARTY_MAX) return -1;
d = &party[party_count];
d->species = m->species;
d->level = m->level;
d->hp = m->hp;
d->max_hp = m->max_hp;
d->exp = m->exp;
d->num_moves = m->num_moves;
for (i = 0; i < MAX_MOVES; i++) d->move[i] = m->move[i];
return party_count++;
}
int switch_active_mon(int new_idx) {
if (new_idx < 0 || new_idx >= party_count) return 0;
if (new_idx == active_mon_idx) return 0;
if (party[new_idx].hp <= 0) return 0;
active_mon_idx = new_idx;
return 1;
}
static void say(int after, const char* line1, const char* line2) {
str_set(bt_line1, BT_LINE, line1);
str_set(bt_line2, BT_LINE, line2);
bt_state = BS_MESSAGE;
bt_after = after;
bt_redraw |= BT_BOX;
}
static int calc_damage(int level, int power) {
return power * level / 30 + 1 + (int)(rng_next() % 3u);
}
static void take_damage(Mon* m, int dmg) {
m->hp -= dmg;
if (m->hp < 0) m->hp = 0;
bt_redraw |= BT_HUD;
}
static void say_attack(const Mon* attacker, const Mon* target, int mv, int dmg, int after) {
char a[BT_LINE], b[BT_LINE];
str_set(a, BT_LINE, "");
if (attacker == &wild_mon) str_add(a, BT_LINE, "Wild ");
str_add(a, BT_LINE, mon_name(attacker));
str_add(a, BT_LINE, " used ");
str_add(a, BT_LINE, moves[mv].name);
str_add(a, BT_LINE, "!");
str_set(b, BT_LINE, mon_name(target));
str_add(b, BT_LINE, " took ");
str_add_num(b, BT_LINE, dmg);
str_add(b, BT_LINE, " damage.");
say(after, a, b);
}
void battle_begin(int sp) {
char a[BT_LINE], b[BT_LINE];
int i;
wild_species = sp;
wild_mon.species = (unsigned char)sp;
wild_mon.level = (unsigned char)(2 + rng_next() % 3u);
wild_mon.max_hp = species[sp].hp_base + 2 * wild_mon.level;
wild_mon.hp = wild_mon.max_hp;
wild_mon.exp = 0;
wild_mon.num_moves = 1;
wild_mon.move[0] = species[sp].move;
// Auto pick first healthy mon
if (player_mon.hp <= 0) {
for (i = 0; i < party_count; i++) {
if (party[i].hp > 0) { active_mon_idx = i; break; }
}
}
if (player_mon.hp <= 0) heal_party();
bt_cursor = 0;
bt_redraw = BT_HUD | BT_BOX;
str_set(a, BT_LINE, "A wild ");
str_add(a, BT_LINE, mon_name(&wild_mon));
str_add(a, BT_LINE, " appeared!");
str_set(b, BT_LINE, "Go! ");
str_add(b, BT_LINE, mon_name(&player_mon));
str_add(b, BT_LINE, "!");
say(AFTER_MENU, a, b);
}
static void player_attack(int slot) {
int mv = player_mon.move[slot];
int dmg = calc_damage(player_mon.level, moves[mv].power);
take_damage(&wild_mon, dmg);
say_attack(&player_mon, &wild_mon, mv, dmg,
(wild_mon.hp <= 0) ? AFTER_WIN : AFTER_ENEMY);
}
static void enemy_attack(void) {
int mv = wild_mon.move[0];
int dmg = calc_damage(wild_mon.level, moves[mv].power);
take_damage(&player_mon, dmg);
say_attack(&wild_mon, &player_mon, mv, dmg,
(player_mon.hp <= 0) ? (has_alive_mon() ? AFTER_MUST_SWITCH : AFTER_LOSE) : AFTER_MENU);
}
static int catch_chance(void) {
return 15 + 65 * (wild_mon.max_hp - wild_mon.hp) / wild_mon.max_hp;
}
static void throw_ball(void) {
char b[BT_LINE];
if (party_count >= PARTY_MAX) {
say(AFTER_MENU, "Your party is full!", "You can't catch more POKEMON.");
return;
}
if (pokeballs <= 0) {
say(AFTER_MENU, "You have no Poke Balls left!", "Nothing was thrown.");
return;
}
pokeballs--;
str_set(b, BT_LINE, "Poke Balls left: ");
str_add_num(b, BT_LINE, pokeballs);
say(AFTER_CATCH, "You threw a Poke Ball!", b);
}
static void catch_result(void) {
char l1[BT_LINE], l2[BT_LINE];
int slot = -1;
if ((int)(rng_next() % 100u) < catch_chance()) slot = party_add(&wild_mon);
if (slot >= 0) {
str_set(l1, BT_LINE, "Gotcha! ");
str_add(l1, BT_LINE, mon_name(&wild_mon));
str_add(l1, BT_LINE, " was caught!");
str_set(l2, BT_LINE, "Added to party (");
str_add_num(l2, BT_LINE, party_count);
str_add(l2, BT_LINE, "/");
str_add_num(l2, BT_LINE, PARTY_MAX);
str_add(l2, BT_LINE, ").");
say(AFTER_END_CAUGHT, l1, l2);
} else {
str_set(l1, BT_LINE, "Oh no! ");
str_add(l1, BT_LINE, mon_name(&wild_mon));
str_add(l1, BT_LINE, " broke free!");
say(AFTER_ENEMY, l1, "");
}
}
static void level_up(void) {
char l1[BT_LINE], l2[BT_LINE];
int old_max = player_mon.max_hp, new_max;
player_mon.exp = (unsigned short)(player_mon.exp - exp_to_next(player_mon.level));
player_mon.level = (unsigned char)(player_mon.level + 1);
new_max = max_hp_for_level(player_mon.species, player_mon.level);
player_mon.max_hp = new_max;
player_mon.hp += new_max - old_max;
if (player_mon.hp > new_max) player_mon.hp = new_max;
if (player_mon.hp < 1) player_mon.hp = 1;
if (player_mon.level >= MAX_LEVEL) player_mon.exp = 0;
str_set(l1, BT_LINE, mon_name(&player_mon));
str_add(l1, BT_LINE, " grew to Lv");
str_add_num(l1, BT_LINE, player_mon.level);
str_add(l1, BT_LINE, "!");
str_set(l2, BT_LINE, "Max HP ");
str_add_num(l2, BT_LINE, new_max);
str_add(l2, BT_LINE, ". Stats increased!");
say(AFTER_LEVELUP, l1, l2);
bt_redraw |= BT_HUD;
}
int battle_input(unsigned short pressed) {
int a = (pressed & KEY_A) != 0;
int b = (pressed & KEY_B) != 0;
int step = 0;
char l1[BT_LINE], l2[BT_LINE];
if (pressed & (KEY_DOWN | KEY_RIGHT)) step = 1;
if (pressed & (KEY_UP | KEY_LEFT)) step = -1;
if (bt_state == BS_MENU) {
int pos = bt_cursor;
if ((pressed & KEY_RIGHT) && pos == MENU_FIGHT) pos = MENU_POKEMON_SWAP;
if ((pressed & KEY_LEFT)  && pos == MENU_POKEMON_SWAP) pos = MENU_FIGHT;
if ((pressed & KEY_DOWN)  && pos == MENU_FIGHT) pos = MENU_CATCH;
if ((pressed & KEY_DOWN)  && pos == MENU_POKEMON_SWAP) pos = MENU_RUN;
if ((pressed & KEY_UP)    && pos == MENU_CATCH) pos = MENU_FIGHT;
if ((pressed & KEY_UP)    && pos == MENU_RUN) pos = MENU_POKEMON_SWAP;
if ((pressed & KEY_RIGHT) && pos == MENU_CATCH) pos = MENU_RUN;
if ((pressed & KEY_LEFT)  && pos == MENU_RUN) pos = MENU_CATCH;
if (pos != bt_cursor) { bt_cursor = pos; bt_redraw |= BT_BOX; }
if (a) {
if (bt_cursor == MENU_FIGHT) {
bt_state = BS_MOVES;
bt_cursor = 0;
bt_redraw |= BT_BOX;
} else if (bt_cursor == MENU_POKEMON_SWAP) {
game_state = GM_PARTY_MENU;
party_menu_cursor = 0;
return BATTLE_GOING;
} else if (bt_cursor == MENU_CATCH) {
throw_ball();
} else {
say(AFTER_END_RAN, "Got away safely!", "");
}
}
} else if (bt_state == BS_MOVES) {
int n = player_mon.num_moves;
if (step != 0 && n > 0) { bt_cursor = (bt_cursor + step + n) % n; bt_redraw |= BT_BOX; }
if (b) { bt_state = BS_MENU; bt_cursor = 0; bt_redraw |= BT_BOX; }
else if (a) player_attack(bt_cursor);
} else if (a) {
switch (bt_after) {
case AFTER_MENU:
bt_state = BS_MENU;
bt_cursor = 0;
bt_redraw |= BT_BOX;
break;
case AFTER_SWITCH_TURN:
enemy_attack();
break;
case AFTER_ENEMY:
enemy_attack();
break;
case AFTER_WIN: {
int gain = 5 + 2 * wild_mon.level;
str_set(l1, BT_LINE, "Wild ");
str_add(l1, BT_LINE, mon_name(&wild_mon));
str_add(l1, BT_LINE, " fainted!");
str_set(l2, BT_LINE, mon_name(&player_mon));
if (player_mon.level >= MAX_LEVEL) {
str_add(l2, BT_LINE, " is at max level!");
} else {
player_mon.exp = (unsigned short)(player_mon.exp + gain);
str_add(l2, BT_LINE, " gained ");
str_add_num(l2, BT_LINE, gain);
str_add(l2, BT_LINE, " Exp. Points!");
}
say(AFTER_LEVELUP, l1, l2);
bt_redraw |= BT_HUD;
break;
}
case AFTER_LEVELUP:
if (player_mon.level < MAX_LEVEL && player_mon.exp >= (unsigned short)exp_to_next(player_mon.level))
level_up();
else
return BATTLE_WON;
break;
case AFTER_CATCH:
catch_result();
break;
case AFTER_MUST_SWITCH:
str_set(l1, BT_LINE, mon_name(&player_mon));
str_add(l1, BT_LINE, " fainted!");
say(AFTER_MENU, l1, "Choose another POKEMON!");
game_state = GM_PARTY_MENU;
party_menu_cursor = 0;
break;
case AFTER_LOSE:
str_set(l1, BT_LINE, mon_name(&player_mon));
str_add(l1, BT_LINE, " fainted!");
say(AFTER_END_LOST, l1, "You rushed to the POKEMON CENTER!");
break;
case AFTER_END_WON:    return BATTLE_WON;
case AFTER_END_RAN:    return BATTLE_RAN;
case AFTER_END_CAUGHT: return BATTLE_CAUGHT;
case AFTER_END_LOST:
heal_party();
return BATTLE_LOST;
default:
break;
}
}
return BATTLE_GOING;
}
