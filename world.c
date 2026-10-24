// world.c - the rules: turning maps into tiles, collision, doors, finding
// the person in front of you, wild encounters, the battle rules, leveling and catching.
#include "game.h"

// Turn map letters into tile numbers (unknown letters: tile 0).
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
int can_stand(int px, int py) {
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

// Put the trainer on tile (stx, sty); if that is blocked use the first free tile.
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

// ---------- Talking to people ----------
// Index of the person standing on this tile of the current room, or -1.
static int npc_at(int tx, int ty) {
  int i;
  for (i = 0; i < NUM_NPCS; i++)
    if (npcs[i].room == cur_room && npcs[i].tx == tx && npcs[i].ty == ty)
      return i;
  return -1;
}

// Look from pixel (ax, ay) in direction (dx, dy), tile by tile. A person
// counts if they are within their "reach"; we only look further through
// SOLID tiles (a counter), never across open floor.
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

// The person right in front of the trainer (within 3 px), or -1.
// Same two probe points as front_door.
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

// ---------- Exits you walk onto, and wild encounters ----------
static int last_step_tx = -1, last_step_ty = -1;   // tile of the last step
static int enc_cooldown = 0;                       // safe tall-grass steps left
static unsigned int rng_state = 0x2545F491u;       // random number state

// Mix a number into the random state (main calls this every frame, so the
// result depends on how long you have been playing and on your button timing).
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

// The map tile under the trainer's feet (the middle of the body box).
static int foot_tx(int x) { return (x + 8) >> 4; }
static int foot_ty(int y) { return (y + HB_BOTTOM - 2) >> 4; }

// Index of the warp the trainer is standing on, or -1.
int warp_at_feet(int x, int y) {
  int i, tx = foot_tx(x), ty = foot_ty(y);
  for (i = 0; i < NUM_WARPS; i++)
    if (warps[i].from == cur_room && warps[i].tx == tx && warps[i].ty == ty)
      return i;
  return -1;
}

// Call when arriving in a room: the tile you arrive on is not a "new step",
// and the encounter cooldown starts fresh.
void steps_reset(int x, int y) {
  last_step_tx = foot_tx(x);
  last_step_ty = foot_ty(y);
  enc_cooldown = 0;
}

// Call after the trainer MOVED. Only stepping onto a NEW tile counts as a step,
// and only on the room's tall grass. Returns the wild species (SP_...) or -1.
int step_encounter(int x, int y) {
  int tx = foot_tx(x), ty = foot_ty(y), i, total, r;
  const Zone* z = 0;
  if (tx == last_step_tx && ty == last_step_ty) return -1;   // same tile
  last_step_tx = tx;
  last_step_ty = ty;
  if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return -1;
  if (rooms[cur_room].tall == NO_TILE ||
      map_kind[cur_room][ty][tx] != rooms[cur_room].tall) return -1;
  if (enc_cooldown > 0) { enc_cooldown--; return -1; }       // safe steps
  for (i = 0; i < NUM_ZONES; i++)
    if (zones[i].room == cur_room) z = &zones[i];
  if (!z) return -1;
  if ((int)(rng_next() % 100) >= z->percent) return -1;      // no encounter
  total = 0;
  for (i = 0; i < z->count; i++) total += z->entries[i].weight;
  if (total <= 0) return -1;
  r = (int)(rng_next() % (unsigned int)total);               // pick by weight
  for (i = 0; i < z->count; i++) {
    if (r < z->entries[i].weight) {
      enc_cooldown = ENC_COOLDOWN_STEPS;
      return z->entries[i].species;
    }
    r -= z->entries[i].weight;
  }
  return -1;
}

// ---------- Battles ----------
// The battle is a tiny state machine. bt_state says what the bottom box shows:
//   BS_MESSAGE  two lines of text; A closes them and then bt_after happens
//   BS_MENU     FIGHT / CATCH / RUN
//   BS_MOVES    the list of your moves
// Every turn goes: you attack -> (if it survives) it attacks -> back to menu.
// A Poke Ball throw also uses up your turn: if it fails, the wild one attacks.
// This file only changes numbers and text; render.c draws them (bt_redraw
// tells it what is out of date).

// Tiny text helpers (no sprintf: we use no libraries).
// Add text s to the end of dst, never writing more than `size` bytes in total.
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

// Add the number v (0 or more) written in digits.
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

// Heal every POKEMON in your party to full HP. (The nurse calls this; so does
// fainting.) EXP, levels and Poke Balls are kept, nothing else changes.
void heal_party(void) {
  int i;
  for (i = 0; i < party_count; i++) party[i].hp = party[i].max_hp;
}

// ---------- Leveling rules ----------
// EXP you must collect to go from `level` to `level + 1`: 20 + 4 per level.
// (Level 5 needs 40, level 6 needs 44, level 10 needs 60 ...)
// Your EXP number counts only the progress toward the NEXT level: it starts
// again from the leftover after every level-up.
int exp_to_next(int level) {
  return 20 + 4 * level;
}

// Maximum HP of PIKACHU at a level: 10 + 2 per level (level 5 = 20 HP).
static int max_hp_for_level(int level) {
  return 10 + 2 * level;
}

// ---------- The party ----------
// Copy a POKEMON into the next free party slot. Returns the slot number, or
// -1 if the party is already full (PARTY_MAX). It keeps species, level, HP...
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

// Show a two-line message; `after` says what happens when A closes it.
static void say(int after, const char* line1, const char* line2) {
  str_set(bt_line1, BT_LINE, line1);
  str_set(bt_line2, BT_LINE, line2);
  bt_state = BS_MESSAGE;
  bt_after = after;
  bt_redraw |= BT_BOX;
}

// Damage of one hit: grows with the attacker's level and the move's power,
// plus 0-2 extra points of luck. (Level 5 THUNDER SHOCK = 7 to 9.)
static int calc_damage(int level, int power) {
  return power * level / 30 + 1 + (int)(rng_next() % 3u);
}

// Take `dmg` HP away (HP never goes below 0).
static void take_damage(Mon* m, int dmg) {
  m->hp -= dmg;
  if (m->hp < 0) m->hp = 0;
  bt_redraw |= BT_HUD;
}

// "<attacker> used <move>!" / "<target> took N damage."
static void say_attack(const Mon* attacker, const Mon* target, int mv,
                       int dmg, int after) {
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

// Start a fight against wild species sp: make the wild POKEMON (level 2-4)
// and show the first message.
void battle_begin(int sp) {
  char a[BT_LINE], b[BT_LINE];
  wild_species = sp;
  wild_mon.species = (unsigned char)sp;
  wild_mon.level = (unsigned char)(2 + rng_next() % 3u);
  wild_mon.max_hp = species[sp].hp_base + 2 * wild_mon.level;
  wild_mon.hp = wild_mon.max_hp;
  wild_mon.exp = 0;
  wild_mon.num_moves = 1;
  wild_mon.move[0] = species[sp].move;
  if (player_mon.hp <= 0) heal_party();                        // safety net
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

// Your attack with move slot `slot`.
static void player_attack(int slot) {
  int mv = player_mon.move[slot];
  int dmg = calc_damage(player_mon.level, moves[mv].power);
  take_damage(&wild_mon, dmg);
  say_attack(&player_mon, &wild_mon, mv, dmg,
             (wild_mon.hp <= 0) ? AFTER_WIN : AFTER_ENEMY);
}

// The wild POKEMON attacks back.
static void enemy_attack(void) {
  int mv = wild_mon.move[0];
  int dmg = calc_damage(wild_mon.level, moves[mv].power);
  take_damage(&player_mon, dmg);
  say_attack(&wild_mon, &player_mon, mv, dmg,
             (player_mon.hp <= 0) ? AFTER_LOSE : AFTER_MENU);
}

// ---------- Catching ----------
// Chance (in percent) that a thrown ball works: 15% against a POKEMON with
// full HP, rising to about 80% when it is almost fainted.
static int catch_chance(void) {
  return 15 + 65 * (wild_mon.max_hp - wild_mon.hp) / wild_mon.max_hp;
}

// CATCH was chosen. If a ball can really be thrown, use one up and show the
// throw message; the result comes after A (AFTER_CATCH). If not, explain why:
// nothing is used up and it is still your turn.
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

// The ball landed: roll the dice. Caught = it joins the party and the battle
// ends WITHOUT the wild one attacking. Broke free = the wild one attacks.
static void catch_result(void) {
  char l1[BT_LINE], l2[BT_LINE];
  int slot = -1;
  if ((int)(rng_next() % 100u) < catch_chance()) slot = party_add(&wild_mon);
  if (slot >= 0) {
    str_set(l1, BT_LINE, "Gotcha! ");
    str_add(l1, BT_LINE, mon_name(&wild_mon));
    str_add(l1, BT_LINE, " was caught!");
    str_set(l2, BT_LINE, "Added to your party (");
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

// ---------- Level-up ----------
// PIKACHU has enough EXP for the next level: pay the EXP, go up one level,
// raise max HP (and the current HP by the same amount, so a hurt PIKACHU stays
// hurt) and show the news. Attacks get stronger by themselves because
// calc_damage() uses the level.
static void level_up(void) {
  char l1[BT_LINE], l2[BT_LINE];
  int old_max = player_mon.max_hp, new_max;
  player_mon.exp = (unsigned short)(player_mon.exp - exp_to_next(player_mon.level));
  player_mon.level = (unsigned char)(player_mon.level + 1);
  new_max = max_hp_for_level(player_mon.level);
  player_mon.max_hp = new_max;
  player_mon.hp += new_max - old_max;
  if (player_mon.hp > new_max) player_mon.hp = new_max;   // stay inside 1..max
  if (player_mon.hp < 1) player_mon.hp = 1;
  if (player_mon.level >= MAX_LEVEL) player_mon.exp = 0;  // nothing left to earn
  str_set(l1, BT_LINE, mon_name(&player_mon));
  str_add(l1, BT_LINE, " grew to Lv");
  str_add_num(l1, BT_LINE, player_mon.level);
  str_add(l1, BT_LINE, "!");
  str_set(l2, BT_LINE, "Max HP ");
  str_add_num(l2, BT_LINE, new_max);
  str_add(l2, BT_LINE, ". Attack went up!");
  say(AFTER_LEVELUP, l1, l2);          // asks again: maybe another level is due
  bt_redraw |= BT_HUD;
}

// Call once per frame with the buttons that were JUST pressed this frame
// (not held). Returns BATTLE_GOING until the battle is over.
int battle_input(unsigned short pressed) {
  int a = (pressed & KEY_A) != 0;
  int b = (pressed & KEY_B) != 0;
  int step = 0;                      // -1 = up/left, +1 = down/right
  char l1[BT_LINE], l2[BT_LINE];
  if (pressed & (KEY_DOWN | KEY_RIGHT)) step = 1;
  if (pressed & (KEY_UP | KEY_LEFT)) step = -1;

  if (bt_state == BS_MENU) {
    // The menu is   FIGHT  CATCH     (bt_cursor 0, 1)
    //               RUN              (bt_cursor 2)
    int pos = bt_cursor;
    if ((pressed & KEY_RIGHT) && pos == MENU_FIGHT) pos = MENU_CATCH;
    if ((pressed & KEY_LEFT)  && pos == MENU_CATCH) pos = MENU_FIGHT;
    if ((pressed & KEY_DOWN)  && pos == MENU_FIGHT) pos = MENU_RUN;
    if ((pressed & KEY_UP)    && pos == MENU_RUN)   pos = MENU_FIGHT;
    if (pos != bt_cursor) { bt_cursor = pos; bt_redraw |= BT_BOX; }
    if (a) {
      if (bt_cursor == MENU_FIGHT) {
        bt_state = BS_MOVES;
        bt_cursor = 0;
        bt_redraw |= BT_BOX;
      } else if (bt_cursor == MENU_CATCH) {
        throw_ball();
      } else {                                            // RUN: always works
        say(AFTER_END_RAN, "Got away safely!", "");
      }
    }
  } else if (bt_state == BS_MOVES) {
    int n = player_mon.num_moves;
    if (step != 0 && n > 0) { bt_cursor = (bt_cursor + step + n) % n; bt_redraw |= BT_BOX; }
    if (b) { bt_state = BS_MENU; bt_cursor = 0; bt_redraw |= BT_BOX; }
    else if (a) player_attack(bt_cursor);
  } else if (a) {                                         // BS_MESSAGE
    switch (bt_after) {
      case AFTER_MENU:
        bt_state = BS_MENU;
        bt_cursor = 0;
        bt_redraw |= BT_BOX;
        break;
      case AFTER_ENEMY:
        enemy_attack();
        break;
      case AFTER_WIN: {                                   // the wild one fainted
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
        bt_redraw |= BT_HUD;                              // new EXP number
        break;
      }
      case AFTER_LEVELUP:                                 // enough EXP? level up
        if (player_mon.level < MAX_LEVEL &&               // (repeats if EXP is
            player_mon.exp >= (unsigned short)exp_to_next(player_mon.level))
          level_up();                                     //  enough for several)
        else
          return BATTLE_WON;
        break;
      case AFTER_CATCH:
        catch_result();
        break;
      case AFTER_LOSE:                                    // your POKEMON fainted
        str_set(l1, BT_LINE, mon_name(&player_mon));
        str_add(l1, BT_LINE, " fainted!");
        say(AFTER_END_LOST, l1, "You rushed to the POKEMON CENTER!");
        break;
      case AFTER_END_WON:    return BATTLE_WON;
      case AFTER_END_RAN:    return BATTLE_RAN;
      case AFTER_END_CAUGHT: return BATTLE_CAUGHT;
      case AFTER_END_LOST:
        heal_party();                                     // healed at the CENTER
        return BATTLE_LOST;
      default:
        break;
    }
  }
  return BATTLE_GOING;
}
// END OF world.c
