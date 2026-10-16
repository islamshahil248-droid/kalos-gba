#define REG_DISPCNT  (*(volatile unsigned short*)0x04000000)
#define REG_VCOUNT   (*(volatile unsigned short*)0x04000006)
#define REG_KEYINPUT (*(volatile unsigned short*)0x04000130)
#define VRAM ((volatile unsigned short*)0x06000000)
#define RGB15(r,g,b) ((r)|((g)<<5)|((b)<<10))

static void draw_box(int x, int y, unsigned short c) {
  for (int j = 0; j < 8; j++)
    for (int i = 0; i < 8; i++)
      VRAM[(y + j) * 240 + (x + i)] = c;
}

static void wait_vblank(void) {
  while (REG_VCOUNT >= 160) {}
  while (REG_VCOUNT < 160) {}
}

int main(void) {
  unsigned short grass = RGB15(8, 20, 8);
  int x = 116, y = 76;
  REG_DISPCNT = 0x0403;
  for (int i = 0; i < 240 * 160; i++) VRAM[i] = grass;
  while (1) {
    wait_vblank();
    draw_box(x, y, grass);
    unsigned short k = ~REG_KEYINPUT;
    if ((k & 0x10) && x < 232) x++;
    if ((k & 0x20) && x > 0)   x--;
    if ((k & 0x40) && y > 0)   y--;
    if ((k & 0x80) && y < 152) y++;
    draw_box(x, y, RGB15(31, 0, 0));
  }
}
