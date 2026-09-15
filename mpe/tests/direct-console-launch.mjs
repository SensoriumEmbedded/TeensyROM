// SPDX-License-Identifier: MIT
// Actual registry/preflight and DMA helper checks; no ROMs or source export.
// node mpe/tests/direct-console-launch.mjs --packages /path/to/SD-root
// Or supply --nes-package, --doom-package, --gb-package and --gg-package.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
const root=path.resolve(import.meta.dirname,'../..');
const args=process.argv.slice(2);
const option=(name,fallback)=>{const i=args.indexOf(name);if(i<0)return fallback;assert.ok(args[i+1]&&!args[i+1].startsWith('--'),'Missing '+name);return args[i+1];};
const sd=option('--packages');
const sourcePackages=Object.fromEntries([['NESVM','nes'],['DOOMVM','doom'],['GBVM','gb'],['GGVM','gg']]
  .map(([id,optionName])=>[id,option('--'+optionName+'-package',sd&&path.join(sd,'VMS',id))]));
assert.ok(Object.values(sourcePackages).every(Boolean),'Supply --packages SD-root or all four --nes-package, --doom-package, --gb-package and --gg-package');
const compiler=[option('--cxx',process.env.CXX),'g++','clang++','C:/msys64/mingw64/bin/g++.exe'].filter(Boolean)
  .find(c=>spawnSync(c,['--version'],{encoding:'utf8',windowsHide:true}).status===0);
assert.ok(compiler,'A C++17 host compiler is required');
const output=path.resolve(option('--out',path.join(root,'build/direct-console-launch')));
fs.mkdirSync(output,{recursive:true});
const run=fs.mkdtempSync(path.join(output,'run-')),fixture=path.join(run,'packages');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const packageHashes=[];
for(const [id,folder] of Object.entries(sourcePackages)) for(const file of ['manifest.vmi','engine.mvm','client.crt']) {
  const source=path.resolve(folder,file),destination=path.join(fixture,'VMS',id,file);
  assert.ok(fs.statSync(source).isFile());fs.mkdirSync(path.dirname(destination),{recursive:true});fs.copyFileSync(source,destination);
  assert.equal(sha(source),sha(destination));packageHashes.push({path:'VMS/'+id+'/'+file,bytes:fs.statSync(source).size,sha256:sha(source)});
}
const read=file=>fs.readFileSync(path.join(root,file),'utf8').replaceAll('\r\n','\n');
const dmaPath='Source/Teensy/MinimalBoot/Common/DMAControl_Minimal.h';
const definitionsPath='Source/Teensy/MinimalBoot/Common/Common_Defs.h';
const dma=read(dmaPath),definitions=read(definitionsPath);
function extract(signature) {
  const at=dma.indexOf(signature);assert.ok(at>=0,signature);
  const start=dma.lastIndexOf('\n',at)+1,open=dma.indexOf('{',at);let depth=1,end=open+1;
  while(end<dma.length&&depth){if(dma[end]==='{')depth++;if(dma[end]==='}')depth--;end++;}
  assert.equal(depth,0);return dma.slice(start,end);
}
const timing=definitions.split('\n').filter(line=>/^#define Def_nS_DMAData(?:Setup|Hold)(?:PAL|NTSC)\b/.test(line));assert.equal(timing.length,4);
fs.writeFileSync(path.join(run,'direct-console-dma-under-test.h'),timing.join('\n')+'\n'+extract('inline uint8_t DataPortWaitReadDMA()')+'\n'+extract('inline void DataPortWriteWaitDMA(uint8_t Data)')+'\n');
const env={...process.env,PATH:path.dirname(compiler)+path.delimiter+process.env.PATH};
const execute=(name,command,argv)=>{const result=spawnSync(command,argv,{cwd:root,env,encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:4*1024*1024});
  const log=(result.stdout??'')+(result.stderr??'');fs.writeFileSync(path.join(run,name+'.log'),log);assert.equal(result.status,0,result.error||log);return log;};
const executable=path.join(run,'direct-console-launch'+(process.platform==='win32'?'.exe':''));
execute('compile',compiler,['-std=c++17','-O2',...(process.platform==='win32'?['-static']:[]),'-I',run,path.join(root,'mpe/tests/direct-console-launch.cpp'),'-o',executable]);
const result=execute('test',executable,[fixture,fs.mkdtempSync(path.join(run,'launch-sandbox-'))]);
assert.match(result,/PASS: \d+ launch-route checks, 4 released-package preflights, 512 PAL\/NTSC DMA byte cases/);
const sourcePaths=['Source/Teensy/MinimalBoot/Common/VMRegistry.h','Source/Teensy/MinimalBoot/Common/MPELaunch.h',dmaPath,definitionsPath,'vm/tests/fake_sd.h','mpe/tests/direct-console-launch.cpp','mpe/tests/direct-console-launch.mjs'];
const report={status:'PASS',physicalHardware:false,romEmulation:false,scope:'Production MPE launch routing, four actual released engine/client packages, and PAL/NTSC DMA GPIO/time helpers; no MGC1/.MPE routing or host-library execution.',result:result.trim(),packageCount:Object.keys(sourcePackages).length,packageFileCount:packageHashes.length,packageHashes,sources:sourcePaths.map(file=>({path:file,sha256:sha(path.join(root,file))})),runRoot:run};
fs.writeFileSync(path.join(run,'verification.json'),JSON.stringify(report,null,2)+'\n');fs.writeFileSync(path.join(output,'latest.json'),JSON.stringify(report,null,2)+'\n');
console.log(result.trim());console.log('Verification: '+path.join(output,'latest.json'));
