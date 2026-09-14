// SPDX-License-Identifier: MIT
// Execute the actual staged MPE text-menu button callers against the helper.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import {spawnSync} from 'node:child_process';
const root=path.resolve(import.meta.dirname,'../..');
const option=(name,fallback)=>{const i=process.argv.indexOf(name);return i<0?fallback:process.argv[i+1];};
const buildPath=option('--build');
const build=buildPath?JSON.parse(fs.readFileSync(buildPath)):null;
if(build)assert.equal(build.mode,'mpe','Only the MPE staging adapter uses this helper');
const source=option('--source',build?path.join(build.runRoot,'source/Source'):null);
assert(source,'Use --build path/to/latest.json or --source path/to/staged/Source');
const sourceRoot=path.resolve(source),sketch=path.join(sourceRoot,'Teensy/Teensy.ino');
const helper=path.resolve(option('--helper',path.join(sourceRoot,'Teensy/ButtonDebounce.h')));
const hash=p=>crypto.createHash('sha256').update(fs.readFileSync(p)).digest('hex');
assert.equal(hash(helper),hash(path.join(root,'mpe/host/ButtonDebounce.h')),'Staged helper differs from the recorded MPE adapter');
const compiler=[option('--cxx'),process.env.CXX,'g++','clang++','C:/msys64/mingw64/bin/g++.exe'].filter(Boolean)
 .find(exe=>spawnSync(exe,['--version'],{encoding:'utf8',windowsHide:true}).status===0);
assert(compiler,'A C++11 host compiler is required');
const temporary=fs.mkdtempSync(path.join(os.tmpdir(),'mpe-button-debounce-'));
try{
 const code=fs.readFileSync(sketch,'utf8');
 assert.match(code,/#include "ButtonDebounce\.h"/);
 assert.doesNotMatch(code,/#include\s*[<"]Bounce\.h[>"]/);
 const alias=code.match(/^#define Bounce ButtonDebounce\s*$/m);assert(alias,'MPE alias adapter is missing');
 assert.match(code,/pinMode\(Special_Btn_In_PIN, INPUT_PULLUP\)/);
 const declaration=code.match(/^\s*Bounce SpecialBtnBounce = Bounce\(Special_Btn_In_PIN, 35\);[^\r\n]*/m);
 assert(declaration,'Compile the actual upstream object declaration');
 const loopStart=code.indexOf('   if (SpecialBtnBounce.update())');
 const loopEnd=code.indexOf('\n#endif',loopStart);assert(loopStart>=0&&loopEnd>loopStart);
 const statusPath=path.join(sourceRoot,'Teensy/MinimalBoot/Common/IO_Handlers/StatusFunctions.c');
 const handlerPath=path.join(sourceRoot,'Teensy/MinimalBoot/Common/IO_Handlers/IOH_TeensyROM.c');
 const status=fs.readFileSync(statusPath,'utf8'),handler=fs.readFileSync(handlerPath,'utf8');
 const external=handler.match(/^extern Bounce SpecialBtnBounce;/m);assert(external,'Compile the actual shared declaration');
 const statusStart=status.indexOf('//Alt Button:'),statusEnd=status.indexOf('//USB Device',statusStart);
 assert(statusStart>=0&&statusEnd>statusStart);
 fs.writeFileSync(path.join(temporary,'production-button-callers.h'),
   alias[0]+'\n'+declaration[0]+'\n'+external[0]+'\nvoid productionLoopButton() {\n'+code.slice(loopStart,loopEnd)+
   '\n}\nvoid productionStatusButton() {\n'+status.slice(statusStart,statusEnd)+'\n}\n');
 fs.writeFileSync(path.join(temporary,'Arduino.h'),'#pragma once\n#include <stdint.h>\n#define LOW 0\n#define HIGH 1\nint digitalRead(uint8_t);\nuint32_t millis();\n');
 fs.copyFileSync(helper,path.join(temporary,'ButtonDebounce.h'));
 const executable=path.join(temporary,'button-debounce'+(process.platform==='win32'?'.exe':''));
 const env={...process.env,PATH:path.dirname(compiler)+path.delimiter+process.env.PATH};
 const compiled=spawnSync(compiler,['-std=c++11','-Wall','-Wextra','-Werror','-I',temporary,path.join(import.meta.dirname,'button-debounce.cpp'),'-o',executable],{encoding:'utf8',env,windowsHide:true});
 assert.equal(compiled.status,0,compiled.error||compiled.stdout+compiled.stderr);
 const result=spawnSync(executable,[],{encoding:'utf8',env,windowsHide:true,timeout:5000});
 assert.equal(result.status,0,result.error||result.stdout+result.stderr);
 assert.match(result.stdout,/10 debounce and production integration scenarios passed/);
 const report={passed:true,scenarios:10,desktopResetCallbacks:0,sourceRoot,helperSha256:hash(helper),testedSourceFiles:[sketch,statusPath,handlerPath].map(file=>({path:path.relative(sourceRoot,file).replaceAll('\\','/'),sha256:hash(file)}))};
 if(option('--report'))fs.writeFileSync(option('--report'),JSON.stringify(report,null,2)+'\n');
 console.log(result.stdout.trim());console.log(JSON.stringify(report,null,2));
}finally{
 assert.equal(path.dirname(temporary),path.resolve(os.tmpdir()));
 fs.rmSync(temporary,{recursive:true,force:true});
}
