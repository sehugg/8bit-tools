#!/usr/bin/python

import sys, string, math, argparse
import mido

parser = argparse.ArgumentParser()
parser.add_argument('-s', '--start', type=int, default=21, help="first MIDI note")
parser.add_argument('-n', '--num', type=int, default=64, help="number of notes")
parser.add_argument('-v', '--voices', type=int, default=3, help="number of voices")
parser.add_argument('-T', '--transpose', type=int, default=0, help="transpose by half-steps")
parser.add_argument('-t', '--tempo', type=int, default=48, help="tempo")
parser.add_argument('-o', '--one', action="store_true", help="one voice per channel")
parser.add_argument('-H', '--hex', action="store_true", help="hex output")
parser.add_argument('-A', '--asm', action="store_true", help="asm output")
parser.add_argument('-L', '--locpu', action="store_true", help="one command per frame")
parser.add_argument('-D', '--drums', type=int, default=-1, help="drum channel")
parser.add_argument('midifile', help="MIDI file")
parser.add_argument('midichannels', nargs='?', help="comma-separated list of MIDI channels, or -")
args = parser.parse_args()

min_note = args.start
max_note = min_note + args.num - 1
max_voices = args.voices
one_voice_per_channel = args.one
tempo = args.tempo
transpose = args.transpose
coutput = not args.hex
asmoutput = args.asm
locpu = args.locpu
drumch = args.drums

fn = args.midifile

mid = mido.MidiFile(fn)

def hex1(n):
    return '%02x'%n
def hex2(n):
    return '0x%02x'%n

g_code = 0xc1
g_subs = []

def channels_for_track(track):
    channels = set()
    for msg in track:
        if msg.type == 'note_on':
            channels.add(msg.channel)
    return list(channels)
    
def note2drum(n):
    if n in [35,36,41]:
        return 0x41 # bass drum
    if n in [37,38,39,40]:
        return 0x42 # snare
    if n in [43,45,47,48]:
        return 0x43 # tom
    if n in [46,49,52,53,55,57]:
        return 0x44 # crash
    return -1

if not args.midichannels:
    #print(mid)
    print((mid.length, 'seconds'))
    for i, track in enumerate(mid.tracks):
        print(('Track {}: {} ({}) {}'.format(i, track.name, len(track), channels_for_track(track))))
        #for msg in track:
        #    print(msg)
else:
    gtime = 0
    curtime = 0
    nnotes = 0
    nvoices = 0
    curchans = 0
    channels = [int(x) for x in args.midichannels.split(',')]
    print ('')
    #print(("// %s %s" % (mid, channels)))
    output = []
    for msg in mid:
        gtime += msg.time * tempo
        if msg.type == 'note_on' and msg.channel in channels:
            note = msg.note + transpose
            vel = msg.velocity
            t = int(math.ceil(gtime))
            if vel > 0:
                while curtime < t:
                    dt = min(63, t-curtime)
                    curtime += dt
                    if nnotes > 0:
                        nvoices = 0
                        curchans = 0
                        output.append(dt+128)
                if note >= min_note and note <= max_note and nvoices < max_voices:
                    if not (one_voice_per_channel and (curchans & (1<<msg.channel))):
                        if msg.channel == drumch:
                            n = note2drum(note)
                        else:
                            n = note - min_note
                        if n >= 0 and n <= 127:
                            output.append(n)
                            nnotes += 1
                            nvoices += 1
                            curchans |= 1<<msg.channel
                            if locpu:
                              curtime += 1
    output.append(0xff)
    if asmoutput:
        print((','.join(['$'+hex1(x) for x in output])))
    elif coutput:
        print((','.join([hex2(x) for x in output])))
    else:
        bighex = ''.join([hex1(x) for x in output])
        for i in range(0,len(bighex)+32,32):
            print(('\thex', bighex[i:i+32]))
