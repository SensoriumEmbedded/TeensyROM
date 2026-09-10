import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';

const root=path.resolve(import.meta.dirname,'../..');
const read=filename=>JSON.parse(fs.readFileSync(filename));
const sha=filename=>crypto.createHash('sha256').update(fs.readFileSync(filename)).digest('hex');
const builds=process.argv.slice(2).map(read);
assert.equal(builds.length,3,'Supply MPE, stock-plus and stock latest.json reports');
assert.deepEqual(builds.map(build=>build.mode),['mpe','stock-plus','stock']);
for(const build of builds){
  assert.equal(sha(build.artifact),build.sha256);
  for(const input of build.inputs)assert.equal(sha(path.join(root,input.path)),input.sha256,input.path+' changed after build');
}
const build=builds[0],verification=read(path.join(build.runRoot,'verification/report.json'));
assert.equal(verification.firmwareSha256,build.sha256);assert.equal(verification.passed,true);
assert.equal(verification.unchangedUpstreamFiles,219);
function memory(build,name){
  const symbols=fs.readFileSync(path.join(build.runRoot,name+'.nm'),'utf8');
  const symbol=label=>parseInt(symbols.match(new RegExp('^([0-9a-f]+) \\w '+label+'$','m'))[1],16);
  const sections=fs.readFileSync(path.join(build.runRoot,name+'.size'),'utf8');
  const size=label=>Number(sections.match(new RegExp('^'+label.replaceAll('.','\\.')+'\\s+(\\d+)\\s','m'))?.[1]??0);
  const physical=size('.text.itcm')+size('.fini')+size('.ARM.exidx');
  return {itcmCodeBytes:symbol('_etext'),itcmPhysicalBytes:physical,itcmBanks:symbol('_itcm_block_count'),
    dtcmStaticBytes:symbol('_ebss')-0x20000000,linkedEndToStackGapBytes:symbol('_estack')-symbol('_ebss'),
    ram2StaticBytes:size('.bss.dma'),psramStaticBytes:size('.bss.extram'),heapStart:symbol('_heap_start'),heapEnd:symbol('_heap_end')};
}
const memoryMap=Object.fromEntries(build.images.map(image=>[image.name,memory(build,image.name)]));
assert.ok(memoryMap.vm.itcmPhysicalBytes<=98304&&memoryMap.vm.ram2StaticBytes===0);
assert.equal(memoryMap.main.itcmBanks,7);assert.ok(memoryMap.main.linkedEndToStackGapBytes>=25000);
const review=path.join(root,'mpe/review'),filename=path.basename(build.artifact);
const artifact=path.join(review,filename);
assert.ok(!fs.existsSync(artifact),'Do not overwrite an earlier review artifact');
fs.copyFileSync(build.artifact,artifact);assert.equal(sha(artifact),build.sha256);
const testLog=path.join(build.runRoot,'verification/tests.log');
fs.copyFileSync(testLog,path.join(review,'host-1.2.6-tests.log'));
const lock=read(path.join(root,'mpe/source-lock.json'));
const tests=['mpe/tools/verify.mjs','mpe/tests/startup.test.mjs','Source/Teensy/tests/recovery-flash-source.test.js','mpe/tests/nuflix-poll.cpp','Source/Teensy/tests/flash-update-parser.cpp'];
const record={upstreamBase:'dc1174ce8475153160e0b0da4ff65525a7dd4e5a',sharedHostRevision:lock.revision,
  artifact:{path:'mpe/review/'+filename,bytes:fs.statSync(artifact).size,sha256:build.sha256},
  toolchain:{arduinoCli:'1.4.1',teensyCore:'1.61.0',gcc:'11.3.1',optimization:'o2std',board:'TeensyROM+ Fab0.4'},
  memory:memoryMap,vmReservations:{hostCode:98304,hostHeap:16384,moduleData:196608,executionStack:49152,
    legacyModuleCode:98304,auxProfileModuleCode:65536,auxProfileNonExecutableItcmTail:32768},
  layout:build.layout,images:build.images.map(image=>({name:image.name,sha256:image.sha256})),
  comparisonBuilds:builds.slice(1).map(other=>({mode:other.mode,sha256:other.sha256,mainMemory:memory(other,'main'),hardwareTested:false})),
  verification,tests:tests.map(file=>({path:file,sha256:sha(path.join(root,file))})),
  testLogSha256:sha(testLog),sourceInputs:build.inputs,hardwareTested:false,
  notes:['The original text UI and 219 protected upstream files remain unchanged.',
    'Firmware contains the host, not DOS/SCI/SCUMM or other emulator engines or private games.',
    'Startup checks exclude button-triggered flashing; parser tests stub flash operations.',
    'PAL/NTSC double-buffer tests simulate DMA. Physical gameplay, audio and updater acceptance remain pending.',
    'Component licenses are documented in docs/MPE-FIRMWARE-NOTICES.md.']};
fs.writeFileSync(path.join(review,'host-1.2.6-verification.json'),JSON.stringify(record,null,2)+'\n');
console.log(JSON.stringify({artifact:record.artifact,memory:record.memory,passed:true,hardwareTested:false},null,2));
