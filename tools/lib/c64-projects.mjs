// SPDX-License-Identifier: MIT
//
// Loads and checks tools/c64-projects.json, the list of C64 programs the build handles.
// Checking everything up front means a typo in the manifest is reported before any
// assembler runs.
//
// Each project:
//   name         what you pass to --project
//   dir          the project's directory (assemblers run with it as the working directory)
//   steps        assemblies to run, in order, each writing build/<output>:
//                  assembler  "acme" or "kickass"
//                  source     path relative to dir
//                  output     file name in build/
//                  format     acme only: "cbm" keeps the 2-byte load address at the start of
//                             the file, "plain" leaves it off (for ROM images)
//                  report, labels   acme only: file names in build/ for the assembly report
//                             and the VICE label file (default BuildReport, Labels)
//   headers      C headers to write into the firmware's ROM directory. Required, but [] is fine
//                for a program the firmware does not embed (an extension's client cartridge):
//                  input      path relative to dir: a step's build/<output>, or an existing file
//                  dest       header file name
//                  progmem    true puts the array in flash (PROGMEM); false keeps it in RAM
import fs from 'node:fs';
import path from 'node:path';

export const ROM_DIR = 'Source/Teensy/TRMenuFiles/ROMs';
export const MANIFEST = 'tools/c64-projects.json';

const ASSEMBLERS = ['acme', 'kickass'];
const FORMATS = ['cbm', 'plain'];

function fail(where, message) {
  throw new Error(`${MANIFEST}: ${where}: ${message}`);
}

function isPlainFileName(value) {
  return typeof value === 'string' && value !== '' && value === path.basename(value) && !value.includes('\\');
}

export function loadProjects(root) {
  const { projects } = JSON.parse(fs.readFileSync(path.join(root, MANIFEST), 'utf8'));
  if (!Array.isArray(projects) || projects.length === 0) fail('projects', 'expected a non-empty array');

  const names = new Set();
  const headerDests = new Set();
  for (const project of projects) {
    const where = `project ${JSON.stringify(project.name)}`;
    if (typeof project.name !== 'string' || !project.name) fail(where, 'name is required');
    if (names.has(project.name.toLowerCase())) fail(where, 'duplicate name');
    names.add(project.name.toLowerCase());
    if (typeof project.dir !== 'string' || !fs.statSync(path.join(root, project.dir), { throwIfNoEntry: false })?.isDirectory()) {
      fail(where, `dir ${JSON.stringify(project.dir)} is not a directory`);
    }

    const steps = project.steps ?? [];
    if (!Array.isArray(steps)) fail(where, 'steps must be an array');
    const outputs = new Set();
    // build/ file names the steps write besides their output, so two steps can't collide on
    // a defaulted report or label file and silently overwrite each other.
    const sideFiles = new Set();
    for (const step of steps) {
      if (!ASSEMBLERS.includes(step.assembler)) fail(where, `assembler must be one of ${ASSEMBLERS.join(', ')}`);
      // `?? ''` would resolve to the project directory, which always exists, so check the type first.
      if (typeof step.source !== 'string' || !step.source) fail(where, 'source is required');
      if (!fs.existsSync(path.join(root, project.dir, step.source))) fail(where, `source ${JSON.stringify(step.source)} does not exist`);
      if (!isPlainFileName(step.output)) fail(where, `output ${JSON.stringify(step.output)} must be a file name`);
      if (outputs.has(step.output)) fail(where, `two steps write build/${step.output}`);
      outputs.add(step.output);
      if (step.assembler === 'acme') {
        if (!FORMATS.includes(step.format)) fail(where, `${step.source}: format must be one of ${FORMATS.join(', ')}`);
        for (const [key, fallback] of [['report', 'BuildReport'], ['labels', 'Labels']]) {
          if (step[key] !== undefined && !isPlainFileName(step[key])) fail(where, `${key} must be a file name`);
          const name = step[key] ?? fallback;
          if (sideFiles.has(name)) fail(where, `two steps write build/${name}; give one of them its own "${key}"`);
          sideFiles.add(name);
        }
      } else if (step.format !== undefined || step.report !== undefined || step.labels !== undefined) {
        fail(where, `${step.source}: format, report and labels only apply to acme`);
      }
    }

    if (!Array.isArray(project.headers)) fail(where, 'headers is required (use [] for a program the firmware does not embed)');
    for (const header of project.headers) {
      if (typeof header.input !== 'string' || !header.input) fail(where, 'header input is required');
      if (typeof header.progmem !== 'boolean') fail(where, `${header.input}: progmem must be true or false`);
      if (!isPlainFileName(header.dest) || !header.dest.endsWith('.h')) fail(where, `dest ${JSON.stringify(header.dest)} must be a .h file name`);
      if (headerDests.has(header.dest.toLowerCase())) fail(where, `two headers write ${header.dest}`);
      headerDests.add(header.dest.toLowerCase());
      if (header.input.startsWith('build/')) {
        if (!outputs.has(header.input.slice('build/'.length))) fail(where, `header input ${header.input} is not written by any step`);
      } else if (!fs.existsSync(path.join(root, project.dir, header.input))) {
        fail(where, `header input ${JSON.stringify(header.input)} does not exist`);
      }
    }
  }
  return projects.map((p) => ({ ...p, steps: p.steps ?? [] }));
}

// names: what the user asked for (empty = everything). Matching ignores case; the result
// keeps the manifest's order, which is also a valid build order.
export function selectProjects(projects, names) {
  if (names.length === 0) return projects;
  const wanted = new Set(names.map((n) => n.toLowerCase()));
  const unknown = names.filter((n) => !projects.some((p) => p.name.toLowerCase() === n.toLowerCase()));
  if (unknown.length) {
    throw new Error(`unknown project ${unknown.map((n) => JSON.stringify(n)).join(', ')}; the projects are: ${projects.map((p) => p.name).join(', ')}`);
  }
  return projects.filter((p) => wanted.has(p.name.toLowerCase()));
}
