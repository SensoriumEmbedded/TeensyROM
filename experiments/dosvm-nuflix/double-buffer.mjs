import assert from 'node:assert/strict';

const lengths = new Map();
for (const opcode of [0x18,0x38,0x48,0x58,0x60,0x68,0x78,0x88,0x8a,0x98,0x9a,0xa8,0xaa,0xba,0xc8,0xca,0xe8,0xea]) lengths.set(opcode,1);
for (const opcode of [0x09,0x10,0x24,0x29,0x30,0x49,0x69,0x85,0x86,0x84,0x90,0x95,0xa0,0xa2,0xa4,0xa5,0xa6,0xa9,0xb0,0xc0,0xc6,0xc9,0xd0,0xe0,0xe6,0xe9,0xf0]) lengths.set(opcode,2);
for (const opcode of [0x20,0x2c,0x4c,0x6c,0x8c,0x8d,0x8e,0x99,0x9d,0xac,0xad,0xae,0xb9,0xbd,0xbe,0xce,0xee]) lengths.set(opcode,3);

export function mirrorDisplay(prg, template) {
  assert.equal(prg.readUInt16LE(),0x1000);
  assert.equal(prg.length,0x6a02);
  const image=Buffer.from(prg.subarray(2)), edits=[];
  const byte=(address,value)=>{edits.push({address,before:image[address-0x1000],after:value});image[address-0x1000]=value;};
  const word=(address,value)=>{byte(address,value&255);byte(address+1,value>>8);};
  const wait=template.readUInt16LE(0), table=0x304d;
  assert.equal(image[table-0x1003],0x4c);
  assert.equal(image.readUInt16LE(table-0x1002),wait);
  const relocateCode=(start,end)=>{
    for(let address=start;address<end;){
      if(address===table){byte(address+2,image[address-0x1000+2]+0x80);byte(address+3,image[address-0x1000+3]+0x80);address+=4;continue;}
      const opcode=image[address-0x1000],length=lengths.get(opcode);
      assert.ok(length,`Unknown opcode $${opcode.toString(16)} at $${address.toString(16)}`);
      assert.ok(address+length<=end);
      if(length===3){
        const operand=image.readUInt16LE(address-0x1000+1);
        if(operand>=0x0200&&operand<0x8000&&![0x02a6,0x0314,0x0315].includes(operand))word(address+1,operand+0x8000);
      }
      if((opcode===0x09||opcode===0xa9)&&image[address-0x1000+1]===2&&image.subarray(address-0x1000+2,address-0x1000+5).equals(Buffer.from([0x8d,0,0xdd])))byte(address+1,0);
      address+=length;
    }
  };
  relocateCode(0x3000,0x320e);
  relocateCode(wait,wait+23);
  const bankOperand=image.readUInt16LE(template.readUInt16LE(2)-0x1000)-0x8000;
  assert.ok(bankOperand>=0x1000&&bankOperand<0x2100);
  assert.equal(image[bankOperand-0x1000],3);
  byte(bankOperand,1);
  if(image[bankOperand-0x1001]===0xa2)for(let address=bankOperand+1;address<0x2100;){
    const opcode=image[address-0x1000];
    if([0xa2,0xa6,0x60].includes(opcode))break;
    const length=lengths.get(opcode);assert.ok(length);
    if(opcode===0x9d){assert.equal(image.readUInt16LE(address-0x1000+1),0xd00e);word(address+1,0xd010);}
    address+=length;
  }
  return {prg:Buffer.concat([Buffer.from([0,0x90]),image]),edits,bankOperand};
}

export const receiverHoles=[[0x0400,0x07f8],[0x0800,0x1000],[0x7a00,0x7ff8],[0x8400,0x87f8],[0x8800,0x9000],[0xfa00,0xfff8]];
