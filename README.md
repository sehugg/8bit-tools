# 8bitworkshop Tools

A comprehensive collection of Python and C utilities for converting modern digital assets (images, audio, fonts, graphics) into formats suitable for 8-bit retro computing systems and arcade hardware. These tools are designed to work with platforms like Atari 2600/VCS, Williams arcade systems, VIC Dual, Midway 8080, and others, primarily for use with the [8bitworkshop](https://8bitworkshop.com) IDE.

## Quick Start

```bash
git clone https://github.com/sehugg/8bit-tools
cd 8bit-tools
python3 -m venv .
. bin/activate
pip install -r requirements.txt
```

## Dependencies

- **Python 3.x** - Core scripting language
- **ImageMagick** - Image conversion (`convert` command)
- **Mido** - MIDI file processing
- **Sox** - Audio processing (for sound tools)

## Platform Structure

The project is organized by target platform, with each directory containing specialized conversion tools:

```
8bit-tools/
├── vcs/          # Atari 2600/VCS bitmap conversion
├── williams/     # Williams arcade hardware graphics
├── vicdual/      # VIC Dual arcade system ROM tools
├── mw8080/       # Midway 8080 platform utilities
├── scramble/     # Galaxian/Scramble ROM handling
├── nes/          # Nintendo Entertainment System graphics
├── atari7800/    # Atari 7800 specific tools
├── sound/        # Audio processing (MIDI, WAV, note tables)
├── fonts/        # BDF font parsing and conversion
├── vector/       # SVG to vector graphics conversion
├── misc/         # General purpose utilities
├── lfsr/         # Linear Feedback Shift Register calculations
└── tts/          # Text-to-speech processing
```

## Audio Tools

### MIDI to 8-Bit Song Conversion

Convert MIDI files to 8-bit song format with customizable voice mapping:

```bash
cd sound
python3 midi2song.py [options] input.mid [channels]
```

**Options:**
- `-s, --start N` - First MIDI note (default: 21)
- `-n, --num N` - Number of notes (default: 64)  
- `-v, --voices N` - Number of voices (default: 3)
- `-T, --transpose N` - Transpose by half-steps
- `-t, --tempo N` - Tempo (default: 48)
- `-H, --hex` - Hex output format
- `-A, --asm` - Assembly output format
- `-D, --drums N` - Drum channel

**Examples:**
```bash
# Convert MIDI with 3 voices, hex output
python3 midi2song.py -v 3 -H entertainer.mid

# Transpose up 2 semitones, assembly format
python3 midi2song.py -T 2 -A maple.mid

# Use specific MIDI channels
python3 midi2song.py bwv-988-aria.mid 0,1,2
```

### Audio Processing

```bash
# Convert WAV to Apple II Delta Modulation
cd sound
python3 wav2amc.py input.wav bit_depth

# Generate note frequency tables
python3 mknotes.py        # Generic 8-bit systems
python3 mknotes2600.py    # Atari 2600 specific
python3 mknotes800.py     # Atari 800 series
```

## Graphics Tools

### Atari 2600/VCS Graphics

Convert bitmap images to Atari 2600 playfield format:

```bash
cd vcs
python3 p4_to_pfbytes.py input.pbm > output.hex
python3 p4_to_48pix.py input.pbm > output.hex
```

**Input requirements:**
- PBM (Portable Bitmap) format, P4 binary
- Specific dimensions based on target format

**Workflow using Makefiles:**
```bash
cd vcs
# Convert JPG to 40x192 playfield bitmap
make scott-joplin-pf.pbm  # Creates from scott-joplin.jpg
# Convert to hex data
make scott-joplin-pf.hex
```

### Williams Arcade Graphics

Convert PCX images to Williams arcade hardware format:

```bash
cd williams
python3 pcx2will.py input.pcx > output.c
```

**Features:**
- Generates C sprite data with palette
- Handles RLE compression
- Outputs Williams-compatible color format

**Example workflow:**
```bash
cd williams
# Convert PNG to PCX (via Makefile)
make badspacerobots.pcx   # Creates from badspacerobots.png
# Convert to Williams C format  
python3 pcx2will.py badspacerobots.pcx > sprites.c
```

## Font Tools

### BDF Font Conversion

Convert BDF bitmap fonts to various 8-bit formats:

```bash
cd fonts
python3 parsebdf8.py [options] font.bdf
```

**Output Format Options:**
- `-C, --carray` - Nested C array (default)
- `-F, --flatcarray` - Flat C array
- `-A, --asmhex` - DASM-compatible hex
- `-B, --asmdb` - Z80ASM-compatible hex  
- `-V, --verilog` - Verilog-compatible hex

**Character Selection:**
- `-s, --start N` - Index of first character
- `-e, --end N` - Index of last character
- `-H, --height N` - Character height

**Transformations:**
- `-i, --invert` - Invert bits
- `-r, --rotate` - Rotate bits  
- `-f, --flip` - Flip vertically
- `-m, --mirror` - Mirror horizontally

**Examples:**
```bash
# Convert ASCII characters 33-97 to C array
python3 parsebdf8.py -s 33 -e 97 -C tom-thumb.bdf > font.c

# Generate Verilog format with inverted bits
python3 parsebdf8.py -i -V c64.bdf > font.v

# Create assembly format for Z80
python3 parsebdf8.py -B -s 32 -e 126 cp437-8x8.bdf > font.asm
```

## Mathematical Tools

### Linear Feedback Shift Register (LFSR)

Generate LFSR sequences for pseudorandom number generation:

```bash
cd lfsr
python3 lfsrcalc.py > lfsr_results.txt
python3 lfsrcalc2.py     # Enhanced version
```

**Output format:** `Period,nbits,feedback,mask`

### Lookup Tables

```bash
cd misc
python3 sintbl.py     # Generate sine wave tables
python3 mkztab.py     # Create Z-tables
```

## Utility Tools

### File Format Conversion

```bash
cd misc
# Convert binary files to C arrays
python3 bin2arr.py input.bin > output.c

# Convert C arrays back to binary
python3 carr2bin.py input.c > output.bin

# Convert PBM bitmaps to C format
python3 pbm_to_c.py input.pbm > output.c
```

### Cross-Platform Graphics

```bash
cd nes
# Convert NES tiles to Sega Master System tiles
python3 nes2sms.py input.nes > output.sms

# Generate road graphics for racing games
python3 road.py
```

### Vector Graphics

```bash
cd vector
# Convert SVG to vector display format
python3 svg2vector.py input.svg > output.vec
```

## Platform-Specific Workflows

### Atari 2600 Development

1. **Prepare graphics:**
   ```bash
   cd vcs
   # Convert image to proper format
   convert sprite.jpg -resize 48x192! -colorspace Gray -dither FloydSteinberg sprite-48.pbm
   # Generate playfield data
   python3 p4_to_48pix.py sprite-48.pbm > sprite.hex
   ```

2. **Process fonts:**
   ```bash
   cd fonts
   python3 parsebdf8.py -A -s 32 -e 126 tom-thumb.bdf > font_2600.asm
   ```

### Williams Arcade Development

1. **Graphics pipeline:**
   ```bash
   cd williams
   # PNG → PCX → Williams C format
   make sprites.pcx
   python3 pcx2will.py sprites.pcx > sprites.c
   ```

2. **Font integration:**
   ```bash
   cd fonts
   python3 parsebdf4bit.py -s 33 -e 97 ../fonts/tom-thumb.bdf > williams_font.c
   ```

### Audio Integration

1. **MIDI conversion:**
   ```bash
   cd sound
   # Convert to 3-voice hex format
   python3 midi2song.py -v 3 -H -t 60 song.mid > music.hex
   ```

2. **Note table generation:**
   ```bash
   # Platform-specific note tables
   python3 mknotes2600.py > notes_2600.c
   python3 mknotes800.py > notes_800.c  
   ```

## Common Make Targets

Each platform directory includes Makefiles with common conversion patterns:

```bash
# Build all targets in a directory
make all

# Convert specific formats
make %.hex     # Binary to hex dump
make %.c       # Various to C array format
make %.s       # Various to assembly format
make %.pbm     # JPG to PBM bitmap
make %.pcx     # PNG to PCX format
make %.tga     # PNG to TGA format
```

## Integration with 8bitworkshop

These tools generate data files compatible with [8bitworkshop](https://8bitworkshop.com):

1. **Graphics:** Generate hex/assembly data files
2. **Audio:** Create music data arrays  
3. **Fonts:** Produce character definition arrays
4. **Data:** Convert various formats to platform-specific structures

Include generated files in your 8bitworkshop projects using appropriate include statements or data directives.

## Contributing

When adding new tools:
1. Follow the platform-specific directory structure
2. Include appropriate command-line help
3. Support common output formats (C, assembly, hex)
4. Add Make targets for common workflows
5. Document usage patterns

## License

See LICENSE file for details.