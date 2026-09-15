// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {gameCartFixture} from './game-cart-fixture.mjs';
const root=path.resolve(import.meta.dirname,'../..');
test('MGC1 launches through actual text browser and minimal boot without a registry',()=>{
  const output=path.join(root,'build/game-cart-launch');fs.mkdirSync(output,{recursive:true});
  const run=fs.mkdtempSync(path.join(output,'run-')),read=p=>fs.readFileSync(path.join(root,p),'utf8').replaceAll('\r\n','\n');
  fs.writeFileSync(path.join(run,'bundle.MPE'),gameCartFixture());
  const drive=read('Source/Teensy/DriveDirLoad.ino'),assoc=read('Source/Teensy/MinimalBoot/Common/DriveDirLoad.h');
  const slice=(source,start,end)=>{const a=source.indexOf(start),b=source.indexOf(end,a);assert.ok(a>=0&&b>a,start);return source.slice(a,b);};
  const functionBody=(source,signature)=>{const start=source.indexOf(signature),open=source.indexOf('{',start);assert.ok(start>=0&&open>start);let depth=1,end=open+1;while(depth&&end<source.length){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}assert.equal(depth,0);return source.slice(start,end);};
  const tableStart=assoc.indexOf('   struct StructExt_ItemType_Assoc'),tableEnd=assoc.indexOf('\n};',assoc.indexOf('StructExt_ItemType_Assoc Ext_ItemType_Assoc[]'));
  assert.ok(tableStart>=0&&tableEnd>tableStart);
  fs.writeFileSync(path.join(run,'game-cart-text-under-test.h'),assoc.slice(tableStart,tableEnd+3)+'\n'+functionBody(drive,'uint8_t Assoc_Ext_ItemType(char * FileName)'));
  fs.writeFileSync(path.join(run,'game-cart-browser-under-test.h'),slice(drive,'FLASHMEM void HandleExecution()','   if (MenuSelCpy.ItemType == rtNone)')+'++fallbacks;\n}\n');
  const minimal=read('Source/Teensy/MinimalBoot/MinimalBoot.ino');
  fs.writeFileSync(path.join(run,'game-cart-boot-under-test.h'),'static void bootUnderTest(){\n'+slice(minimal,'   char vmMarker[5]{};','\n#endif')+'\n'+slice(minimal,'   if(!mpeSdInitialized)','   BigBuf =').replace('#endif','')+'++ordinaryBoots;\n}\n');
  const compiler=[process.env.CXX,'g++','clang++','C:/msys64/mingw64/bin/g++.exe'].filter(Boolean).find(c=>spawnSync(c,['--version'],{encoding:'utf8',windowsHide:true}).status===0);assert.ok(compiler,'C++17 compiler required');
  const env={...process.env,PATH:path.dirname(compiler)+path.delimiter+process.env.PATH},exe=path.join(run,'test'+(process.platform==='win32'?'.exe':''));
  const execute=(command,args)=>{const r=spawnSync(command,args,{cwd:root,env,encoding:'utf8',windowsHide:true,timeout:60000,maxBuffer:4*1024*1024});assert.equal(r.status,0,r.error??r.stdout+r.stderr);return r.stdout;};
  execute(compiler,['-std=c++17','-O2','-DMPE_VM_ENABLED',...(process.platform==='win32'?['-static']:[]),'-I',run,path.join(root,'mpe/tests/game-cart-launch.cpp'),'-o',exe]);
  const result=execute(exe,[run]);assert.match(result,/PASS: \d+ MPE text-browser\/parser\/boot assertions/);console.log(result.trim());
  fs.writeFileSync(path.join(output,'latest.json'),JSON.stringify({passed:true,scope:'Actual public MGC1 text-browser, preflight, extension and minimal boot functions with synthetic SD/EEPROM/reset; no compiled-host execution or hardware acceptance.',runRoot:run,result:result.trim()},null,2)+'\n');
});
