import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {MPE_VERSION} from './build-identity.mjs';
import {checkFlashHeadroom} from '../../tools/lib/flash-headroom.mjs';

const root=path.resolve(import.meta.dirname,'../..');
const read=filename=>JSON.parse(fs.readFileSync(filename));
const sha=filename=>crypto.createHash('sha256').update(fs.readFileSync(filename)).digest('hex');
const reports=process.argv.slice(2).map(read);
const builds=reports.slice(0,3),baselines=reports.slice(3);
assert.equal(builds.length,3,'Supply MPE, stock-plus and stock latest.json reports');
assert.deepEqual(builds.map(build=>build.mode),['mpe','stock-plus','stock']);
assert.ok(baselines.length===0||baselines.length===2,'Optionally supply upstream stock-plus and stock baseline reports');
for(let i=0;i<baselines.length;i++){
  const baseline=baselines[i],candidate=builds[i+1];
  assert.equal(baseline.sourceRevision,'80ba6378b4417b284d3e212f65befd8c9b25d968');
  assert.equal(baseline.mode,candidate.mode);
  assert.equal(sha(baseline.artifact),baseline.sha256);
  assert.equal(candidate.sha256,baseline.sha256,'Stock output differs from upstream baseline');
}
for(const build of builds){
  assert.equal(sha(build.artifact),build.sha256);
  for(const input of build.inputs)assert.equal(sha(path.join(root,input.path)),input.sha256,input.path+' changed after build');
}
const build=builds[0],verification=read(path.join(build.runRoot,'verification/report.json'));
assert.equal(verification.firmwareSha256,build.sha256);assert.equal(verification.passed,true);
assert.equal(verification.unchangedUpstreamFiles,221);
const headroom=checkFlashHeadroom(root,build.artifact);
assert.equal(headroom.status,'OK');
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
fs.copyFileSync(testLog,path.join(review,`host-${MPE_VERSION}-tests.log`));
const lock=read(path.join(root,'mpe/source-lock.json'));
const tests=['mpe/tools/verify.mjs','mpe/tools/build-identity.mjs','mpe/tools/build-identity.test.mjs',
  'mpe/tools/library-verification.mjs','mpe/tools/library-verification.test.mjs','mpe/tools/upstream-flash-verification.test.mjs',
  'mpe/tests/startup.test.mjs','mpe/tests/direct-console-launch.mjs','mpe/tests/direct-console-launch.cpp',
  'mpe/tests/button-debounce.mjs','mpe/tests/button-debounce.cpp','mpe/tests/sync-host.test.mjs',
  'mpe/tests/game-cart-launch.test.mjs','mpe/tests/game-cart-launch.cpp','mpe/tests/game-cart-fixture.mjs'];
const record={upstreamBase:'80ba6378b4417b284d3e212f65befd8c9b25d968',legacyPublicSourceRevision:lock.revision,
  identity:build.identity,library:{version:build.library.version,packageRevision:build.library.packageRevision,
    archive:'mpe/library/'+path.basename(build.library.archive),sha256:build.library.sha256,
    manifestSha256:build.library.manifestSha256,sourceFirmwareSha256:build.library.sourceFirmwareSha256,
    relinkSdkSha256:sha(path.join(root,'mpe/library/Relink-SDK.zip'))},
  artifact:{path:'mpe/review/'+filename,bytes:fs.statSync(artifact).size,sha256:build.sha256},
  toolchain:{arduinoCli:'1.4.1',teensyCore:'1.61.0',gcc:'11.3.1',optimization:'o2std',board:'TeensyROM+ Fab0.4'},
  memory:memoryMap,vmReservations:{hostCode:98304,hostHeap:16384,moduleData:196608,executionStack:49152,
    legacyModuleCode:98304,auxProfileModuleCode:65536,auxProfileNonExecutableItcmTail:32768},
  layout:build.layout,selfUpdate:{status:headroom.status,spanBytes:headroom.sizeBytes,
    steadyStateLimitBytes:headroom.steadyStateMax,headroomBytes:headroom.headroomBytes},
  images:build.images.map(image=>({name:image.name,sha256:image.sha256})),
  correspondingLibrarySources:{path:`mpe/review/firmware-${MPE_VERSION}-library-sources.zip`,
    sha256:sha(path.join(review,`firmware-${MPE_VERSION}-library-sources.zip`))},
  comparisonBuilds:builds.slice(1).map((other,index)=>({mode:other.mode,sha256:other.sha256,mainMemory:memory(other,'main'),
    upstreamBaselineSha256:baselines[index]?.sha256,byteIdenticalToUpstream:baselines[index]?true:undefined,hardwareTested:false})),
  verification,tests:tests.map(file=>({path:file,sha256:sha(path.join(root,file))})),
  testLogSha256:sha(testLog),sourceInputs:build.inputs,hardwareTested:false,
  notes:['The original text UI and 221 protected upstream files remain unchanged.',
    'Firmware contains the host, not DOS/SCI/SCUMM or other emulator engines or private games.',
    'The FXUtil rollback from upstream remains intact; HEX bounds and self-update headroom are checked during the build.',
    'The compiled package is the complete MPE host with Prism+, with public interface and corresponding-source relink SDK.',
    'Source tests simulate DMA and launch routing. Physical gameplay, audio, buttons and updater acceptance remain pending.',
    'Component licenses are documented in THIRD-PARTY-NOTICES.md.']};
fs.writeFileSync(path.join(review,`host-${MPE_VERSION}-verification.json`),JSON.stringify(record,null,2)+'\n');
console.log(JSON.stringify({artifact:record.artifact,memory:record.memory,passed:true,hardwareTested:false},null,2));
