// SPDX-License-Identifier: MIT
// Synthetic MGC1 fixture: no game, emulator or executable C64 program.
export function crc32(bytes){let c=0xffffffff;for(const byte of bytes){c^=byte;for(let n=0;n<8;n++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (~c)>>>0;}
export function gameCartFixture(){
  const prefix=Buffer.alloc(0x6070);prefix.write('C64 CARTRIDGE   ');prefix.writeUInt32BE(64,16);prefix.writeUInt16BE(32,22);
  for(let i=0;i<3;i++){const p=64+i*8208;prefix.write('CHIP',p);prefix.writeUInt32BE(8208,p+4);prefix.writeUInt16BE(i===2?1:0,p+10);prefix.writeUInt16BE(i===1?0xa000:0x8000,p+12);prefix.writeUInt16BE(8192,p+14);}
  const payload=Buffer.alloc(16,0x42),module=Buffer.alloc(80);
  [0x314d564d,2,64,16,0,0,0x18001,0x18000,0x20014000,31,crc32(payload)].forEach((v,i)=>module.writeUInt32LE(v,i*4));
  module.writeUInt32LE(crc32(module.subarray(0,64)),44);payload.copy(module,64);
  const content=Buffer.alloc(6500,0x51),table=Buffer.alloc(32),blocks=[];
  let offset=prefix.length+256+table.length;
  for(let i=0;i<2;i++){
    const raw=content.subarray(i*4096,(i+1)*4096),encoded=[0x1f,0x51,1,0];let more=raw.length-25;
    while(more>=255){encoded.push(255);more-=255;}encoded.push(more,0x50,0x51,0x51,0x51,0x51,0x51);
    const block=Buffer.from(encoded);blocks.push(block);[offset,block.length,raw.length,crc32(raw)].forEach((v,j)=>table.writeUInt32LE(v,i*16+j*4));offset+=block.length;
  }
  const index=Buffer.alloc(256);index.write('content.bin');index.writeUInt32LE(1,64);index.writeUInt32LE(content.length,68);
  index.writeUInt32LE(prefix.length+256+32,72);index.writeUInt32LE(blocks.reduce((n,b)=>n+b.length,0),76);index.writeUInt32LE(crc32(content),80);
  index.writeUInt32LE(2,84);index.writeUInt32LE(prefix.length+256,88);index.writeUInt32LE(crc32(table),92);
  index.write('engine.mvm',128);index.writeUInt32LE(module.length,196);index.writeUInt32LE(offset,200);index.writeUInt32LE(module.length,204);index.writeUInt32LE(crc32(module),208);
  const h=prefix.subarray(0x4070,0x4170);h.write('MGC1');h.writeUInt16LE(1,4);h.writeUInt16LE(256,6);
  [offset+module.length,1,4096,2,prefix.length,128,crc32(index),crc32(Buffer.concat([prefix.subarray(80,8272),prefix.subarray(8288,16480)])),1,0,31].forEach((v,i)=>h.writeUInt32LE(v,8+i*4));
  h.write('TEST',64);h.write('Synthetic MPE cartridge',88);h.write('content.bin',152);h.write('MGC1 host',216);h.writeUInt32LE(crc32(h),56);
  return Buffer.concat([prefix,index,table,...blocks,module]);
}
