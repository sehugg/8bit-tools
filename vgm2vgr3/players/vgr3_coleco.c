/* vgr3_coleco.c -- ColecoVision SN76489 playback of a .vgr3 file
 * (see vgr3_format.h / vgr3_play.h).
 *
 * Reconstructs the SN76489 latch/data byte protocol from the decoded
 * shadow registers (the inverse of snWrite() in vgm2vgr3.c): tone
 * period registers need a latch byte (low 4 bits) followed by a data
 * byte (high 6 bits); every other register is a single latch byte
 * carrying the whole 4-bit value.
 */

#include "vgr3_format.h"
#include "vgr3_play.h"
//#link "vgr3_play.c"

#include <cv.h>

__sfr __at (0xff) io_psg;

inline void colecoPsgOut(uint8_t byte) {
  io_psg = byte;
}

static Vgr3Player g_player;

/* Flush one dirty byte starting at shadow register r0<<3. t/cc are the
 * latch phase / channel for the first register of the group; they are a
 * pure function of the register index, so each group can be seeded
 * independently (t=0,cc=0 for 0x0; t=2,cc=2 for 0x8) and a clean group
 * can be skipped without disturbing the next one. */
static void colecoFlush8(uint8_t r0, uint8_t n, uint8_t t, uint8_t cc) {
   uint8_t dirty = g_player.dirty[r0];
   uint8_t r,v,i;
   if (!dirty) return;
   g_player.dirty[r0] = 0;
   r = r0 << 3;
   for (i = 0; i < n; i++) {
     if (dirty & 1) {
       v = g_player.regs[r];
       if (r >= 9) {
           /* noise: 9 = control (0xE0), 10 = volume (0xF0) */
           colecoPsgOut((uint8_t)((r == 9 ? 0xE0 : 0xF0) | (v & (r == 9 ? 0x07 : 0x0F))));
       } else {
           if (t == 1) {
               /* high 6 bits: must be preceded by this channel's latch,
                * which may not itself be dirty this frame. */
               uint8_t lo = g_player.regs[r - 1];
               colecoPsgOut((uint8_t)(0x80 | (cc << 5) | (lo & 0x0F)));
               colecoPsgOut((uint8_t)(v & 0x3F));
           } else {
               /* t == 0: period low nibble; t == 2: volume */
               colecoPsgOut((uint8_t)(0x80 | (cc << 5) | (t == 2 ? 0x10 : 0x00) | (v & 0x0F)));
           }
       }
     }
     dirty >>= 1;
     if (++t == 3) { t = 0; cc++; }
     r++;
   }
}

void colecoFlushVgr(void) {
  colecoFlush8(0, 8, 0, 0); // 0x0-0x7: ch0, ch1, ch2 period low/high
  colecoFlush8(1, 3, 2, 2); // 0x8-0xA: ch2 volume + noise control/volume
}

const unsigned char MUSIC1[] = {
  #embed "nightmarket.vgr3"
};

void vint_handler(void) {
  cv_set_colors(CV_COLOR_BLACK, CV_COLOR_BLUE);
  vgr3Frame(&g_player);
  colecoFlushVgr();
  cv_set_colors(CV_COLOR_BLACK, CV_COLOR_BLACK);
}

void main() {
  cv_set_vint_handler(vint_handler);
  cv_set_screen_active(true);
  vgr3Init(&g_player, MUSIC1);
  while (1) {
  }
}
