// SPDX-License-Identifier: MIT
// Builds in a private source/core copy. Never flashes hardware or edits the SDK.
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {combineHex,FLASH_BASE,FLASH_LIMIT,MAIN_BASE,VM_BASE,VM_LIMIT} from './hex.mjs';
import {generateNativeData} from '../../experiments/dosvm-nuflix/native-data.mjs';
import {generateDoubleData} from '../../experiments/dosvm-nuflix/double-data.mjs';
import {audioHost} from '../../experiments/dosvm-nuflix/live-audio.mjs';

const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const args=process.argv.slice(2), option=(name,fallback)=>{
  const i=args.indexOf(name);if(i<0)return fallback;
  if(!args[i+1]||args[i+1].startsWith('--'))throw Error('Missing value for '+name);
  return args[i+1];
};
const mode=option('--mode','mpe');
if(!['stock','stock-plus','mpe'].includes(mode))throw Error('Expected --mode stock, stock-plus or mpe');
const cli=option('--arduino-cli',process.env.ARDUINO_CLI??'arduino-cli');
const sdk=path.resolve(option('--arduino-data',process.env.ARDUINO_DIRECTORIES_DATA??path.join(process.env.LOCALAPPDATA??path.join(os.homedir(),'.local/share'),'Arduino15')));
const libraries=path.resolve(option('--arduino-user',process.env.ARDUINO_DIRECTORIES_USER??path.join(os.homedir(),'Documents/Arduino')));
const output=path.resolve(option('--out',path.join(root,'build/mpe',mode)));
const hardware=path.join(sdk,'packages/teensy/hardware/avr/1.61.0');
if(!fs.existsSync(hardware))throw Error('Install Teensy core 1.61.0, or specify --arduino-data');
const write=(p,b)=>{fs.mkdirSync(path.dirname(p),{recursive:true});fs.writeFileSync(p,b);};
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
const read=p=>fs.readFileSync(p,'utf8');
function inputs(){
  const files=[];
  const walk=dir=>{for(const e of fs.readdirSync(path.join(root,dir),{withFileTypes:true})){
    if(e.name==='build')continue;const p=path.join(dir,e.name);
    if(e.isDirectory())walk(p);else files.push({path:p.replaceAll('\\','/'),sha256:sha(fs.readFileSync(path.join(root,p)))});
  }};
  for(const dir of ['Source','mpe/host','vm/video','experiments/dosvm-nuflix'])walk(dir);
  for(const p of ['mpe/tools/build.mjs','mpe/tools/hex.mjs'])files.push({path:p,sha256:sha(fs.readFileSync(path.join(root,p)))});
  return files.sort((a,b)=>a.path.localeCompare(b.path));
}
const inputSnapshot=inputs();
fs.mkdirSync(output,{recursive:true});
const runRoot=fs.mkdtempSync(path.join(output,'run-'));
const stage=path.join(runRoot,'source'),data=path.join(runRoot,'Arduino15');
if(process.platform==='win32'&&data.length>70)throw Error('Use a shorter --out path, for example C:/MPE-build, to stay within Windows toolchain path limits');
fs.cpSync(path.join(root,'Source'),path.join(stage,'Source'),{recursive:true,filter:p=>!path.relative(path.join(root,'Source'),p).split(path.sep).includes('build')});
if(mode==='mpe'){
  fs.cpSync(path.join(root,'vm/video'),path.join(stage,'vm/video'),{recursive:true});
  const nuflix=path.join(stage,'experiments/dosvm-nuflix');
  fs.cpSync(path.join(root,'experiments/dosvm-nuflix'),nuflix,{recursive:true});
  generateNativeData(path.join(nuflix,'upstream-pinned'),nuflix);
  generateDoubleData(nuflix);
  const poll=path.join(stage,'Source/Teensy/MinimalBoot/VMHostPoll.h');
  write(poll,audioHost(read(poll)));
  const vmSketch=path.join(stage,'Source/Teensy/MPEBoot');
  fs.cpSync(path.join(stage,'Source/Teensy/MinimalBoot'),vmSketch,{recursive:true});
  fs.unlinkSync(path.join(vmSketch,'MinimalBoot.ino'));
  fs.copyFileSync(path.join(root,'mpe/host/MinimalBoot.ino'),path.join(vmSketch,'MPEBoot.ino'));
  fs.copyFileSync(path.join(root,'mpe/host/Min_TeensyROM.h'),path.join(vmSketch,'Min_TeensyROM.h'));
}
fs.cpSync(hardware,path.join(data,'packages/teensy/hardware/avr/1.61.0'),{recursive:true});
fs.symlinkSync(path.join(sdk,'packages/teensy/tools'),path.join(data,'packages/teensy/tools'),process.platform==='win32'?'junction':'dir');
for(const entry of fs.readdirSync(sdk,{withFileTypes:true}))if(entry.isFile())fs.copyFileSync(path.join(sdk,entry.name),path.join(data,entry.name));
const core=path.join(data,'packages/teensy/hardware/avr/1.61.0/cores/teensy4');
const linkers=path.join(root,'Source/Teensy/tools/BootLinkerFiles');
const env={...process.env,ARDUINO_DIRECTORIES_DATA:data,ARDUINO_DIRECTORIES_USER:libraries,SOURCE_DATE_EPOCH:'1788566400'};
function run(exe,argv){
  const r=spawnSync(exe,argv,{cwd:root,env,encoding:'utf8',windowsHide:true,maxBuffer:48*1024*1024});
  if(r.error||r.status)throw Error(`${exe} failed\n${r.error??''}\n${(r.stdout+r.stderr).slice(-14000)}`);
  return r.stdout+r.stderr;
}
const arm=path.join(sdk,'packages/teensy/tools/teensy-compile/11.3.1/arm/bin/arm-none-eabi-');
const exe=process.platform==='win32'?'.exe':'';
const images=[];
function compile(name,min,{extra='',ld=null,bootdata=null,usb=null,sketchName=null}={}){
  const suffix=min?'orig':'upper';
  write(path.join(core,'imxrt1062_t41.ld'),ld??read(path.join(linkers,'imxrt1062_t41.ld.'+suffix)));
  write(path.join(core,'bootdata.c'),bootdata??read(path.join(linkers,'bootdata.c.'+suffix)));
  const sketch=path.join(stage,'Source/Teensy',sketchName??(min?'MinimalBoot':''));
  const fqbn=`teensy:avr:teensy41:usb=${min?'serial':'serialmidi'},speed=600,opt=o2std,keys=en-us`;
  const props=run(cli,['compile','--fqbn',fqbn,'--show-properties',sketch]);
  const defs=props.match(/^build.flags.defs=(.*)$/m)?.[1].trim();
  if(!defs)throw Error('Cannot resolve Teensy compiler flags');
  const build=path.join(runRoot,name);
  const options=['compile','--fqbn',fqbn,'--build-path',build,'--build-property',
    'build.flags.defs='+defs+(mode==='stock'?'':' -DFab04_Features')+extra];
  if(usb)options.push('--build-property','build.usbtype='+usb);
  console.log('Building '+name);
  const result=spawnSync(cli,[...options,sketch],{cwd:root,env,encoding:'utf8',windowsHide:true,maxBuffer:48*1024*1024});
  const log=(result.stdout??'')+(result.stderr??'');write(path.join(runRoot,name+'.log'),log);
  if(result.error||result.status)throw Error(`${name} build failed; see ${path.join(runRoot,name+'.log')}\n${result.error??''}\n${log.slice(-14000)}`);
  console.log(log.split(/\r?\n/).filter(l=>/Memory Usage|RAM1:|RAM2:|FLASH:/.test(l)).join('\n'));
  const stem=sketchName??(min?'MinimalBoot':'Teensy'),elf=path.join(build,stem+'.ino.elf');
  const symbols=run(arm+'nm'+exe,['-n','-C',elf]);write(path.join(runRoot,name+'.nm'),symbols);
  const sizes=run(arm+'size'+exe,['-A',elf]);write(path.join(runRoot,name+'.size'),sizes);
  const image={name,elf,hex:path.join(build,stem+'.ino.hex'),symbols,sizes};images.push(image);return image;
}
const enabled=mode==='mpe'?' -DMPE_VM_ENABLED':'';
compile('minimal',true,{extra:enabled,ld:read(path.join(linkers,'imxrt1062_t41.ld.orig')).replace('LENGTH = 7936K','LENGTH = 384K')});
compile('main',false,{extra:enabled,ld:read(path.join(linkers,'imxrt1062_t41.ld.upper')).replace('LENGTH = 7552K',`LENGTH = ${(VM_BASE-MAIN_BASE)/1024}K`)});
if(mode==='mpe'){
  const yieldSource=read(path.join(core,'yield.cpp'));
  // Core 1.61 leaves this reference unguarded when USB is disabled.
  write(path.join(core,'yield.cpp'),yieldSource.replace('if (Serial.available()) serialEvent();','#ifndef USB_DISABLED\n\tif (Serial.available()) serialEvent();\n#endif'));
  let ld=read(path.join(linkers,'imxrt1062_t41.ld.orig'))
    .replace('ORIGIN = 0x60000000, LENGTH = 7936K',`ORIGIN = 0x${VM_BASE.toString(16)}, LENGTH = ${(VM_LIMIT-VM_BASE)/1024}K`)
    .replace('_itcm_block_count = (SIZEOF(.text.itcm) + SIZEOF(.ARM.exidx) + 0x7FFF) >> 15;','_itcm_block_count = 6;')
    .replace('_heap_start = ADDR(.bss.dma) + SIZEOF(.bss.dma);','_heap_start = ALIGN(_ebss, 32) + 32;')
    .replace('_heap_end = ORIGIN(RAM) + LENGTH(RAM);','_heap_end = _heap_start + 16384;')
    .replace('_teensy_model_identifier = 0x25;',`_teensy_model_identifier = 0x25;
      _vm_data_start = 0x20014000; _vm_data_end = 0x20044000;
      ASSERT(_etext <= 0x18000, "Host overlaps module ITCM")
      ASSERT(_heap_end <= _vm_data_start, "Host heap overlaps module RAM1")
      ASSERT(_estack - _vm_data_end >= 49152, "VM stack below 48 KiB")
      ASSERT(SIZEOF(.bss.dma) == 0, "Host globals overlap guest RAM2")
      ASSERT(SIZEOF(.bss.extram) == 0, "Host requires PSRAM")`);
  const bootdata=read(path.join(linkers,'bootdata.c.orig')).replace('0x60000000,','0x'+VM_BASE.toString(16)+',');
  compile('vm',true,{sketchName:'MPEBoot',ld,bootdata,usb:'USB_DISABLED',extra:' -DMHS_VM_PROFILE_192_320 -DMPE_DOS_NUFLIX -DMPE_DOS_NUFLIX_DOUBLE -I'+stage.replaceAll('\\','/')});
}
const combined=combineHex(images.map((image,i)=>({name:image.name,text:read(image.hex),start:[FLASH_BASE,MAIN_BASE,VM_BASE][i],end:[MAIN_BASE,VM_BASE,VM_LIMIT][i]})));
const version=read(path.join(root,'Source/Teensy/MinimalBoot/Common/Common_Defs.h')).match(/#define TRVersion\s+"([^"]+)"/)[1];
const filename=`TeensyROM${mode==='stock'?'':'+'}_${version}${mode==='mpe'?'_MPE-1.2.6':''}_full.hex`;
const artifact=path.join(runRoot,filename);write(artifact,combined.hex);
if(JSON.stringify(inputs())!==JSON.stringify(inputSnapshot))throw Error('Source changed during build; do not use these artifacts');
const report={mode,sourceRevision:run('git',['rev-parse','HEAD']).trim(),inputs:inputSnapshot,runRoot,artifact,sha256:sha(fs.readFileSync(artifact)),layout:{regions:combined.regions,imageSpan:combined.imageSpan,stagingStart:combined.stagingStart,stagingBytes:combined.stagingBytes},images:images.map(({name,elf,hex,symbols})=>({name,elf,hex,sha256:sha(fs.readFileSync(hex)),itcmEnd:symbols.match(/^([0-9a-f]+) \w _etext$/m)?.[1]}))};
write(path.join(runRoot,'report.json'),JSON.stringify(report,null,2)+'\n');
write(path.join(output,'latest.json'),JSON.stringify(report,null,2)+'\n');
console.log('Build report: '+path.join(runRoot,'report.json'));
