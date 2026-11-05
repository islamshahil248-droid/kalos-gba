// main.c - starts the game and runs the main loop (input, A button,
// entering/leaving buildings, talking, wild battles, the START menu).
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
    int moved, door, npc, a_press, warp, wild, speed;
    unsigned short pressed;
    wait_vblank();
    frame++;
    play_frames++;                                  // the clock on the Trainer card
    keys = (unsigned short)(~REG_KEYINPUT) & 0x03FF;

    rng_stir(frame ^ ((unsigned int)keys << 16));   // randomness for encounters

    // A counts only on the press (not while held). `pressed` holds every
    // button that went down this frame (the battle menus use it).
    pressed = (unsigned short)(keys & ~prev_keys);
    a_press = (pressed & KEY_A) != 0;

    // While the battle screen is showing, the battle rules (world.c) get the
    // buttons and nothing else moves. (START does nothing here: the party
    // screen inside the battle menu is how you switch.) When the battle ends
    // we go back to the map - or to the POKEMON CENTER if every POKEMON fainted.
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

    // While a menu screen is open, world.c's ui_input() gets the buttons and
    // nothing else moves. When the menu closes, repaint the map.
    if (ui_screen != UI_NONE) {
      prev_keys = keys;
      if (ui_input(pressed)) {
        show_map(x, y, dir);
      } else if (ui_redraw) {
        ui_draw(x, y, dir);
      }
#if DEBUG_HUD
      show_lamps(frame, keys, 0, 0);
#endif
      continue;
    }

    // START opens the main menu (only here: not in a battle, not in a dialogue).
    if (pressed & KEY_START) {
      prev_keys = keys;
      walk_frame = 0;                   // stand still behind the menu
      walk_timer = 0;
      ui_open_menu();
      ui_draw(x, y, dir);
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
      if (npcs[npc].heals) heal_party();   // the nurse heals your whole party
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
    // (Options > WALK SPEED = FAST takes one more pixel per frame.)
    speed = opt_fast ? SPEED_FAST : SPEED;
    for (n = 0; n < speed; n++) {
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

    // A new step on tall grass may start a wild encounter (unless the
    // player switched wild POKEMON off in Options).
    wild = (moved && opt_encounters) ? step_encounter(x, y) : -1;
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
