// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  ROW_WIDTH, versionBase, tagLimit, validateTag, bannerSuffix, fileSuffix, suffixDefine,
  pagesWithoutWord, chooseEpoch, dirtyFromStatus,
} from './build-tag.mjs';

const V = '0.9';

test('the tag budget is what the version string leaves of the 40-column banner row', () => {
  assert.equal(versionBase('tr-plus', V), 'TeensyROM+ v0.9');
  assert.equal(versionBase('tr', V), 'TeensyROM v0.9');
  assert.equal(tagLimit({ target: 'tr-plus', trVersion: V, dirty: false }), 24);
  assert.equal(tagLimit({ target: 'tr-plus', trVersion: V, dirty: true }), 23);
  assert.equal(tagLimit({ target: 'tr', trVersion: V, dirty: false }), 25);
  assert.equal(tagLimit({ target: 'tr', trVersion: V, dirty: true }), 24);
  // A tag at the limit fills the row exactly, dirty marker included.
  const full = versionBase('tr-plus', V) + bannerSuffix({ tag: 'a'.repeat(23), dirty: true });
  assert.equal(full.length, ROW_WIDTH);
});

test('a tag at the limit passes and one past it is refused with the limit named', () => {
  const opts = { target: 'tr-plus', trVersion: V, dirty: true };
  assert.equal(validateTag('a'.repeat(23), opts), 'a'.repeat(23));
  assert.throws(() => validateTag('a'.repeat(24), opts), /is 24 characters; 23 is the most that fits/);
  assert.equal(validateTag('a'.repeat(24), { ...opts, dirty: false }), 'a'.repeat(24));
  assert.equal(validateTag('a'.repeat(25), { ...opts, target: 'tr', dirty: false }), 'a'.repeat(25));
});

test('a tag is letters, digits, dots and dashes', () => {
  const opts = { target: 'tr-plus', trVersion: V, dirty: false };
  for (const ok of ['pr65', 'geos128', 'rc.1', 'Beta-2']) assert.equal(validateTag(ok, opts), ok);
  for (const bad of ['a b', 'with_underscore', 'pr#65', 'tag*']) {
    assert.throws(() => validateTag(bad, opts), /may only use letters, digits/);
  }
  assert.throws(() => validateTag('', opts), /--tag is empty/);
});

test('the banner and the filenames spell the same build their own way', () => {
  assert.equal(bannerSuffix({ tag: 'pr65', dirty: true }), '-pr65*');
  assert.equal(bannerSuffix({ tag: 'pr65', dirty: false }), '-pr65');
  assert.equal(bannerSuffix({ tag: null, dirty: true }), '*');
  assert.equal(bannerSuffix({ tag: null, dirty: false }), '');
  assert.equal(fileSuffix({ tag: 'pr65', dirty: true }), '-pr65-dirty');
  assert.equal(fileSuffix({ tag: 'pr65', dirty: false }), '-pr65');
  assert.equal(fileSuffix({ tag: null, dirty: true }), '-dirty');
  assert.equal(fileSuffix({ tag: null, dirty: false }), '');
});

test('the define is one quoted token, and nothing at all for a release build', () => {
  assert.equal(suffixDefine('-pr65*'), `'-DTRVersionSuffix="-pr65*"'`);
  assert.equal(suffixDefine(''), '');
});

test('the pages whose word a long version pushes off the row are named', () => {
  assert.deepEqual(pagesWithoutWord('TeensyROM+ v0.9-pr65*'.length), []);
  assert.deepEqual(pagesWithoutWord(31), []);                       // 31 + 9 = 40 still fits
  assert.deepEqual(pagesWithoutWord(32), ['Settings']);             // 32 + 5 = 37 keeps the others
  assert.deepEqual(pagesWithoutWord(36), ['Settings', 'Help', 'port tests']);
});

test('the build date: the environment, then the commit for a clean tree, else now', () => {
  const base = { envEpoch: undefined, commitTime: '1700000000', dirty: false, now: 1800000000 };
  assert.deepEqual(chooseEpoch({ ...base, envEpoch: '1600000000' }), { epoch: '1600000000', source: 'environment' });
  assert.deepEqual(chooseEpoch(base), { epoch: '1700000000', source: 'commit' });
  assert.deepEqual(chooseEpoch({ ...base, dirty: true }), { epoch: '1800000000', source: 'dirty tree' });
  assert.deepEqual(chooseEpoch({ ...base, commitTime: '' }), { epoch: '1800000000', source: 'no git commit' });
  assert.equal(chooseEpoch({ ...base, commitTime: 'fatal: not a git repository' }).source, 'no git commit');
  assert.equal(chooseEpoch({ ...base, envEpoch: '1600000000', dirty: true }).source, 'environment');
});

test('git status: listed files make a dirty tree; no checkout is a clean one, any other failure a dirty one', () => {
  const clean = { dirty: false, files: [], problem: '' };
  assert.deepEqual(dirtyFromStatus({ status: 0, stdout: ' M Source/Teensy/Teensy.ino\n M Source/C64/x.asm\n' }),
    { dirty: true, files: ['Source/Teensy/Teensy.ino', 'Source/C64/x.asm'], problem: '' });
  assert.deepEqual(dirtyFromStatus({ status: 0, stdout: '' }), clean);
  // Not a checkout (an export, or GIT_DIR pointing nowhere) and no git at all: the documented degrade.
  assert.deepEqual(dirtyFromStatus({ status: 128, stdout: '', stderr: "fatal: not a git repository: 'C:/x/not-a-repo'\n" }), clean);
  assert.deepEqual(dirtyFromStatus({ status: 128, stdout: '', stderr: 'fatal: not a git repository (or any of the parent directories): .git\n' }), clean);
  assert.deepEqual(dirtyFromStatus({ status: null, stdout: undefined, stderr: undefined, error: new Error('spawnSync git ENOENT') }), clean);
  // A checkout git could not read is not shown clean.
  const dubious = dirtyFromStatus({ status: 128, stdout: '', stderr: "fatal: detected dubious ownership in repository at 'C:/x'\n" });
  assert.equal(dubious.dirty, true);
  assert.match(dubious.problem, /dubious ownership/);
  assert.deepEqual(dirtyFromStatus({ status: 1, stdout: '', stderr: '' }), { dirty: true, files: [], problem: 'git status exited with 1' });
});
