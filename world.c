// world.c - the rules: turning maps into tiles, collision, doors, finding
// the person in front of you.
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
// END OF world.c

