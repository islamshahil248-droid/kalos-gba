// main.c - starts the game and runs the main loop (input, A button,
// entering/leaving buildings, talking, wild battles).
#include "game.h"

// Switch to another room: build its pictures, put the trainer on the landing
// tile, and repaint the whole screen once. Used by doors and by warps.
static void change_room(int room, int sx, int sy, int sdir,
                        int* x, int* y, int* dir) {
  cur_room = room;
  rooms[cur_room].build();          // this room's pictures
  pick_start(sx, sy, x, y);
  *dir = sdir;
  walk_frame = 0;
  walk_timer = 0;
  steps_reset(*x, *y);              // arriving is not a step; no free encounter
  show_map(*x, *y, *dir);
}

int main(void) {
  int x, y;
  int dir = DIR_UP;
  int n, m;
  unsigned int frame = 0;
  unsigned short prev_keys = 0;

  REG_DISPCNT = 0x0080;   // forced blank while we build everything
  for (m = 0; m < NUM_ROOMS; m++) build_map(m);
  build_sprites();
  cur_room = R_TOWN;
  rooms[cur_room].build();
  pick_start(START_TX, START_TY, &x, &y);
  steps_reset(x, y);
  show_map(x, y, dir);

#if DEBUG_HUD
  lamp(0, 1, RGB15(0, 31, 0));
#endif

  while (1) {
    unsigned short keys;
    int old_x = x, old_y = y, old_dir = dir;
    int old_walk_frame = walk_frame;
    int dx = 0, dy = 0;
    int moved, door, npc, a_press, warp, wild;
    unsigned short pressed;
    wait_vblank();
    frame++;
    keys = (unsigned short)(~REG_KEYINPUT) & 0x03FF;

    rng_stir(frame ^ ((unsigned int)keys << 16));   // randomness for encounters

    // A counts only on the press (not while held). `pressed` holds every
    // button that went down this frame (the battle menus use it).
    pressed = (unsigned short)(keys & ~prev_keys);
    a_press = (pressed & KEY_A) != 0;

    // While the battle screen is showing, the battle rules (world.c) get the
    // buttons and nothing else moves. When the battle ends we go back to the
    // map - or to the POKEMON CENTER if PIKACHU fainted.
    if (battle_open) {
      int result;
      prev_keys = keys;
      result = battle_input(pressed);
      if (result == BATTLE_GOING) {
        if (bt_redraw) battle_redraw();
      } else {
        battle_close();
        if (result == BATTLE_LOST)
          change_room(BLACKOUT_ROOM, BLACKOUT_TX, BLACKOUT_TY, DIR_UP,
                      &x, &y, &dir);
        else
          show_map(x, y, dir);
      }
#if DEBUG_HUD
      show_lamps(frame, keys, 0, 0);
#endif
      continue;
    }

    // While a dialogue box is open, A flips the page and nothing else moves.
    if (dlg_open) {
      prev_keys = keys;
      if (a_press) dialog_advance(x, y, dir);
#if DEBUG_HUD
      show_lamps(frame, keys, 0, 0);
#endif
      continue;
    }

    // A button: a door in front of us first, then a person.
    door = a_press ? front_door(x, y, dir) : -1;
    npc  = (a_press && door < 0) ? front_npc(x, y, dir) : -1;
    if (npc >= 0) {
      prev_keys = keys;
      walk_frame = 0;                   // stand still while talking
      walk_timer = 0;
      paint_box(x, y, x + TILE, y + TILE, x, y, dir);
      if (npcs[npc].heals) heal_party();   // the nurse heals PIKACHU
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

    // One pixel at a time; X and Y separately so you slide along walls.
    for (n = 0; n < SPEED; n++) {
      if (dx != 0 && can_stand(x + dx, y)) x += dx;
      if (dy != 0 && can_stand(x, y + dy)) y += dy;
    }

    moved = (x != old_x || y != old_y);

    // Walking onto an exit tile (town <-> Route 1) changes the room.
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

    // Repaint only one box covering the old and new trainer position.
    if (moved || dir != old_dir || walk_frame != old_walk_frame) {
      int left   = (x < old_x) ? x : old_x;
      int top    = (y < old_y) ? y : old_y;
      int right  = ((x > old_x) ? x : old_x) + TILE;
      int bottom = ((y > old_y) ? y : old_y) + TILE;
      paint_box(left, top, right, bottom, x, y, dir);
    }

    // A new step on tall grass may start a wild encounter.
    wild = moved ? step_encounter(x, y) : -1;
    if (wild >= 0) {
      walk_frame = 0;
      walk_timer = 0;
      battle_begin(wild);               // make the wild POKEMON (sets wild_species)
      battle_screen_open();             // and show the battle screen
      continue;
    }

#if DEBUG_HUD
    show_lamps(frame, keys, moved, ((dx != 0 || dy != 0) && !moved));
#endif
  }
}
// END OF FILE
