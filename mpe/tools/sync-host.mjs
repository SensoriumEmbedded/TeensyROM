// SPDX-License-Identifier: MIT
// Import committed generic host files, never a developer's in-progress tree.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const [source,ref]=process.argv.slice(2);
if(!source||!ref)throw Error('Usage: node mpe/tools/sync-host.mjs <source-checkout> <committed-revision>');
const git=args=>execFileSync('git',['-C',source,...args],{windowsHide:true,maxBuffer:16*1024*1024});
const revision=git(['rev-parse','--verify',ref+'^{commit}']).toString().trim();
const lockPath=path.join(root,'mpe/source-lock.json');
const lock=JSON.parse(fs.readFileSync(lockPath));
const adapted=new Set(lock.adaptations.map(a=>a.path));
for(const p of ['vm/video/mpe_video_camera.h','vm/video/mpe_video_sprites.h',
  'vm/tests/mpe_video_crop_test.cpp','vm/tests/mpe_video_detail_test.cpp','vm/tests/mpe_video_sprite_test.cpp',
  'vm/tests/center_video_test.cpp','vm/tests/full_video_converter_test.cpp',
  'vm/tests/full_video_host_test.cpp','vm/tests/full_video_kernel_test.cpp',
  'vm/tests/helpers/indexed_video_fixture.h']){
  if(!lock.files.some(f=>f.path===p))lock.files.push({path:p});
}
// Read every source before writing anything, to reject an incomplete revision.
const imported=lock.files.map(file=>({file,bytes:git(['show',revision+':'+(file.sourcePath??file.path)])}));
for(const {file,bytes} of imported){
  file.sha256=crypto.createHash('sha256').update(bytes).digest('hex');
  if(adapted.has(file.path))continue;
  fs.mkdirSync(path.dirname(path.join(root,file.path)),{recursive:true});
  fs.writeFileSync(path.join(root,file.path),bytes);
}
lock.revision=revision;
fs.writeFileSync(lockPath,JSON.stringify(lock,null,2)+'\n');
console.log('Imported generic host at '+revision+'; preserved documented platform adapters.');
