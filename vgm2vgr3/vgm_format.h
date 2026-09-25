/* vgm_format.h -- constants for reading source .vgm files (input side).
 * Offsets/opcodes per the public VGM spec (vgmrips.net/wiki/VGM_Specification).
 * Only the subset needed to decode SN76489/AY8910/NES-APU/GB-DMG/POKEY streams
 * and safely skip everything else is enumerated here.
 */

#ifndef VGM_FORMAT_H
#define VGM_FORMAT_H

#define VGM_MAGIC_STR   "Vgm "

/* Header field byte offsets. Fields store "offset relative to itself"
 * per the spec (e.g. absolute data offset = VGM_HDR_OFF_VGM_DATA_OFFSET
 * + the stored value), except where noted. */
#define VGM_HDR_OFF_MAGIC             0x00
#define VGM_HDR_OFF_EOF_OFFSET        0x04
#define VGM_HDR_OFF_VERSION           0x08
#define VGM_HDR_OFF_SN76489_CLOCK     0x0C
#define VGM_HDR_OFF_GD3_OFFSET        0x14
#define VGM_HDR_OFF_TOTAL_SAMPLES     0x18
#define VGM_HDR_OFF_LOOP_OFFSET       0x1C
#define VGM_HDR_OFF_LOOP_SAMPLES      0x20
#define VGM_HDR_OFF_RATE              0x24
#define VGM_HDR_OFF_SN76489_FEEDBACK  0x28  /* u16 */
#define VGM_HDR_OFF_SN76489_SRWIDTH   0x2A  /* u8 */
#define VGM_HDR_OFF_SN76489_FLAGS     0x2B  /* u8, v1.51+ */
#define VGM_HDR_OFF_VGM_DATA_OFFSET   0x34  /* u32, v1.50+ */
#define VGM_HDR_OFF_YM2151_CLOCK      0x30
#define VGM_HDR_OFF_AY8910_CLOCK      0x74
#define VGM_HDR_OFF_AY8910_TYPE       0x78
#define VGM_HDR_OFF_GB_DMG_CLOCK      0x80
#define VGM_HDR_OFF_NES_APU_CLOCK     0x84
#define VGM_HDR_OFF_POKEY_CLOCK       0xB0  /* v1.61+ */

#define VGM_HDR_DEFAULT_DATA_OFFSET   0x40  /* pre-1.50 files: data starts here */
#define VGM_HDR_MIN_READ_SIZE         0x100 /* header prefix we always read */

/* Fields below a given offset only exist from that VGM version onward;
 * older files may have unrelated bytes (or nothing) sitting there, so
 * these must be version-gated before reading, not just bounds-checked. */
#define VGM_VERSION_1_50   0x150  /* adds vgmDataOffset */
#define VGM_VERSION_1_51   0x151  /* adds AY8910 clock/type, SN76489 flags */
#define VGM_VERSION_1_60   0x160  /* adds NES APU clock */
#define VGM_VERSION_1_61   0x161  /* adds Game Boy DMG clock */

/* Stream command opcodes we decode directly. */
#define VGM_CMD_SN76489_WRITE   0x50  /* 1 data byte: latch/data protocol */
#define VGM_CMD_AY8910_WRITE    0xA0  /* 2 data bytes: reg, data */
#define VGM_CMD_GB_DMG_WRITE    0xB3  /* 2 data bytes: reg, data */
#define VGM_CMD_NES_APU_WRITE   0xB4  /* 2 data bytes: reg, data */
#define VGM_CMD_POKEY_WRITE     0xBB  /* 2 data bytes: reg, data */
/* DefleMask has no SID chip in VGM, so it exports SID as YM2151
 * register writes: 0xB6 aa dd where aa is a SID register ($D400+aa).
 * A real YM2151 uses register numbers >= 0x20, so a file whose 0xB6
 * stream stays in 0x00-0x18 is SID (see samples/sid). */
#define VGM_CMD_YM2151_WRITE    0xB6  /* 2 data bytes: reg, data */
#define VGM_YM2151_MAX_SID_REG  0x18

/* Timing / control opcodes. */
#define VGM_CMD_WAIT_NN          0x61  /* + u16 LE: wait n samples */
#define VGM_CMD_WAIT_735         0x62  /* wait 735 samples (1/60s @44100) */
#define VGM_CMD_WAIT_882         0x63  /* wait 882 samples (1/50s @44100) */
#define VGM_CMD_END_OF_DATA      0x66  /* stop */
#define VGM_CMD_DATA_BLOCK       0x67  /* 0x66 tt <u32 size LE> <size bytes> */
#define VGM_CMD_DATA_BLOCK_TAG   0x66  /* required 2nd byte of 0x67 */
#define VGM_CMD_PCM_RAM_WRITE    0x68  /* 11 data bytes */
#define VGM_CMD_WAIT_OVERRIDE    0x64  /* 4 data bytes (rare) */
#define VGM_CMD_WAIT_N_PLUS1_LO  0x70  /* 0x70-0x7F: wait (opcode&0xF)+1 samples */
#define VGM_CMD_WAIT_N_PLUS1_HI  0x7F
#define VGM_CMD_YM2612_DAC_LO    0x80  /* 0x80-0x8F: DAC write + wait n samples */
#define VGM_CMD_YM2612_DAC_HI    0x8F

/* Data block type for NES APU DPCM sample data. Data blocks 0x00-0x3F
 * are chip PCM banks, concatenated in file order; 0x07 is the NES DMC
 * bank the DMC's $4012/$4013 registers index into from $C000. See
 * study/VGM_Specification.txt. */
#define VGM_DB_NES_DPCM   0x07

/* Opcode ranges with a uniform trailing data-byte count, used to safely
 * skip commands for chips we don't decode. */
#define VGM_RANGE_1BYTE_LO     0x30
#define VGM_RANGE_1BYTE_HI     0x3F
#define VGM_RANGE_2BYTE_A_LO   0x40   /* reserved */
#define VGM_RANGE_2BYTE_A_HI   0x4E
#define VGM_RANGE_2BYTE_B_LO   0x51   /* YM*, AY, GB, NES, PSG-adjacent writes */
#define VGM_RANGE_2BYTE_B_HI   0xBF
#define VGM_RANGE_3BYTE_LO     0xC0
#define VGM_RANGE_3BYTE_HI     0xD6
#define VGM_RANGE_4BYTE_LO     0xE0
#define VGM_RANGE_4BYTE_HI     0xE1

#define VGM_CMD_GG_STEREO        0x4F  /* 1 data byte */
#define VGM_CMD_DAC_SETUP        0x90  /* 4 data bytes */
#define VGM_CMD_DAC_SETDATA      0x91  /* 4 data bytes */
#define VGM_CMD_DAC_SETFREQ      0x92  /* 5 data bytes */
#define VGM_CMD_DAC_START        0x93  /* 10 data bytes */
#define VGM_CMD_DAC_STOP         0x94  /* 1 data byte */
#define VGM_CMD_DAC_STARTFAST    0x95  /* 4 data bytes */

#define VGM_SAMPLES_PER_SEC   44100  /* VGM's fixed internal sample clock */

#endif /* VGM_FORMAT_H */
