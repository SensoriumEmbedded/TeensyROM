// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadProjects, selectProjects, MANIFEST } from './c64-projects.mjs';
import { headerFileName } from './bin2header.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');

test('the real manifest loads: every source, header input and directory exists', () => {
  const projects = loadProjects(root);
  assert.equal(projects.length, 12);
});

test('every directory under Source/C64 is in the manifest', () => {
  const inManifest = new Set(loadProjects(root).map((p) => path.basename(p.dir)));
  // A new program added under Source/C64 without a manifest entry would never be built.
  const missing = fs.readdirSync(path.join(root, 'Source/C64'), { withFileTypes: true })
    .filter((e) => e.isDirectory() && !inManifest.has(e.name))
    .map((e) => e.name);
  assert.deepEqual(missing, [], `not listed in ${MANIFEST}: ${missing.join(', ')}. `
    + 'Add an entry, or delete the directory if it is build leftovers -- the old KickAssembler '
    + 'script wrote its intermediates to Source/C64/build, which nothing ignores or cleans up now.');
});

test('a header named after its input follows bin2header.py\'s naming unless the manifest says otherwise', () => {
  for (const project of loadProjects(root)) {
    for (const header of project.headers) {
      const expected = headerFileName(path.basename(header.input));
      // The one deliberate rename: the cartridge image is included as TeensyROMC64.h.
      if (header.dest === 'TeensyROMC64.h') continue;
      assert.equal(header.dest, expected, `${project.name}: ${header.input}`);
    }
  }
});

test('project selection ignores case, keeps manifest order, and rejects unknown names', () => {
  const projects = loadProjects(root);
  assert.deepEqual(selectProjects(projects, ['todcheck', 'SettingsMenu']).map((p) => p.name), ['SettingsMenu', 'TODCheck']);
  assert.equal(selectProjects(projects, []).length, projects.length);
  assert.throws(() => selectProjects(projects, ['Nope']), /unknown project "Nope".*SettingsMenu/);
});

// Builds a throwaway repo layout containing only a manifest, to check validation messages.
function withManifest(manifest, fn) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'c64-projects-'));
  try {
    fs.mkdirSync(path.join(dir, 'tools'));
    fs.mkdirSync(path.join(dir, 'proj/source'), { recursive: true });
    fs.writeFileSync(path.join(dir, 'proj/source/a.asm'), '');
    fs.writeFileSync(path.join(dir, MANIFEST), JSON.stringify(manifest));
    return fn(dir);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}

const good = () => ({
  name: 'A',
  dir: 'proj',
  steps: [{ assembler: 'acme', source: 'source/a.asm', output: 'a.prg', format: 'cbm' }],
  headers: [{ input: 'build/a.prg', dest: 'a.prg.h', progmem: true }],
});

test('validation: a header must come from a step or an existing file', () => {
  const p = good();
  p.headers[0].input = 'build/other.prg';
  withManifest({ projects: [p] }, (dir) => assert.throws(() => loadProjects(dir), /not written by any step/));
  p.headers[0].input = 'missing.prg';
  withManifest({ projects: [p] }, (dir) => assert.throws(() => loadProjects(dir), /does not exist/));
});

test('validation: acme needs a format, kickass rejects acme-only fields, and names are unique', () => {
  const noFormat = good();
  delete noFormat.steps[0].format;
  withManifest({ projects: [noFormat] }, (dir) => assert.throws(() => loadProjects(dir), /format must be one of/));

  const kick = good();
  kick.steps[0].assembler = 'kickass';
  withManifest({ projects: [kick] }, (dir) => assert.throws(() => loadProjects(dir), /only apply to acme/));

  const dup = [good(), { ...good(), name: 'a' }];
  withManifest({ projects: dup }, (dir) => assert.throws(() => loadProjects(dir), /duplicate name/));
});

test('validation: two headers may not write the same file', () => {
  const b = { ...good(), name: 'B' };
  withManifest({ projects: [good(), b] }, (dir) => assert.throws(() => loadProjects(dir), /two headers write a\.prg\.h/));
});

test('validation: headers must be present, but may be empty for a program the firmware does not embed', () => {
  const none = good();
  none.headers = [];
  withManifest({ projects: [none] }, (dir) => assert.equal(loadProjects(dir).length, 1));

  const omitted = good();
  delete omitted.headers;
  withManifest({ projects: [omitted] }, (dir) => assert.throws(() => loadProjects(dir), /headers is required/));
});
