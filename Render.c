// render.c - drawing: the screen painter, VBlank wait, the dialogue box and
// the debug lamps.
#include "game.h"

// Paint a screen rectangle (right/bottom not included). Each pixel's final
// color (map, then trainer on top) is worked out first and written ONCE,
// so the trainer is never painted over and nothing is erased first.
void paint_box(int left, int top, int right, int bottom,
                      int px, int py, int dir) {
  int sx, sy;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > SCREEN_W) right = SCREEN_W;
  if (bottom > SCREEN_H) bottom = SCREEN_H;

  for (sy = top; sy < bottom; sy++) {
    volatile unsigned short* row = VRAM + sy * SCREEN_W;
    int in_rows = (sy >= py && sy < py + TILE);
    for (sx = left; sx < right; sx++) {
      unsigned short color =
          tile_gfx[map_kind[cur_room][sy >> 4][sx >> 4]][((sy & 15) << 4) | (sx & 15)];
      if (in_rows && sx >= px && sx < px + TILE) {
        unsigned short c = sprite_gfx[dir][walk_frame][(sy - py) * TILE + (sx - px)];
        if (c != CLEAR) color = c;
      }
      row[sx] = color;
    }
  }
}

void wait_vblank(void) {
  while (REG_VCOUNT >= 160) {}
  while (REG_VCOUNT < 160) {}
}

// Full-screen repaint (start, entering, leaving). Mode 3 has ONE framebuffer,
// so a long repaint would be seen half-finished. Forced blank hides it.
void show_map(int x, int y, int dir) {
  wait_vblank();
  REG_DISPCNT = 0x0080;
  paint_box(0, 0, SCREEN_W, SCREEN_H, x, y, dir);
  REG_DISPCNT = 0x0403;
}
// ---------- Dialogue box ----------
#define DLG_X      8        // the box: 224 x 40 pixels at the bottom
#define DLG_Y      112
#define DLG_W      224
#define DLG_H      40
#define DLG_COLS   34       // characters per line (6 pixels each = 204)
#define DLG_LINES  2        // lines per page
#define DLG_PAPER  RGB15(31, 31, 29)
#define DLG_INK    RGB15(3, 3, 6)
#define DLG_SHADOW RGB15(20, 20, 23)
int dlg_open = 0;                    // 1 while the box is on screen
static const char* dlg_text = 0;            // the whole text being shown (set when a box opens)
static int dlg_pos = 0;                     // where the current page starts
static int dlg_next = 0;                    // where the next page starts
static char dlg_lines[DLG_LINES][DLG_COLS + 1];

// Work out the current page: fill dlg_lines (word-wrapped) and find where
// the next page starts. Never writes past DLG_COLS characters per line.
static void dlg_layout(void) {
  int p = dlg_pos, line, col, len, k;
  for (line = 0; line < DLG_LINES; line++) dlg_lines[line][0] = '\0';
  line = 0;
  while (line < DLG_LINES) {
    while (dlg_text[p] == ' ') p++;                  // no spaces at the start of a line
    if (dlg_text[p] == '\0') break;                  // end of the text
    if (dlg_text[p] == '|') {                        // forced new page
      p++;
      if (line > 0) break;
      continue;
    }
    col = 0;
    while (dlg_text[p] != '\0' && dlg_text[p] != '|') {
      len = 0;
      while (dlg_text[p + len] != ' ' && dlg_text[p + len] != '\0' &&
             dlg_text[p + len] != '|') len++;
      if (col > 0 && col + 1 + len > DLG_COLS) break;   // word does not fit: next line
      if (col > 0) dlg_lines[line][col++] = ' ';
      if (len > DLG_COLS - col) len = DLG_COLS - col;   // a giant word is cut
      for (k = 0; k < len; k++) dlg_lines[line][col++] = dlg_text[p + k];
      p += len;
      while (dlg_text[p] == ' ') p++;
      if (col >= DLG_COLS) break;
    }
    dlg_lines[line][col] = '\0';
    line++;
  }
  dlg_next = p;
}

// Is there another page after the current one?
static int dlg_has_more(void) {
  int p = dlg_next;
  while (dlg_text[p] == ' ' || dlg_text[p] == '|') p++;
  return dlg_text[p] != '\0';
}

// Fill a rectangle of the screen (x1/y1 not included). Callers keep it on screen.
static void fill_rect(int x0, int y0, int x1, int y1, unsigned short c) {
  int x, y;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++)
      VRAM[y * SCREEN_W + x] = c;
}

// Draw one letter's lit pixels.
static void draw_glyph(int x, int y, char ch, unsigned short color) {
  int col, row;
  const unsigned char* g;
  if (ch < ' ' || ch > 'z') ch = '?';
  g = font5x8[ch - ' '];
  for (col = 0; col < 5; col++)
    for (row = 0; row < 8; row++)
      if (g[col] & (1 << row))
        VRAM[(y + row) * SCREEN_W + x + col] = color;
}

// A line of text with a soft shadow (shadow first, so it never covers letters).
static void draw_line(int x, int y, const char* str) {
  int i;
  for (i = 0; str[i]; i++) draw_glyph(x + i * 6 + 1, y + 1, str[i], DLG_SHADOW);
  for (i = 0; str[i]; i++) draw_glyph(x + i * 6,     y,     str[i], DLG_INK);
}

// Redraw the text area of the box for the current page.
static void draw_dialog_text(void) {
  int i, ax = DLG_X + DLG_W - 14, ay = DLG_Y + DLG_H - 10;
  fill_rect(DLG_X + 4, DLG_Y + 4, DLG_X + DLG_W - 4, DLG_Y + DLG_H - 4, DLG_PAPER);
  for (i = 0; i < DLG_LINES; i++)
    draw_line(DLG_X + 10, DLG_Y + 8 + i * 13, dlg_lines[i]);
  if (dlg_has_more()) {                              // little "more" arrow
    fill_rect(ax,     ay,     ax + 7, ay + 1, DLG_INK);
    fill_rect(ax + 1, ay + 1, ax + 6, ay + 2, DLG_INK);
    fill_rect(ax + 2, ay + 2, ax + 5, ay + 3, DLG_INK);
    fill_rect(ax + 3, ay + 3, ax + 4, ay + 4, DLG_INK);
  }
}

// Start talking to person number n.
void dialog_open(int n) {
  dlg_text = npcs[n].text;
  dlg_pos = 0;
  dlg_layout();
  fill_rect(DLG_X, DLG_Y, DLG_X + DLG_W, DLG_Y + DLG_H, OUTLINE);
  fill_rect(DLG_X + 1, DLG_Y + 1, DLG_X + DLG_W - 1, DLG_Y + DLG_H - 1,
            RGB15(12, 16, 26));
  draw_dialog_text();
  dlg_open = 1;
}

// A pressed while talking: next page, or close the box and repaint what was
// under it (the map, with the trainer on top).
void dialog_advance(int x, int y, int dir) {
  if (dlg_has_more()) {
    dlg_pos = dlg_next;
    dlg_layout();
    draw_dialog_text();
  } else {
    dlg_open = 0;
    walk_frame = 0;
    walk_timer = 0;
    paint_box(DLG_X, DLG_Y, DLG_X + DLG_W, DLG_Y + DLG_H, x, y, dir);
  }
}

#if DEBUG_HUD
// Lamps live in the top row (always solid), where the trainer can't walk.
void lamp(int slot, int on, unsigned short color) {
  int lx = 2 + slot * 8;
  int x, y;
  for (y = 2; y < 8; y++)
    for (x = lx; x < lx + 6; x++)
      VRAM[y * SCREEN_W + x] = on ? color : RGB15(4, 4, 4);
}

// 0 blinks = loop runs; 1-4 = LEFT/RIGHT/UP/DOWN; 5 = moved; 6 = blocked;
// 7 (orange) = a dialogue box is open
void show_lamps(unsigned int frame, unsigned short keys,
                       int moved, int blocked) {
  lamp(0, (frame >> 5) & 1, RGB15(31, 0, 31));
  lamp(1, keys & KEY_LEFT,  RGB15(0, 31, 31));
  lamp(2, keys & KEY_RIGHT, RGB15(0, 31, 31));
  lamp(3, keys & KEY_UP,    RGB15(0, 31, 31));
  lamp(4, keys & KEY_DOWN,  RGB15(0, 31, 31));
  lamp(5, moved,            RGB15(31, 31, 31));
  lamp(6, blocked,          RGB15(31, 0, 0));
  lamp(7, dlg_open,         RGB15(31, 20, 0));
}
#endif
// END OF render.c
