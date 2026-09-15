// SPDX-License-Identifier: MIT
// MPE package identity is independent of the upstream TeensyROM version.
export const MPE_VERSION='1.2.24';
// The integration can advance without rebuilding an unchanged host archive.
export const MPE_LIBRARY_VERSION='1.2.23';
const modes=new Set(['stock','stock-plus','mpe']);

function checkMode(mode){
  if(!modes.has(mode))throw Error('Expected --mode stock, stock-plus or mpe');
}

export function createBuildIdentity(mode,upstreamVersion){
  checkMode(mode);
  if(typeof upstreamVersion!=='string'||!/^[0-9A-Za-z][0-9A-Za-z._+-]*$/.test(upstreamVersion)){
    throw Error('Expected a valid TRVersion from Common_Defs.h');
  }
  const board=mode==='stock'?'TeensyROM':'TeensyROM+';
  const mpeVersion=mode==='mpe'?MPE_VERSION:null;
  return {
    mode,board,upstreamVersion,mpeVersion,
    artifactFilename:`${board}_${upstreamVersion}${mpeVersion?'_MPE-'+mpeVersion:''}_full.hex`
  };
}

// Apply only to the staged copy of Fab04FeatureCtl.h, before compiling images.
// Let the real C/C++ preprocessor evaluate comments, conditionals and each
// image's compiler flags. A text search cannot distinguish #if 0 from an
// enabled board override, and a value of zero still enables #ifdef features.
// These directives emit no code and leave the original header text intact.
export function guardFeatureControl(source,mode){
  checkMode(mode);
  if(typeof source!=='string')throw TypeError('Expected Fab04FeatureCtl.h source text');
  const stock=mode==='stock';
  const condition=stock?'defined(Fab04_Features)':'!defined(Fab04_Features)';
  const message=stock?
    'Build identity mismatch: --mode stock requires Fab04_Features to be undefined. Remove the override in Fab04FeatureCtl.h/compiler flags, or select --mode stock-plus or --mode mpe.':
    `Build identity mismatch: --mode ${mode} requires Fab04_Features. Remove a header override that undefines it and retain the TR+ compiler flag.`;
  return source+'\n\n// Validate the requested board after the feature-control header.\n'+
    `#if ${condition}\n#error "${message}"\n#endif\n`;
}
