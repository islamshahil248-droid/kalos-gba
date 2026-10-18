// main.c - starts the game and runs the main loop (input, A button,
// entering/leaving buildings, talking).
#include "game.h"

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
  show_map(x, y, dir);

#if DEBUG_HUD
  lamp(0, 1, RGB15(0, 31, 0));
#endif

  while (1) {
    unsigned short keys;
    int old_x = x, old_y = y, old_dir = dir;
    int old_walk_frame = walk_frame;
    int dx = 0, dy = 0;
    int moved, door, npc, a_press;
    wait_vblank();
    frame++;
    keys = (unsigned short)(~REG_KEYINPUT) & 0x03FF;

    // A counts only on the press (not while held).
    a_press = (keys & KEY_A) && !(prev_keys & KEY_A);

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
      dialog_open(npc);
      continue;
    }
    if (door >= 0) {
      cur_room = doors[door].to;
      rooms[cur_room].build();          // this room's pictures
      pick_start(doors[door].sx, doors[door].sy, &x, &y);
      dir = doors[door].sdir;
      walk_frame = 0;
      walk_timer = 0;
      show_map(x, y, dir);
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

#if DEBUG_HUD
    show_lamps(frame, keys, moved, ((dx != 0 || dy != 0) && !moved));
#endif
  }
}
// END OF FILE
