
.PHONY: all binaries clean

all: binaries

%.lzg: %
	lzg -9 $< $@

binaries: scr2floyd scr2floyd_percept galois raw8to4

scr2floyd: tms9918a/scr2floyd.c
	gcc -o $@ $<

scr2floyd_percept: tms9918a/scr2floyd_percept.c
	gcc -o $@ $<

galois: lfsr/galois.c
	gcc -o $@ $<

raw8to4: sound/raw8to4.c
	gcc -o $@ $<

clean:
	rm -f scr2floyd scr2floyd_percept galois raw8to4 *.o *.s

%-pf.hex: %-pf.pbm vcs/p4_to_pfbytes.py
	python3 vcs/p4_to_pfbytes.py $< > $@

%-48.hex: %-48.pbm vcs/p4_to_48pix.py
	python3 vcs/p4_to_48pix.py $< > $@

%-pf.pbm: %.jpg
	convert $< -resize 40x192\! -colorspace Gray -dither FloydSteinberg $@

%-48.pbm: %.jpg
	convert $< -resize 48x192\! -colorspace Gray -dither FloydSteinberg $@

%.tga: %.png
	convert $< -resize 192 $<.gif
	convert $<.gif +dither -type palette -depth 4 -compress RLE -colors 8 -flip $@
	convert $@ $@.png

%.pcx: %.png
	convert $< -format raw -type palette -compress none -colors 15 +dither $@
%.rle.pcx: %.png
	convert $< -format raw -type palette -compress rle -colors 15 +dither $@
%.4.pcx: %.png
	convert $< -format raw -type palette -compress none -colors 4 +dither $@

ship1.pbm: ship1.png
	convert ship1.png -negate -flop ship1.pbm

%.h:
	cat $* | hexdump -v -e '"\n" 128/1 "0x%02x,"'

%.prom:
	cat $* | hexdump -v -e '" \n defb  " 32/1 "$$%02x,"' | cut -c 2-134

%.s:
	cat $* | hexdump -v -e '" \n .byte  " 32/1 "$$%02x,"' | cut -c 2-135

%.rot.pbm: %.pbm
	convert $< -transpose -bordercolor white -border 4x4 $@

baddies-horiz.rot.pbm: baddies-horiz.png
	convert $< +dither -brightness-contrast 50x50 -fill black -transpose -negate $@
	convert $@ foo.png

lfsr.out: lfsr/lfsrcalc.py
	cd lfsr && python3 lfsrcalc.py | sort -n > ../lfsr.out 
