
/*

Multiple compressed streams.

11nn nnnn - wait for n+1 frames
10nn yyyy - wait n frames, then set register y (1 byte)
01aa yyyy - set register y, then register y+a (2 bytes)
00jj jjjj - replay sequence j
0000 0000 - return

*/

const fs = require("fs");
const {  
  VGM, 
  VGMWriteDataCommand, 
  VGMWaitCommand,
  VGMEndCommand, 
  parseVGMCommand 
} = require("vgm-parser");
 
function toArrayBuffer(b) {
  return b.buffer.slice(b.byteOffset, b.byteOffset + b.byteLength);
}
 
//const buf = fs.readFileSync("carnival.vgm");
const buf = fs.readFileSync("04 Cave Explorer.vgm");
 
/* Do not pass buf.buffer directly. It only works when buf.byteOffset == 0. */
/* fs.readFileSync often returns Buffer with byteOffset != 0. */
const vgm = VGM.parse(toArrayBuffer(buf));
console.log(vgm);
console.log(vgm.chips);

var time = 0;
const SPF = 735;

var regs = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];

/* Access VGM commands as a list */
const stream = vgm.getDataStream();
for (const cmd of stream.commands) {
  if (cmd instanceof VGMWriteDataCommand) {
    if (cmd.index==0 && cmd.port==0) {
      regs[cmd.addr] = cmd.data;
      //console.log('=', cmd.addr, cmd.data);
    }
  } else if (cmd instanceof VGMWaitCommand) {
    time += cmd.count;
    if (time > SPF) {
      var frames = Math.floor(time/SPF);
      console.log(regs.join(' '));
      console.log('+', frames);
      time -= frames*SPF;
    }
  }
}
