// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import {combineHex,decodeHex,encodeHex,FLASH_BASE,MAIN_BASE,VM_BASE,VM_LIMIT} from './hex.mjs';
const image=(name,start,end)=>({name,start,end,text:encodeHex(new Map([[start,0x46],[start+1,0x43],[start+0x1010,0x23]]))});
test('three disjoint images round-trip and leave stock updater staging space',()=>{
  const images=[image('minimal',FLASH_BASE,MAIN_BASE),image('main',MAIN_BASE,VM_BASE),image('vm',VM_BASE,VM_LIMIT)];
  const result=combineHex(images);assert.equal(decodeHex(result.hex).size,9);
  assert.equal(result.regions.length,3);assert.ok(result.stagingBytes>result.imageSpan);
});
test('reject bad checksum, truncated image and data after EOF',()=>{
  const good=image('minimal',FLASH_BASE,MAIN_BASE).text;
  assert.throws(()=>decodeHex(good.replace('46','47')),/checksum/);
  assert.throws(()=>decodeHex(good.replace(':00000001FF','')),/EOF/);
  assert.throws(()=>decodeHex(good+':00000001FF\n'),/Malformed/);
});
test('reject cross-partition records, overlapping images and unsafe updater span',()=>{
  const good=image('minimal',FLASH_BASE,MAIN_BASE);
  assert.throws(()=>combineHex([good,good]),/overlap/);
  assert.throws(()=>combineHex([{...good,end:FLASH_BASE+1}]),/bounds/);
  assert.throws(()=>combineHex([good,image('too-high',0x60600000,0x607c0000)]),/staging/);
});
