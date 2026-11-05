// render.c - drawing: the screen painter, VBlank wait, the dialogue box, the
// battle screen, the party screen, the START menu screens and the debug lamps.
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

// A line of text in any color, with a soft shadow (shadow first, so it never
// covers letters). Every letter is 6 pixels wide.
static void draw_line_color(int x, int y, const char* str, unsigned short color) {
  int i;
  for (i = 0; str[i]; i++) draw_glyph(x + i * 6 + 1, y + 1, str[i], DLG_SHADOW);
  for (i = 0; str[i]; i++) draw_glyph(x + i * 6,     y,     str[i], color);
}

// A line of normal (dark) text.
static void draw_line(int x, int y, const char* str) {
  draw_line_color(x, y, str, DLG_INK);
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

// Draw the box with the first page of a text (used by people and encounters).
static void dlg_show(const char* text) {
  dlg_text = text;
  dlg_pos = 0;
  dlg_layout();
  fill_rect(DLG_X, DLG_Y, DLG_X + DLG_W, DLG_Y + DLG_H, OUTLINE);
  fill_rect(DLG_X + 1, DLG_Y + 1, DLG_X + DLG_W - 1, DLG_Y + DLG_H - 1,
            RGB15(12, 16, 26));
  draw_dialog_text();
}

// Start talking to person number n.
void dialog_open(int n) {
  dlg_show(npcs[n].text);
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

// ---------- Battle screen ----------
// Layout (240x160): the wild POKEMON's panel top-left and its picture top-right;
// your fighting POKEMON's picture bottom-left and its panel bottom-right; the
// message / menu box at the very bottom (same place as the dialogue box).
// The whole scene is drawn once under forced blank (like show_map). After that
// only the parts that changed are drawn again (battle_redraw), so it is quick.
// world.c changes the numbers and text; this file only draws them.
int battle_open = 0;                   // 1 while the battle screen is showing

#define PANEL_BORDER RGB15(12, 16, 26)
#define PANEL_W      120

// The 8x8 placeholder picture of species sp at 4x size (32x32 pixels).
static void draw_species(int sp, int x0, int y0) {
  const Species* s = &species[sp];
  int cx, cy;
  for (cy = 0; cy < 8; cy++) {
    for (cx = 0; cx < 8; cx++) {
      char c = s->art[cy * 8 + cx];
      int x = x0 + cx * 4, y = y0 + cy * 4;
      if (c == 'B')      fill_rect(x, y, x + 4, y + 4, s->body);
      else if (c == 'A') fill_rect(x, y, x + 4, y + 4, s->accent);
      else if (c == 'K') fill_rect(x, y, x + 4, y + 4, OUTLINE);
      else if (c == 'Y') fill_rect(x, y, x + 4, y + 4, RGB15(31, 22, 5));
      else if (c == 'E') {
        fill_rect(x, y, x + 4, y + 4, WHITE);
        fill_rect(x + 1, y + 1, x + 3, y + 3, OUTLINE);
      }
    }
  }
}

// Sky, grass, two platforms and the two POKEMON pictures.
static void draw_battle_scene(void) {
  fill_rect(0, 0, SCREEN_W, 28, RGB15(17, 25, 31));              // sky
  fill_rect(0, 28, SCREEN_W, 64, RGB15(22, 29, 31));
  fill_rect(0, 64, SCREEN_W, SCREEN_H, RGB15(11, 24, 8));        // grass
  fill_rect(0, 82, SCREEN_W, 84, RGB15(13, 27, 10));
  fill_rect(0, 98, SCREEN_W, 100, RGB15(13, 27, 10));
  fill_rect(140, 66, 212, 68, RGB15(14, 29, 11));                // their platform
  fill_rect(134, 68, 218, 72, RGB15(9, 20, 7));
  fill_rect(142, 72, 210, 74, RGB15(6, 14, 5));
  fill_rect(20, 102, 92, 105, RGB15(14, 29, 11));                // our platform
  fill_rect(12, 105, 100, 109, RGB15(9, 20, 7));
  fill_rect(22, 109, 90, 111, RGB15(6, 14, 5));
  draw_species(wild_mon.species, 160, 32);
  draw_species(player_mon.species, 36, 70);
}

// A framed white panel.
static void draw_panel(int x, int y, int w, int h) {
  fill_rect(x, y, x + w, y + h, OUTLINE);
  fill_rect(x + 1, y + 1, x + w - 1, y + h - 1, PANEL_BORDER);
  fill_rect(x + 3, y + 3, x + w - 3, y + h - 3, DLG_PAPER);
}

// HP bar w pixels wide: green above half, yellow above a fifth, then red.
static void draw_hp_bar(int x, int y, int w, int hp, int max_hp) {
  int fill = (max_hp > 0) ? hp * w / max_hp : 0;
  unsigned short col;
  if (hp > 0 && fill < 1) fill = 1;          // a sliver while still alive
  if (fill > w) fill = w;
  if (hp * 100 >= max_hp * 50)      col = RGB15(6, 26, 8);
  else if (hp * 100 >= max_hp * 20) col = RGB15(30, 25, 3);
  else                              col = RGB15(30, 6, 4);
  fill_rect(x - 1, y - 1, x + w + 1, y + 6, OUTLINE);
  fill_rect(x, y, x + w, y + 5, RGB15(9, 9, 11));
  if (fill > 0) fill_rect(x, y, x + fill, y + 5, col);
}

// Name, level, HP bar and HP numbers (your POKEMON also shows its EXP).
static void draw_mon_panel(const Mon* m, int x, int y, int is_player) {
  char buf[16];
  draw_panel(x, y, PANEL_W, is_player ? 37 : 27);
  draw_line(x + 6, y + 4, species[m->species].name);
  str_set(buf, (int)sizeof(buf), "Lv");
  str_add_num(buf, (int)sizeof(buf), m->level);
  draw_line(x + 88, y + 4, buf);
  draw_line(x + 6, y + 14, "HP");
  draw_hp_bar(x + 24, y + 16, 54, m->hp, m->max_hp);
  str_set(buf, (int)sizeof(buf), "");
  str_add_num(buf, (int)sizeof(buf), m->hp);
  str_add(buf, (int)sizeof(buf), "/");
  str_add_num(buf, (int)sizeof(buf), m->max_hp);
  draw_line(x + 84, y + 14, buf);
  if (is_player) {
    str_set(buf, (int)sizeof(buf), "EXP ");
    if (m->level >= MAX_LEVEL) {
      str_add(buf, (int)sizeof(buf), "MAX");
    } else {                                   // progress toward the next level
      str_add_num(buf, (int)sizeof(buf), m->exp);
      str_add(buf, (int)sizeof(buf), "/");
      str_add_num(buf, (int)sizeof(buf), exp_to_next(m->level));
    }
    draw_line(x + 6, y + 25, buf);
  }
}

// Little arrow pointing at the chosen menu line (4 wide, 7 tall).
static void draw_cursor(int x, int y) {
  int i;
  for (i = 0; i < 7; i++)
    fill_rect(x, y + i, x + ((i < 4) ? i + 1 : 7 - i), y + i + 1, DLG_INK);
}

// "Press A" arrow in the corner of the box (same shape as the dialogue one).
static void draw_more_arrow(void) {
  int ax = DLG_X + DLG_W - 14, ay = DLG_Y + DLG_H - 10;
  fill_rect(ax,     ay,     ax + 7, ay + 1, DLG_INK);
  fill_rect(ax + 1, ay + 1, ax + 6, ay + 2, DLG_INK);
  fill_rect(ax + 2, ay + 2, ax + 5, ay + 3, DLG_INK);
  fill_rect(ax + 3, ay + 3, ax + 4, ay + 4, DLG_INK);
}

// A small framed area inside the box (for the menu lists).
static void draw_subbox(int x0) {
  fill_rect(DLG_X + x0, DLG_Y + 5, DLG_X + DLG_W - 5, DLG_Y + DLG_H - 5, OUTLINE);
  fill_rect(DLG_X + x0 + 1, DLG_Y + 6, DLG_X + DLG_W - 6, DLG_Y + DLG_H - 6, DLG_PAPER);
}

// The box at the bottom: a message, the battle menu, or the move list.
static void draw_battle_box(void) {
  char buf[BT_LINE];
  int i;
  fill_rect(DLG_X, DLG_Y, DLG_X + DLG_W, DLG_Y + DLG_H, OUTLINE);
  fill_rect(DLG_X + 1, DLG_Y + 1, DLG_X + DLG_W - 1, DLG_Y + DLG_H - 1, PANEL_BORDER);
  fill_rect(DLG_X + 4, DLG_Y + 4, DLG_X + DLG_W - 4, DLG_Y + DLG_H - 4, DLG_PAPER);
  if (bt_state == BS_MESSAGE) {
    draw_line(DLG_X + 10, DLG_Y + 8, bt_line1);
    draw_line(DLG_X + 10, DLG_Y + 21, bt_line2);
    draw_more_arrow();
  } else if (bt_state == BS_MENU) {
    draw_line(DLG_X + 10, DLG_Y + 8, "What will");
    str_set(buf, BT_LINE, species[player_mon.species].name);
    str_add(buf, BT_LINE, " do?");
    draw_line(DLG_X + 10, DLG_Y + 21, buf);
    draw_subbox(104);
    // Two columns, two rows (bt_cursor 0..3):  FIGHT  CATCH xN
    //                                          RUN    POKEMON
    draw_line(DLG_X + 123, DLG_Y + 8, "FIGHT");
    str_set(buf, BT_LINE, "CATCH x");
    str_add_num(buf, BT_LINE, pokeballs);
    draw_line(DLG_X + 167, DLG_Y + 8, buf);
    draw_line(DLG_X + 123, DLG_Y + 21, "RUN");
    draw_line(DLG_X + 167, DLG_Y + 21, "POKEMON");
    draw_cursor(DLG_X + 113 + (bt_cursor & 1) * 44, DLG_Y + 9 + (bt_cursor >> 1) * 13);
  } else {                                           // BS_MOVES
    for (i = 0; i < player_mon.num_moves; i++)
      draw_line(DLG_X + 26, DLG_Y + 8 + i * 13, moves[player_mon.move[i]].name);
    draw_cursor(DLG_X + 14, DLG_Y + 9 + bt_cursor * 13);
    draw_subbox(140);
    draw_line(DLG_X + 148, DLG_Y + 8, "B: Back");
    str_set(buf, BT_LINE, "Power ");
    str_add_num(buf, BT_LINE, moves[player_mon.move[bt_cursor]].power);
    draw_line(DLG_X + 148, DLG_Y + 21, buf);
  }
}

// ---------- Full-screen pages (party, menu screens) ----------
// All of them: a light blue background, a header panel at the top (below the
// debug lamps), content, and a one-line footer panel at the bottom.
#define UI_BG        RGB15(17, 22, 28)
#define UI_SEL_BG    RGB15(31, 30, 18)   // the chosen row
#define UI_SEL_EDGE  RGB15(31, 18, 2)
#define UI_FAINT_BG  RGB15(31, 24, 24)   // a fainted POKEMON's row
#define UI_RED       RGB15(24, 2, 2)
#define UI_BLUE      RGB15(3, 8, 22)
#define UI_EMPTY_BG  RGB15(21, 26, 30)   // an empty party slot
#define UI_EMPTY_EDGE RGB15(13, 18, 25)
#define PG_X   6                         // pages are 228 pixels wide
#define PG_W   228
#define HDR_Y  10                        // header panel: 16 pixels tall
#define HDR_H  16
#define ROW_Y  28                        // first list row
#define ROW_PITCH 19                     // distance between rows
#define ROW_H  17                        // height of one row
#define FTR_Y  142                       // footer panel: 18 pixels tall
#define FTR_H  18

// Text whose RIGHT end is at right_x.
static void draw_line_right(int right_x, int y, const char* str, unsigned short color) {
  int len = 0;
  while (str[len]) len++;
  draw_line_color(right_x - len * 6, y, str, color);
}

// "n" written with at least two digits (for the clock).
static void add_num2(char* dst, int size, int v) {
  if (v < 10) str_add(dst, size, "0");
  str_add_num(dst, size, v);
}

// Header panel with a title on the left and (optional) text on the right.
static void draw_header(const char* title, const char* right) {
  draw_panel(PG_X, HDR_Y, PG_W, HDR_H);
  draw_line(PG_X + 8, HDR_Y + 4, title);
  if (right) draw_line_right(PG_X + PG_W - 8, HDR_Y + 4, right, DLG_INK);
}

// Footer panel with one line of text.
static void draw_footer(const char* text) {
  draw_panel(PG_X, FTR_Y, PG_W, FTR_H);
  draw_line(PG_X + 8, FTR_Y + 5, text);
}

// A framed row (the chosen one gets an orange frame).
static void draw_row_frame(int x, int y, int w, int h, int selected, unsigned short bg) {
  fill_rect(x, y, x + w, y + h, OUTLINE);
  fill_rect(x + 1, y + 1, x + w - 1, y + h - 1, selected ? UI_SEL_EDGE : PANEL_BORDER);
  fill_rect(x + 2, y + 2, x + w - 2, y + h - 2, bg);
}

// ---- The party screen ----
// One row: name, level, HP bar, HP numbers (or FAINTED), and ACTIVE for the
// POKEMON that is fighting / leading. Empty slots are drawn dim.
static void draw_party_row(int i) {
  int y = ROW_Y + i * ROW_PITCH;
  char buf[16];
  const Mon* m;
  int fainted, selected;
  unsigned short bg;
  if (i >= party_count) {
    fill_rect(PG_X, y, PG_X + PG_W, y + ROW_H, UI_EMPTY_EDGE);
    fill_rect(PG_X + 1, y + 1, PG_X + PG_W - 1, y + ROW_H - 1, UI_EMPTY_BG);
    return;
  }
  m = &party[i];
  fainted = (m->hp <= 0);
  selected = (i == pt_cursor);
  bg = fainted ? UI_FAINT_BG : (selected ? UI_SEL_BG : DLG_PAPER);
  draw_row_frame(PG_X, y, PG_W, ROW_H, selected, bg);
  if (selected) draw_cursor(PG_X + 6, y + 5);
  draw_line(PG_X + 14, y + 5, species[m->species].name);
  str_set(buf, (int)sizeof(buf), "Lv");
  str_add_num(buf, (int)sizeof(buf), m->level);
  draw_line(PG_X + 66, y + 5, buf);
  draw_hp_bar(PG_X + 94, y + 6, 36, m->hp, m->max_hp);
  if (fainted) {
    draw_line_color(PG_X + 136, y + 5, "FAINTED", UI_RED);
  } else {
    str_set(buf, (int)sizeof(buf), "");
    str_add_num(buf, (int)sizeof(buf), m->hp);
    str_add(buf, (int)sizeof(buf), "/");
    str_add_num(buf, (int)sizeof(buf), m->max_hp);
    draw_line(PG_X + 136, y + 5, buf);
  }
  if (i == active_idx) draw_line_color(PG_X + 184, y + 5, "ACTIVE", UI_BLUE);
}

// The six rows and the bottom line (this is all that changes when you move).
static void draw_party_body(void) {
  int i;
  const char* hint;
  for (i = 0; i < PARTY_MAX; i++) draw_party_row(i);
  if (pt_note[0])           hint = pt_note;
  else if (!battle_open)    hint = "A: Summary  B: Back";
  else if (pt_forced)       hint = "Choose a POKEMON to send out.";
  else                      hint = "A: Send out  B: Back";
  draw_footer(hint);
}

static void draw_party_page(void) {
  char buf[8];
  const char* title = "POKEMON";
  if (battle_open) title = pt_forced ? "CHOOSE A POKEMON" : "SWITCH POKEMON";
  str_set(buf, (int)sizeof(buf), "");
  str_add_num(buf, (int)sizeof(buf), party_count);
  str_add(buf, (int)sizeof(buf), "/");
  str_add_num(buf, (int)sizeof(buf), PARTY_MAX);
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  draw_header(title, buf);
  draw_party_body();
}

// ---- Summary of one POKEMON ----
static void draw_summary_page(void) {
  const Mon* m = &party[pt_cursor];
  char buf[24];
  int i, y;
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  str_set(buf, (int)sizeof(buf), "Lv");
  str_add_num(buf, (int)sizeof(buf), m->level);
  draw_header(species[m->species].name, buf);

  draw_panel(PG_X, ROW_Y, 88, 110);                 // left: picture and numbers
  draw_species(m->species, PG_X + 28, ROW_Y + 6);
  draw_line(PG_X + 8, ROW_Y + 44, "HP");
  draw_hp_bar(PG_X + 26, ROW_Y + 46, 54, m->hp, m->max_hp);
  if (m->hp <= 0) {
    draw_line_color(PG_X + 8, ROW_Y + 56, "FAINTED", UI_RED);
  } else {
    str_set(buf, (int)sizeof(buf), "");
    str_add_num(buf, (int)sizeof(buf), m->hp);
    str_add(buf, (int)sizeof(buf), "/");
    str_add_num(buf, (int)sizeof(buf), m->max_hp);
    draw_line(PG_X + 8, ROW_Y + 56, buf);
  }
  str_set(buf, (int)sizeof(buf), "EXP ");
  if (m->level >= MAX_LEVEL) {
    str_add(buf, (int)sizeof(buf), "MAX");
  } else {
    str_add_num(buf, (int)sizeof(buf), m->exp);
    str_add(buf, (int)sizeof(buf), "/");
    str_add_num(buf, (int)sizeof(buf), exp_to_next(m->level));
  }
  draw_line(PG_X + 8, ROW_Y + 70, buf);
  if (pt_cursor == active_idx) draw_line_color(PG_X + 8, ROW_Y + 84, "ACTIVE", UI_BLUE);
  str_set(buf, (int)sizeof(buf), "");
  str_add_num(buf, (int)sizeof(buf), pt_cursor + 1);
  str_add(buf, (int)sizeof(buf), "/");
  str_add_num(buf, (int)sizeof(buf), party_count);
  draw_line_right(PG_X + 80, ROW_Y + 96, buf, DLG_INK);

  draw_panel(PG_X + 92, ROW_Y, PG_W - 92, 110);     // right: the moves
  draw_line(PG_X + 102, ROW_Y + 6, "MOVES");
  for (i = 0; i < m->num_moves && i < MAX_MOVES; i++) {
    y = ROW_Y + 20 + i * 22;
    draw_cursor(PG_X + 100, y + 1);
    draw_line(PG_X + 112, y, moves[m->move[i]].name);
    str_set(buf, (int)sizeof(buf), "Power ");
    str_add_num(buf, (int)sizeof(buf), moves[m->move[i]].power);
    draw_line(PG_X + 112, y + 10, buf);
  }
  draw_footer(pt_note[0] ? pt_note : "A: Make lead  B: Back");
}

// ---- Pokedex ----
static int count_flags(const unsigned char* f) {
  int i, n = 0;
  for (i = 0; i < NUM_SPECIES; i++) if (f[i]) n++;
  return n;
}

// Up to six species at a time; the list scrolls so the cursor stays visible.
static void draw_dex_body(void) {
  int top = (ui_cursor >= PARTY_MAX) ? ui_cursor - (PARTY_MAX - 1) : 0;
  int i, sp, y;
  char buf[16];
  for (i = 0; i < PARTY_MAX; i++) {
    sp = top + i;
    y = ROW_Y + i * ROW_PITCH;
    if (sp >= NUM_SPECIES) {
      fill_rect(PG_X, y, PG_X + PG_W, y + ROW_H, UI_EMPTY_EDGE);
      fill_rect(PG_X + 1, y + 1, PG_X + PG_W - 1, y + ROW_H - 1, UI_EMPTY_BG);
      continue;
    }
    draw_row_frame(PG_X, y, PG_W, ROW_H, sp == ui_cursor,
                   (sp == ui_cursor) ? UI_SEL_BG : DLG_PAPER);
    if (sp == ui_cursor) draw_cursor(PG_X + 6, y + 5);
    str_set(buf, (int)sizeof(buf), "No.");
    str_add_num(buf, (int)sizeof(buf), sp + 1);
    draw_line(PG_X + 14, y + 5, buf);
    draw_line(PG_X + 56, y + 5, dex_seen[sp] ? species[sp].name : "?????");
    if (dex_caught[sp])    draw_line_right(PG_X + PG_W - 8, y + 5, "CAUGHT", UI_BLUE);
    else if (dex_seen[sp]) draw_line_right(PG_X + PG_W - 8, y + 5, "SEEN", DLG_INK);
    else                   draw_line_right(PG_X + PG_W - 8, y + 5, "---", DLG_INK);
  }
}

static void draw_dex_page(void) {
  char buf[24];
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  str_set(buf, (int)sizeof(buf), "Seen ");
  str_add_num(buf, (int)sizeof(buf), count_flags(dex_seen));
  str_add(buf, (int)sizeof(buf), "  Caught ");
  str_add_num(buf, (int)sizeof(buf), count_flags(dex_caught));
  draw_header("POKEDEX", buf);
  draw_dex_body();
  draw_footer("Up/Down: Look  B: Back");
}

// ---- Bag ----
// A tiny Poke Ball picture (12x12).
static void draw_ball_icon(int x, int y) {
  fill_rect(x, y, x + 12, y + 12, OUTLINE);
  fill_rect(x + 1, y + 1, x + 11, y + 6, RGB15(27, 5, 7));
  fill_rect(x + 1, y + 6, x + 11, y + 11, WHITE);
  fill_rect(x + 1, y + 5, x + 11, y + 7, OUTLINE);
  fill_rect(x + 4, y + 4, x + 8, y + 8, OUTLINE);
  fill_rect(x + 5, y + 5, x + 7, y + 7, WHITE);
}

static void draw_bag_page(void) {
  char buf[16];
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  draw_header("BAG", 0);
  draw_panel(PG_X, ROW_Y, PG_W, 110);
  draw_ball_icon(PG_X + 10, ROW_Y + 10);
  draw_line(PG_X + 30, ROW_Y + 12, "POKE BALL");
  str_set(buf, (int)sizeof(buf), "x");
  str_add_num(buf, (int)sizeof(buf), pokeballs);
  draw_line_right(PG_X + PG_W - 12, ROW_Y + 12, buf, DLG_INK);
  draw_line(PG_X + 30, ROW_Y + 26, "Catches wild POKEMON in battle.");
  fill_rect(PG_X + 10, ROW_Y + 44, PG_X + PG_W - 10, ROW_Y + 45, UI_EMPTY_EDGE);
  draw_line(PG_X + 10, ROW_Y + 54, "No other items yet.");
  draw_footer("B: Back");
}

// ---- Trainer card ----
static const char* const room_names[NUM_ROOMS] = {
  "Town", "POKEMON CENTER", "POKE MART", "House", "House", "Route 1"
};

// One line "LABEL ........ value" of the trainer card.
static void draw_card_line(int row, const char* label, const char* value) {
  int y = ROW_Y + 8 + row * 13;
  draw_line(PG_X + 12, y, label);
  draw_line(PG_X + 100, y, value);
}

static void draw_trainer_page(void) {
  char buf[24];
  unsigned int secs = play_frames / 60u;
  unsigned int hours = secs / 3600u;
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  draw_header("TRAINER CARD", 0);
  draw_panel(PG_X, ROW_Y, PG_W, 110);
  draw_card_line(0, "NAME", TRAINER_NAME);
  str_set(buf, (int)sizeof(buf), "");
  str_add_num(buf, (int)sizeof(buf), party_count);
  str_add(buf, (int)sizeof(buf), "/");
  str_add_num(buf, (int)sizeof(buf), PARTY_MAX);
  draw_card_line(1, "POKEMON", buf);
  str_set(buf, (int)sizeof(buf), "Seen ");
  str_add_num(buf, (int)sizeof(buf), count_flags(dex_seen));
  str_add(buf, (int)sizeof(buf), " / Caught ");
  str_add_num(buf, (int)sizeof(buf), count_flags(dex_caught));
  draw_card_line(2, "POKEDEX", buf);
  str_set(buf, (int)sizeof(buf), "");
  str_add_num(buf, (int)sizeof(buf), pokeballs);
  draw_card_line(3, "POKE BALLS", buf);
  draw_card_line(4, "BADGES", "0");
  if (hours > 99u) hours = 99u;
  str_set(buf, (int)sizeof(buf), "");
  str_add_num(buf, (int)sizeof(buf), (int)hours);
  str_add(buf, (int)sizeof(buf), ":");
  add_num2(buf, (int)sizeof(buf), (int)((secs / 60u) % 60u));
  str_add(buf, (int)sizeof(buf), ":");
  add_num2(buf, (int)sizeof(buf), (int)(secs % 60u));
  draw_card_line(5, "TIME", buf);
  draw_card_line(6, "PLACE", room_names[cur_room]);
  draw_footer("B: Back");
}

// ---- Save ----
// There is no battery save in this game yet, so this screen says so honestly.
static void draw_save_page(void) {
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  draw_header("SAVE", 0);
  draw_panel(PG_X, ROW_Y, PG_W, 110);
  draw_line_color(PG_X + 12, ROW_Y + 12, "SAVING IS NOT AVAILABLE", UI_RED);
  draw_line(PG_X + 12, ROW_Y + 32, "This game has no backup memory");
  draw_line(PG_X + 12, ROW_Y + 44, "yet, so nothing can be saved.");
  draw_line(PG_X + 12, ROW_Y + 64, "Your progress is lost when you");
  draw_line(PG_X + 12, ROW_Y + 76, "turn the console off.");
  draw_footer("A or B: Back");
}

// ---- Options ----
static void draw_options_body(void) {
  int i, y;
  const char* label;
  const char* value;
  for (i = 0; i < NUM_OPT_LINES; i++) {
    y = ROW_Y + 8 + i * 22;
    if (i == OPT_SPEED)           { label = "WALK SPEED";      value = opt_fast ? "FAST" : "NORMAL"; }
    else if (i == OPT_ENCOUNTERS) { label = "WILD POKEMON";    value = opt_encounters ? "ON" : "OFF"; }
    else                          { label = "BACK";            value = 0; }
    draw_row_frame(PG_X + 8, y, PG_W - 16, ROW_H, i == ui_cursor,
                   (i == ui_cursor) ? UI_SEL_BG : DLG_PAPER);
    if (i == ui_cursor) draw_cursor(PG_X + 14, y + 5);
    draw_line(PG_X + 26, y + 5, label);
    if (value) draw_line_right(PG_X + PG_W - 20, y + 5, value, UI_BLUE);
  }
}

static void draw_options_page(void) {
  fill_rect(0, 0, SCREEN_W, SCREEN_H, UI_BG);
  draw_header("OPTIONS", 0);
  draw_panel(PG_X, ROW_Y, PG_W, 110);
  draw_options_body();
  draw_line(PG_X + 12, ROW_Y + 82, "Settings are not saved.");
  draw_line(PG_X + 12, ROW_Y + 94, "They reset when you power off.");
  draw_footer("A: Change  B: Back");
}

// ---- The main menu itself: a panel on top of the map ----
#define MM_X 156
#define MM_Y 4
#define MM_W 80
#define MM_PITCH 14

static void draw_main_menu(void) {
  int i;
  draw_panel(MM_X, MM_Y, MM_W, NUM_MENU_ENTRIES * MM_PITCH + 12);
  for (i = 0; i < NUM_MENU_ENTRIES; i++)
    draw_line(MM_X + 20, MM_Y + 8 + i * MM_PITCH, menu_labels[i]);
  draw_cursor(MM_X + 10, MM_Y + 8 + ui_menu_cursor * MM_PITCH);
}

// Draw the whole current screen (called under forced blank).
static void draw_ui_page(int x, int y, int dir) {
  switch (ui_screen) {
    case UI_MENU:
      paint_box(0, 0, SCREEN_W, SCREEN_H, x, y, dir);   // the map stays behind
      draw_main_menu();
      break;
    case UI_DEX:     draw_dex_page();     break;
    case UI_PARTY:   draw_party_page();   break;
    case UI_SUMMARY: draw_summary_page(); break;
    case UI_BAG:     draw_bag_page();     break;
    case UI_TRAINER: draw_trainer_page(); break;
    case UI_SAVE:    draw_save_page();    break;
    case UI_OPTIONS: draw_options_page(); break;
    default: break;
  }
}

// main calls this after ui_input() whenever ui_redraw is not 0.
// UIR_FULL = whole screen (hidden by forced blank); UIR_BODY = only the part
// that changes when the cursor moves.
void ui_draw(int x, int y, int dir) {
  if (ui_redraw & UIR_FULL) {
    wait_vblank();
    REG_DISPCNT = 0x0080;
    draw_ui_page(x, y, dir);
    REG_DISPCNT = 0x0403;
  } else if (ui_redraw & UIR_BODY) {
    switch (ui_screen) {
      case UI_MENU:    draw_main_menu();   break;
      case UI_PARTY:   draw_party_body();  break;
      case UI_DEX:     draw_dex_body();    break;
      case UI_OPTIONS: draw_options_body(); break;
      default: break;
    }
  }
  ui_redraw = 0;
}

// Draw again whatever world.c marked as out of date in bt_redraw.
void battle_redraw(void) {
  if (bt_redraw & BT_PARTY) {                  // the party screen, complete
    wait_vblank();
    REG_DISPCNT = 0x0080;
    draw_party_page();
    REG_DISPCNT = 0x0403;
  } else if (bt_redraw & BT_LIST) {            // only the rows + bottom line
    draw_party_body();
  } else if (bt_redraw & BT_SCENE) {           // the whole battle screen again
    wait_vblank();
    REG_DISPCNT = 0x0080;
    draw_battle_scene();
    draw_mon_panel(&wild_mon, 8, 8, 0);
    draw_mon_panel(&player_mon, 112, 74, 1);
    draw_battle_box();
    REG_DISPCNT = 0x0403;
  } else {
    if (bt_redraw & BT_HUD) {
      draw_mon_panel(&wild_mon, 8, 8, 0);
      draw_mon_panel(&player_mon, 112, 74, 1);
    }
    if (bt_redraw & BT_BOX) draw_battle_box();
  }
  bt_redraw = 0;
}

// Show the whole battle screen (call right after battle_begin()).
void battle_screen_open(void) {
  wait_vblank();
  REG_DISPCNT = 0x0080;
  draw_battle_scene();
  bt_redraw = BT_HUD | BT_BOX;
  battle_redraw();
  REG_DISPCNT = 0x0403;
  battle_open = 1;
}

// The battle is over: main repaints the map (or moves you to the CENTER).
void battle_close(void) {
  battle_open = 0;
  walk_frame = 0;
  walk_timer = 0;
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
// 7 (orange) = a dialogue box, the battle screen or a menu is open
void show_lamps(unsigned int frame, unsigned short keys,
                       int moved, int blocked) {
  lamp(0, (frame >> 5) & 1, RGB15(31, 0, 31));
  lamp(1, keys & KEY_LEFT,  RGB15(0, 31, 31));
  lamp(2, keys & KEY_RIGHT, RGB15(0, 31, 31));
  lamp(3, keys & KEY_UP,    RGB15(0, 31, 31));
  lamp(4, keys & KEY_DOWN,  RGB15(0, 31, 31));
  lamp(5, moved,            RGB15(31, 31, 31));
  lamp(6, blocked,          RGB15(31, 0, 0));
  lamp(7, dlg_open || battle_open || ui_screen != UI_NONE, RGB15(31, 20, 0));
}
#endif
// END OF render.c
