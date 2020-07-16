
# converter for Apple ][ Delta Modulation Sample Playback
# ?platform=apple2&file=deltamod.dasm

%.dat: %.wav
	sox $< -t dat -r 30817 -c 1 $@ highpass 100 gain -l 18

%.dat12.bin: %.dat
	python ./wav2amc.py $< 12

