// SPDX-License-Identifier: MIT
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {decodeHex,VM_BASE,VM_LIMIT} from './hex.mjs';
import {registryFixture} from './fixtures.mjs';
import {generateNativeData} from '../../experiments/dosvm-nuflix/native-data.mjs';
import {generateDoubleData} from '../../experiments/dosvm-nuflix/double-data.mjs';
import {audioHost} from '../../experiments/dosvm-nuflix/live-audio.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const args=process.argv.slice(2),option=(name,fallback)=>{const i=args.indexOf(name);return i<0?fallback:args[i+1];};
const reportPath=option('--build'),packages=option('--packages');
const allPackages=args.includes('--all-packages');
if(!reportPath||!packages)throw Error('Use --build path/to/latest.json --packages path/to/SD-root');
const build=JSON.parse(fs.readFileSync(reportPath)),sd=path.resolve(packages);
assert.equal(build.mode,'mpe');
const output=path.join(build.runRoot,'verification');fs.mkdirSync(output,{recursive:true});
const compiler=option('--cxx',process.env.CXX??'g++');
const env={...process.env,PATH:path.dirname(compiler)+path.delimiter+process.env.PATH};
const sha=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
const logs=[];
function run(exe,argv){const r=spawnSync(exe,argv,{cwd:root,env,encoding:'utf8',windowsHide:true,maxBuffer:16*1024*1024});
  if(r.error||r.status)throw Error(`${exe} failed\n${r.error??''}\n${r.stdout??''}${r.stderr??''}`);return r.stdout+r.stderr;}
function native(name,argv=[],flags=[]){const exe=path.join(output,name+(process.platform==='win32'?'.exe':''));
  run(compiler,['-std=c++17','-O2',...(process.platform==='win32'?['-static']:[]),...flags,'-I',root,'-I',output,path.join(root,'vm/tests',name+'.cpp'),'-o',exe]);
  const log=run(exe,argv);logs.push(log);console.log(log.trim());return exe;
}
function sourceTest(name,source,argv=[]){
  const executable=path.join(output,name+(process.platform==='win32'?'.exe':''));
  run(compiler,['-std=c++17','-O2',...(process.platform==='win32'?['-static']:[]),'-I',root,'-I',output,path.join(root,source),'-o',executable]);
  const log=run(executable,argv);logs.push(log);console.log(log.trim());
}
assert.equal(sha(build.artifact),build.sha256);
assert.ok(build.inputs?.length,'Build is missing its source input manifest');
for(const input of build.inputs)assert.equal(sha(path.join(root,input.path)),input.sha256,'Built source drift: '+input.path);
logs.push(run(process.execPath,['--test','mpe/tools/hex.test.mjs']));
logs.push(run(process.execPath,['--test','Source/Teensy/tests/recovery-flash-source.test.js','mpe/tests/startup.test.mjs']));
sourceTest('flash-parser','Source/Teensy/tests/flash-update-parser.cpp');
fs.writeFileSync(path.join(output,'audio-host-poll.h'),audioHost(fs.readFileSync(path.join(root,'Source/Teensy/MinimalBoot/VMHostPoll.h'),'utf8')));
assert.equal(sha(path.join(output,'audio-host-poll.h')),sha(path.join(build.runRoot,'source/Source/Teensy/MPEBoot/VMHostPoll.h')),'Tested scheduler differs from firmware');
sourceTest('nuflix-poll','mpe/tests/nuflix-poll.cpp');
native('files_test',[fs.mkdtempSync(path.join(output,'files-sandbox-'))]);
native('packet_replay_test');native('color_f1_test');native('mpe_video_live_test',[path.join(output,'kernel')]);
native('mpe_video_crop_test');native('mpe_video_detail_test');native('mpe_video_sprite_test');
native('full_video_converter_test');native('full_video_kernel_test',[output]);
if(process.platform==='win32'){
  native('indexed_host_test');native('center_video_test',[output]);native('full_video_host_test',[output]);
  native('indexed_ram2_source_test');native('indexed_timing_test');native('color_f1_host_test');
  generateNativeData(path.join(root,'experiments/dosvm-nuflix/upstream-pinned'),output);
  generateDoubleData(output);
  const frames=[0,1,2].map(phase=>{
    const pixels=Buffer.alloc(64000);
    for(let row=0;row<200;++row)for(let column=0;column<320;++column)
      pixels[row*320+column]=((column>>3)^(row>>3)^(column<160?phase:0))&15;
    const filename=path.join(output,'synthetic-nuflix-'+phase+'.idx');fs.writeFileSync(filename,pixels);return filename;
  });
  sourceTest('nuflix-double','experiments/dosvm-nuflix/double-host-test.cpp',[output,...frames]);
}
// Compile the actual stock-adapted swap functions; shims provide hardware IO.
// Existing Teensy pointer casts require -fpermissive on a 64-bit native host.
const easyflash=fs.readFileSync(path.join(root,'Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_EasyFlash.c'),'utf8');
const extract=(signature)=>{
  const definition=new RegExp('^'+signature.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'[^\\r\\n;]*\\r?\\n\\{','m');
  const start=easyflash.search(definition),open=easyflash.indexOf('{',start);
  assert.ok(start>=0&&open>start,signature);
  let depth=1,end=open+1;
  while(end<easyflash.length&&depth){if(easyflash[end]==='{')depth++;if(easyflash[end]==='}')depth--;end++;}
  assert.equal(depth,0,signature+' closes');return easyflash.slice(start,end);
};
fs.writeFileSync(path.join(output,'easyflash-swap-under-test.h'),
  extract('uint8_t* ImageCheckAssign(')+'\n'+extract('void PollingHndlr_EasyFlash()')+'\n');
native('ram1_aux_profile_test',[],['-fpermissive']);
const fixture=registryFixture(fs.mkdtempSync(path.join(output,'synthetic-fixture-')));
native('registry_test',[fixture,fs.mkdtempSync(path.join(output,'registry-sandbox-'))]);
native('upstream_launch_test',[sd,fs.mkdtempSync(path.join(output,'launch-sandbox-')),...(allPackages?['all']:[])]);
const imageTest=native('image_test',[path.join(fixture,'VMS/NESVM/engine.mvm')]);
if(allPackages)for(const id of ['NESVM','DOSVM','AGIVM','GBVM'])logs.push(run(imageTest,[path.join(sd,'VMS',id,'engine.mvm')]));
native('ram2_profile_test',[path.join(sd,'VMS/DOOMVM/engine.mvm')]);
const image=build.images.find(i=>i.name==='vm'),symbols=fs.readFileSync(path.join(build.runRoot,'vm.nm'),'utf8');
const symbol=name=>{const m=symbols.match(new RegExp('^([0-9a-f]+) \\w '+name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'$','m'));assert.ok(m,name);return parseInt(m[1],16);};
assert.equal(symbol('_itcm_block_count'),6);assert.equal(symbol('_flexram_bank_config'),0xaaaaafff);
assert.ok(symbol('_etext')<=0x18000);assert.equal(symbol('_vm_data_start'),0x20014000);
assert.equal(symbol('_vm_data_end'),0x20044000);assert.equal(symbol('_estack'),0x20050000);
assert.ok(symbol('_heap_end')<=0x20014000);assert.ok(symbol('_heap_start')>=0x20000000);
const auxiliaryOwned=symbol('VmRuntime::auxiliaryOwned'),swapStart=symbol('SwapBuffers');
assert.ok(auxiliaryOwned>=0x20000000&&auxiliaryOwned<symbol('_heap_start'),'auxiliary ownership state stays in host RAM1');
assert.ok(swapStart>=0x20000000&&swapStart+16384<=symbol('_heap_start'),'retired swap AUX span stays outside heap/module/stack');
const sizes=fs.readFileSync(path.join(build.runRoot,'vm.size'),'utf8');
assert.match(sizes,/^\.bss.dma\s+0\s/m);assert.match(sizes,/^\.bss.extram\s+0\s/m);
assert.ok(!/nes::|doomgeneric|MPE[4567]|AGIPicture/.test(symbols),'Emulator code leaked into host');
for(const name of ['main','minimal']){
  const ordinary=fs.readFileSync(path.join(build.runRoot,name+'.nm'),'utf8');
  assert.ok(!/VMHostIO2|VMHostPoll|VmRuntime::/.test(ordinary),'VM runtime leaked into ordinary '+name);
}
const bytes=decodeHex(fs.readFileSync(image.hex,'utf8'));
const word=address=>{let v=0;for(let i=0;i<4;i++){assert.ok(bytes.has(address+i));v+=bytes.get(address+i)*2**(i*8);}return v;};
assert.equal(word(VM_BASE),0x42464346);assert.equal(word(VM_BASE+0x1000),0x432000d1);
const entry=word(VM_BASE+0x1004);assert.equal(entry&1,1);assert.ok(entry>=VM_BASE+0x1000&&entry<=VM_BASE+0x3001);
assert.equal(word(VM_BASE+0x1020),VM_BASE);assert.ok(word(VM_BASE+0x1024)<=VM_LIMIT-VM_BASE);
assert.ok(build.layout.stagingBytes>=build.layout.imageSpan);
const upstream='dc1174ce8475153160e0b0da4ff65525a7dd4e5a';
const unchanged=run('git',['ls-tree','-r',upstream,'Source/C64','Source/Teensy/TRMenuFiles','Source/Teensy/MinimalBoot/Min_TeensyROM.h','Source/Teensy/MinimalBoot/Min_DriveDirLoad.ino','Source/Teensy/MinimalBoot/Common/IO_Handlers/IOH_MagicDesk2.c']).trim().split('\n');
for(const row of unchanged){const [metadata,file]=row.split('\t');const expected=metadata.split(' ')[2];assert.equal(run('git',['hash-object','--path='+file,file]).trim(),expected,file+' changed');}
const packageHashes=[];
for(const id of allPackages?['AGIVM','DOSVM','NESVM','GBVM','DOOMVM']:['DOOMVM'])for(const name of ['manifest.vmi','engine.mvm','client.crt'])packageHashes.push({path:'VMS/'+id+'/'+name,sha256:sha(path.join(sd,'VMS',id,name))});
const result={firmwareSha256:build.sha256,passed:true,unchangedUpstreamFiles:unchanged.length,packageHashes,hardwareTested:false,notes:['Host conformance and image checks; no VM engine gameplay or physical hardware acceptance is implied.','Synthetic large CRT files test launch fallthrough, not cartridge emulation.']};
fs.writeFileSync(path.join(output,'tests.log'),logs.join('\n'));fs.writeFileSync(path.join(output,'report.json'),JSON.stringify(result,null,2)+'\n');
console.log('PASS: VM link/boot headers, updater space, '+unchanged.length+' upstream files unchanged. Hardware acceptance remains pending.');
console.log('Verification report: '+path.join(output,'report.json'));
