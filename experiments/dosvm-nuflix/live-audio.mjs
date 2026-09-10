import assert from 'node:assert/strict';

const replace = (source, before, after) => {
  assert.equal(source.split(before).length, 2, 'Audio integration hook changed: ' + before);
  return source.replace(before, after);
};

export function audioModule(source) {
  source = source.replaceAll('\r\n', '\n');
  source = replace(source, 'static uint32_t BiosCrc;', 'static uint32_t MPE5AudioStamp;\nstatic uint32_t BiosCrc;');
  source = replace(source, '   MPE5TandyRevision = MPE5Tandy.revision();',
    '   MPE5TandyRevision = MPE5Tandy.revision();\n   MPE5AudioStamp = ModuleHost->micros_now();');
  source = replace(source, '      if(result==VmVideoResult::Busy)return;',
    '      if(result==VmVideoResult::Busy){\n' +
    '         if(uint32_t(ModuleHost->micros_now()-MPE5AudioStamp)>=16000u&&\n' +
    '            (MPE5Speaker.revision()!=MPE5SpeakerRevision||MPE5Tandy.revision()!=MPE5TandyRevision))MPE5PublishFrameEnd();\n' +
    '         return;\n' +
    '      }');
  return source;
}

export function audioHost(source) {
  source = source.replaceAll('\r\n', '\n');
  source = replace(source, '    if(failure||pending)return;',
    '    if(failure)return;\n' +
    '    if(!quietRequested&&indexedVideo.nuflix&&indexedVideo.phase==6){\n' +
    '        if(!transferIndexedVideoSlice()){fail(0x17);return;}\n' +
    '    }\n' +
    '    if(pending)return;');
  source = replace(source, '    if(indexedVideo.phase==6){if(!transferIndexedVideoSlice()){fail(0x17);return;}if(indexedVideo.phase==6)return;}',
    '    if(indexedVideo.phase==6&&!indexedVideo.nuflix){if(!transferIndexedVideoSlice()){fail(0x17);return;}if(indexedVideo.phase==6)return;}');
  return source;
}
