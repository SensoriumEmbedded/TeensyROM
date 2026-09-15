// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import path from 'node:path';
import {MPE_VERSION,createBuildIdentity,guardFeatureControl} from './build-identity.mjs';

// Any C++ preprocessor is sufficient; CXX can select the pinned ARM compiler.
const compiler=process.env.CXX??'g++';
function preprocess(source,args=[]){
  const result=spawnSync(compiler,['-E','-P','-x','c++',...args,'-'],{
    input:source,encoding:'utf8',windowsHide:true,maxBuffer:1024*1024,
    env:{...process.env,PATH:path.dirname(compiler)+path.delimiter+process.env.PATH}
  });
  assert.ifError(result.error);
  return result;
}
function checked(source,mode='stock',args=[]){
  return preprocess(guardFeatureControl(source,mode),args);
}
function accepted(result){assert.equal(result.status,0,result.stderr);}
function mismatch(result){
  assert.notEqual(result.status,0);
  assert.match(result.stderr,/Build identity mismatch/);
}

test('artifact names preserve stock identity and version MPE independently',()=>{
  assert.equal(MPE_VERSION,'1.2.24');
  const stock=createBuildIdentity('stock','0.8.0.8');
  assert.deepEqual(stock,{mode:'stock',board:'TeensyROM',upstreamVersion:'0.8.0.8',
    mpeVersion:null,artifactFilename:'TeensyROM_0.8.0.8_full.hex'});
  assert.equal(createBuildIdentity('stock-plus','0.8.0.8').artifactFilename,'TeensyROM+_0.8.0.8_full.hex');
  assert.equal(createBuildIdentity('mpe','0.8.0.8').artifactFilename,'TeensyROM+_0.8.0.8_MPE-1.2.24_full.hex');
  assert.equal(createBuildIdentity('mpe','0.8.0.6t').upstreamVersion,'0.8.0.6t');
});

test('invalid modes and missing/unsafe upstream versions fail clearly',()=>{
  assert.throws(()=>createBuildIdentity('unknown','1.0'),/Expected --mode/);
  assert.throws(()=>guardFeatureControl('','unknown'),/Expected --mode/);
  for(const version of [undefined,'','../1.0','1.0/other','1.0\n']){
    assert.throws(()=>createBuildIdentity('mpe',version),/TRVersion/);
  }
});

test('commented examples and similar macro names do not enable TR+',()=>{
  accepted(checked('// #define Fab04_Features\n/*\n#define Fab04_Features\n*/\n'+
    '#define Fab04_Features_Example\nconst char *help="#define Fab04_Features";\n'));
});

test('stock rejects active source definitions, including zero and spaced directives',()=>{
  for(const source of ['#define Fab04_Features\n','  # define Fab04_Features 1\n',
    '#define Fab04_Features 0\n','#define /* board */ Fab04_Features\n']){
    mismatch(checked(source));
  }
});

test('stock rejects an override supplied through compiler flags',()=>{
  mismatch(checked('// The manual feature switch is disabled.\n','stock',['-DFab04_Features']));
});

test('the preprocessor distinguishes inactive and active conditional definitions',()=>{
  accepted(checked('#if 0\n#define Fab04_Features\n#endif\n'));
  const conditional='#if defined(USB_MIDI)\n#define Fab04_Features\n#endif\n';
  accepted(checked(conditional));
  mismatch(checked(conditional,'stock',['-DUSB_MIDI']));
  mismatch(checked('#if 0\n#else\n#define Fab04_Features\n#endif\n'));
});

test('continued directives and continued comments use actual preprocessor rules',()=>{
  mismatch(checked('#define \\\nFab04_Features\n'));
  accepted(checked('// disabled example \\\n#define Fab04_Features\n'));
  accepted(checked('// disabled final comment \\'));
});

test('final macro state controls board identity, including header undef overrides',()=>{
  accepted(checked('#define Fab04_Features\n#undef Fab04_Features\n'));
  for(const mode of ['stock-plus','mpe']){
    accepted(checked('// supplied by the selected mode\n',mode,['-DFab04_Features']));
    mismatch(checked('#undef Fab04_Features\n',mode,['-DFab04_Features']));
    mismatch(checked('',mode));
  }
});

test('successful checks preserve the preprocessed program for all three modes',()=>{
  const source='#ifdef Fab04_Features\nconst int board=4;\n#else\nconst int board=3;\n#endif\n';
  for(const mode of ['stock','stock-plus','mpe']){
    const args=mode==='stock'?[]:['-DFab04_Features'];
    const original=preprocess(source,args),guarded=checked(source,mode,args);
    accepted(original);accepted(guarded);assert.equal(guarded.stdout,original.stdout);
  }
});
