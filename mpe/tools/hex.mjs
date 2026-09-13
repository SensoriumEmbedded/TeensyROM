// SPDX-License-Identifier: MIT
export const FLASH_BASE=0x60000000, FLASH_LIMIT=0x607c0000;
export const MAIN_BASE=0x60060000, VM_BASE=0x60280000, VM_LIMIT=0x602e0000;
export function decodeHex(text){
  const bytes=new Map();let base=0,eof=false;
  for(const line of text.trim().split(/\r?\n/)){
    if(eof||!/^:[0-9a-f]+$/i.test(line)||line.length%2!==1)throw Error('Malformed HEX record');
    const b=Buffer.from(line.slice(1),'hex');
    if(b.length!==b[0]+5||(b.reduce((a,v)=>a+v,0)&255))throw Error('HEX record checksum/length');
    const address=b.readUInt16BE(1),type=b[3];
    if(type===0){for(let i=0;i<b[0];i++){const a=base+address+i;if(bytes.has(a))throw Error('Duplicate HEX address');bytes.set(a,b[4+i]);}}
    else if(type===4&&b[0]===2&&address===0)base=b.readUInt16BE(4)*65536;
    else if(type===1&&b[0]===0&&address===0)eof=true;
    else if((type===3||type===5)&&b[0]===4&&address===0){} // entry is in the IVT
    else throw Error('Unsupported HEX record');
  }
  if(!eof||!bytes.size)throw Error('Missing HEX data/EOF');return bytes;
}
export function encodeHex(bytes){
  const record=(addr,type,data)=>{const b=Buffer.alloc(data.length+5);b[0]=data.length;b.writeUInt16BE(addr,1);b[3]=type;data.copy(b,4);b[b.length-1]=(-b.reduce((a,v)=>a+v,0))&255;return ':'+b.toString('hex').toUpperCase();};
  const addresses=[...bytes.keys()].sort((a,b)=>a-b);let high=-1,lines=[];
  for(let i=0;i<addresses.length;){const start=addresses[i];if(Math.floor(start/65536)!==high){high=Math.floor(start/65536);const h=Buffer.alloc(2);h.writeUInt16BE(high);lines.push(record(0,4,h));}
    const data=[];while(i<addresses.length&&addresses[i]===start+data.length&&Math.floor(addresses[i]/65536)===high&&data.length<16)data.push(bytes.get(addresses[i++]));
    lines.push(record(start&65535,0,Buffer.from(data)));
  }return lines.concat(':00000001FF').join('\n')+'\n';
}
export function combineHex(images){
  const merged=new Map(),regions=[];
  for(const {name,text,start,end} of images){
    const bytes=decodeHex(text);
    let usedEnd=start;
    for(const [address,value] of bytes){if(address<start||address>=end||merged.has(address))throw Error('HEX partition overlap/bounds: '+name);merged.set(address,value);usedEnd=Math.max(usedEnd,address+1);}
    if(!bytes.has(start))throw Error('Missing image header: '+name);
    regions.push({name,start,end,usedEnd,occupiedBytes:bytes.size});
  }
  const last=Math.max(...regions.map(r=>r.usedEnd));
  const stagingStart=Math.ceil(last/4096)*4096,stagingBytes=FLASH_LIMIT-stagingStart;
  const imageSpan=last-FLASH_BASE;
  if(!merged.has(FLASH_BASE)||last>FLASH_LIMIT||imageSpan>stagingBytes)throw Error('Firmware does not fit stock updater staging area');
  return {hex:encodeHex(merged),regions,imageSpan,stagingStart,stagingBytes};
}
