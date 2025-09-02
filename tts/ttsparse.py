#!/usr/bin/env python3

PhonemeDurations = [
59,71,121,47,47,71,103,90,71,55,80,121,103,
80,71,71,71,121,71,146,121,146,103,185,103,
80,47,71,71,103,55,90,185,65,80,47,
250,103,185,185,185,103,71,90,185,80,185,103,
90,71,103,185,80,121,59,90,80,71,
146,185,121,250,185,47
]

PhonemeTable = [
 "EH3","EH2","EH1","PA0","DT" ,"A1" ,"A2" ,"ZH",
 "AH2","I3" ,"I2" ,"I1" ,"M"  ,"N"  ,"B"  ,"V",
 "CH" ,"SH" ,"Z"  ,"AW1","NG" ,"AH1","OO1","OO",
 "L"  ,"K"  ,"J"  ,"H"  ,"G"  ,"F"  ,"D"  ,"S",
 "A"  ,"AY" ,"Y1" ,"UH3","AH" ,"P"  ,"O"  ,"I",
 "U"  ,"Y"  ,"T"  ,"R"  ,"E"  ,"W"  ,"AE" ,"AE1",
 "AW2","UH2","UH1","UH" ,"O2" ,"O1" ,"IU" ,"U1",
 "THV","TH" ,"ER" ,"EH" ,"E1" ,"AW" ,"PA1","STOP",
]

GorfWordTable = [
 "A2AYY1","A2E1","UH1GEH1I3N","AE1EH2M","AEM",
 "AE1EH3ND","UH1NAH2I1YLA2SHUH2N","AH2NUHTHER","AH1NUHTHVRR",
 "AH1R","UHR","UH1VEH1EH3NNDJER","BAEEH3D","BAEEH1D","BE",
 "BEH3EH1N","BUHT","BUH1DTTEH2NN","KUHDEH2T",
 "KAE1NUH1T","KAE1EH3PTI3N",
 "KRAH2UH3NI3KUH3O2LZ","KO1UH3I3E1N","KO1UH3I3E1NS",
 "KERNAH2L","KAH1NCHEHSNEHS","DE1FEH1NDER",
 "DE1STRO1I1Y","DE1STRO1I1Y1D",
 "DU1UM","DRAW1S","EHMPAH2I3YR","EHND",
 "EH1NEH1MY","EH1SKA1E1P","FLEHGSHIP",
 "FOR","GUH1LAEKTI1K",
 "DJEH2NERUH3L","GDTO1O1RRFF","GDTO1RFYA2N","GDTO1RFE1EH2N","GDTO1RFYA2NS",
 "HAH1HAH1HAH1HAH1","HAH1HAH1HER","HUHRDER",
 "HAE1EH3V","HI1TI1NG","AH1I1Y", "AH1I1Y1","I1MPAH1SI1BL",
 "I1N","INSERT","I1S","LI1V","LAWNG","MEE1T","MUU1V",
 "MAH2I1Y","MAH2I3Y","NIR","NEHKST","NUH3AH2YS","NO",
 "NAH1O1U1W","PA1","PLA1AYER","PRE1PAE1ER","PRI1SI3NEH3RS",
 "PRUH2MOTEH3D","POO1IUSH","RO1U1BAH1T","RO1U1BAH1TS",
 "RO1U1BAH1UH3TS","SEK", "SHIP","SHAH1UH3T","SUHM","SPA2I3YS","PA0",
 "SERVAH2I1Y1VUH3L","TAK","THVUH","THVUH1",
 "THUH","TAH1EH3YM","TU","TIUU1",
 "UH2NBE1AYTUH3BUH3L",
 "WORAYY1EH3R","WORAYY1EH3RS","WI1L",
 "Y1I3U1","YIUU1U1","YI1U1U1","Y1IUU1U1","Y1I1U1U1","YOR","YU1O1RSEH1LF","S",
 "FO1R","FO2R","WIL","GDTO1RVYA2N",

 "KO1UH3I3AYNN",
 "UH1TAEEH3K","BAH2I3Y1T","KAH1NKER","DYVAH1U1ER","DUHST","GAE1LUH1KSY","GAH1EH3T",
 "PAH1I1R","TRAH2I1Y","SU1PRE1N","AWL","HA2AYL",
 "EH1MPAH1I1R",
]

#define num_samples (sizeof(GorfWordTable)/sizeof(char *))

GorfSampleFiles = [
 "a.wav","a.wav","again.wav","am.wav","am.wav","and.wav","anhilatn.wav",
 "another.wav","another.wav","are.wav","are.wav",
 "avenger.wav","bad.wav","bad.wav","be.wav",
 "been.wav","but.wav","button.wav","cadet.wav",
 "cannot.wav","captain.wav","chronicl.wav","coin.wav","coins.wav","colonel.wav",
 "consciou.wav","defender.wav","destroy.wav","destroyd.wav",
 "doom.wav","draws.wav","empire.wav","end.wav",
 "enemy.wav","escape.wav","flagship.wav","for.wav","galactic.wav",
 "general.wav","gorf.wav","gorphian.wav","gorphian.wav","gorphins.wav",
 "hahahahu.wav","hahaher.wav","harder.wav","have.wav",
 "hitting.wav","i.wav","i.wav","impossib.wav","in.wav","insert.wav",
 "is.wav","live.wav","long.wav","meet.wav","move.wav",
 "my.wav","my.wav",
 "near.wav","next.wav","nice.wav","no.wav",
 "now.wav","pause.wav","player.wav","prepare.wav","prisonrs.wav",
 "promoted.wav","push.wav","robot.wav","robots.wav","robots.wav",
 "seek.wav","ship.wav","shot.wav","some.wav","space.wav","spause.wav",
 "survival.wav","take.wav","the.wav","the.wav","the.wav","time.wav",
 "to.wav","to.wav","unbeatab.wav",
 "warrior.wav","warriors.wav","will.wav",
 "you.wav","you.wav","you.wav","you.wav","your.wav","your.wav","yourself.wav",
 "s.wav","for.wav","for.wav","will.wav","gorph.wav",

 "coin.wav", "attack.wav","bite.wav","conquer.wav","devour.wav","dust.wav",
 "galaxy.wav","got.wav","power.wav","try.wav","supreme.wav","all.wav",
 "hail.wav","emperor.wav",
]

def phonemes_to_codes(s):
 codes = []
 while len(s):
  code = -1
  for j in range(0,64):
   ph = PhonemeTable[j]
   if s.startswith(ph):
    if code < 0 or len(ph) > len(PhonemeTable[code]):
     code = j
  if code < 0:
   raise Exception("Invalid phoneme %s" % s)
  s = s[len(PhonemeTable[code]):]
  codes.append(code)
 return codes

def codes_to_times(codes):
 result = []
 t = 0
 for c in codes:
   result.append(t)
   t += PhonemeDurations[c]
 return result + [t]

allwavs = ''
allmaps = {}

def codes2key(codes):
 return ''.join([chr(c) for c in codes])

def add_fragment(codes, t0, t1, t2, offset, wavlen):
 o1 = offset + t0*wavlen//t2
 o2 = offset + t1*wavlen//t2
 #print codes, t0, t1, t2, offset, wavlen, o1, o2
 allmaps[codes2key(codes)] = (o1,o2)

def extract_fragments(fn, codes, times):
 global allwavs
 with open('samples/%s'%fn, 'rb') as f:
  wav = f.read()[0x2c:]
  start = len(allwavs)
  allwavs += wav
  print(fn,len(wav),codes,times)
  add_fragment([63] + codes + [63], times[0], times[-1], times[-1], start, len(wav))
  for i in range(1,len(codes)+1):
   add_fragment([63] + codes[0:i], times[0], times[i], times[-1], start, len(wav))
  for i in range(0,len(codes)):
   add_fragment(codes[i:] + [63], times[i], times[-1], times[-1], start, len(wav))
  for n in range(1,len(codes)):
   for i in range(0,len(codes)-n):
    add_fragment(codes[i:i+n], times[i], times[i+n], times[-1], start, len(wav))

def extract_fragments_from_lists(plist, fnlist):
 n = len(plist)
 for i in range(0,n):
  pword = plist[i]
  fn = fnlist[i]
  codes = phonemes_to_codes(pword)
  times = codes_to_times(codes)
  #print pword,fn,codes,times
  extract_fragments(fn, codes, times)

def concat_speech(codes):
 if codes == '?':
  return ()
 x = allmaps.get(codes)
 if x:
  return x
 for i in range(1,len(codes)):
  x = allmaps.get(codes[0:-i])
  if x:
   return x + concat_speech(codes[-i:])
 for i in range(1,len(codes)):
  x = allmaps.get(codes[i:])
  if x:
   return concat_speech(codes[0:i]) + x
 raise Exception("Could not match %s" % repr([ord(x) for x in codes]))

def fraglist_to_raw(fl):
 s = ''
 for i in range(0,len(fl),2):
  s += allwavs[fl[i]:fl[i+1]]
 return s

###

extract_fragments_from_lists(GorfWordTable, GorfSampleFiles)

#mycodes = phonemes_to_codes('FUHKYIUU1U1AH1SHO1L')
mycodes = phonemes_to_codes('GO2TOO1BEH1D')
print(mycodes)
fraglist = concat_speech(codes2key([63] + mycodes + [63]))
print(fraglist)

outraw = fraglist_to_raw(fraglist)
print(len(outraw))
with open('out.u8','wb') as outf:
 outf.write(outraw)
