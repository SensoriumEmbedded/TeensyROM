// SPDX-License-Identifier: MIT
//
// Builds the C64-side programs (the 6502 code under Source/C64/) and writes each one as a C
// header into the firmware's ROM directory, where the Teensy build embeds it. One command,
// the same on Windows, macOS and Linux. It replaces the per-project build*.bat files,
// BuildAllC64.bat, SetToolPaths.bat, bin2header.py and gen_menu_regs_i.py.
//
//   npm run build:c64                        build everything
//   npm run build:c64 -- --project SettingsMenu,TODCheck
//   npm run build:c64 -- --list              list the projects and what each one needs
//   npm run build:c64 -- --verbose           show the assemblers' full output
//   npm run build:c64 -- --rom-dir <dir>     write headers there instead of the firmware tree
//
// What gets built, and how, is in tools/c64-projects.json (its format is described in
// tools/lib/c64-projects.mjs). Order of work: regenerate Menu_Regs.i from Menu_Regs.h (four
// projects include it), then each project in manifest order. Like the old scripts, it stops
// at the first failure; unlike them, it never asks you to edit a tracked file: ACME,
// KickAssembler and Java come from an environment variable, PATH, or (ACME and KickAssembler)
// a checksummed download into tools/.cache. See tools/lib/c64-toolchain.mjs.
//
// Each project builds in its own build/ directory (emptied first), which also holds the
// assembly report and the VICE label file, as before.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { binToHeader } from './lib/bin2header.mjs';
import { writeMenuRegsI } from './lib/menu-regs.mjs';
import { loadProjects, selectProjects, ROM_DIR } from './lib/c64-projects.mjs';
import { resolveAcme, resolveKickAssembler, resolveJava } from './lib/c64-toolchain.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
// Repo-relative with forward slashes, or the absolute path for something outside the repo.
const rel = (p) => {
  const r = path.relative(root, p);
  return r.startsWith('..') || path.isAbsolute(r) ? p : r.split(path.sep).join('/');
};

const USAGE = `Usage: node tools/build-c64.mjs [--project <name>[,<name>...]] [--list] [--verbose] [--rom-dir <dir>]`;

function parseArgs(argv) {
  const opts = { projects: [], list: false, verbose: false, romDir: null, help: false };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const value = () => {
      if (!argv[i + 1] || argv[i + 1].startsWith('--')) throw new Error(`Missing value for ${arg}\n${USAGE}`);
      return argv[++i];
    };
    // Trimmed, so `--project "A, B"` works as readily as `--project A,B`.
    if (arg === '--project') opts.projects.push(...value().split(',').map((n) => n.trim()).filter(Boolean));
    else if (arg === '--rom-dir') opts.romDir = path.resolve(value());
    else if (arg === '--list') opts.list = true;
    else if (arg === '--verbose') opts.verbose = true;
    else if (arg === '--help' || arg === '-h') opts.help = true;
    else throw new Error(`Unknown argument ${arg}\n${USAGE}`);
  }
  return opts;
}

// Runs an assembler in the project directory. Its output is shown only when it fails (or
// with --verbose), except that ACME's stderr, where warnings go, is always shown.
function run(command, args, cwd, { verbose }) {
  const result = spawnSync(command, args, { cwd, encoding: 'utf8', stdio: verbose ? 'inherit' : ['ignore', 'pipe', 'pipe'] });
  if (result.error) throw new Error(`could not run ${command}: ${result.error.message}`);
  return result;
}

function assemble(step, project, tools, { verbose }) {
  const dir = path.join(root, project.dir);
  const buildDir = path.join(dir, 'build');
  const output = path.join(buildDir, step.output);

  let command;
  let args;
  if (step.assembler === 'acme') {
    command = tools.acme;
    // ACME rejects any option that comes after a source file ("Options (starting with '-')
    // must be given _before_ source files!"), so step.source stays last.
    args = [
      '-r', path.join(buildDir, step.report ?? 'BuildReport'),
      '--vicelabels', path.join(buildDir, step.labels ?? 'Labels'),
      '--msvc', '--format', step.format,
      `-v${verbose ? 3 : 1}`,
      ...(verbose && process.stdout.isTTY ? ['--color'] : []),
      '--outfile', output,
      step.source,
    ];
  } else {
    command = tools.java;
    args = ['-jar', tools.kickassJar, '-showmem', step.source, '-o', output, '-odir', buildDir];
  }

  const result = run(command, args, dir, { verbose });
  if (result.status !== 0) {
    if (!verbose) process.stderr.write(`${result.stdout ?? ''}${result.stderr ?? ''}`);
    throw new Error(`${project.name}: assembling ${step.source} with ${step.assembler} failed (exit ${result.status})`);
  }
  if (!fs.existsSync(output)) throw new Error(`${project.name}: ${step.assembler} succeeded but did not write ${rel(output)}`);
  if (!verbose && result.stderr) process.stderr.write(result.stderr);

  console.log(`   ${step.assembler.padEnd(7)} ${step.source} -> ${rel(output)} (${fs.statSync(output).size.toLocaleString('en-US')} bytes)`);
}

function writeHeader(header, project, romDir) {
  const bytes = fs.readFileSync(path.join(root, project.dir, header.input));
  const dest = path.join(romDir, header.dest);
  fs.writeFileSync(dest, binToHeader(bytes, { name: path.basename(header.input), typemod: header.progmem ? 'PROGMEM ' : '' }));
  console.log(`   header  ${rel(dest)}${header.progmem ? '' : '  (RAM, no PROGMEM)'}`);
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.help) {
    console.log(USAGE);
    return;
  }

  const projects = loadProjects(root);
  if (opts.list) {
    const width = Math.max(...projects.map((p) => p.name.length));
    for (const p of projects) {
      const needs = [...new Set(p.steps.map((s) => s.assembler))];
      console.log(`${p.name.padEnd(width)}  ${(needs.join(', ') || 'no assembler').padEnd(11)}  ${p.description ?? ''}`);
    }
    return;
  }

  const selected = selectProjects(projects, opts.projects);
  const romDir = opts.romDir ?? path.join(root, ROM_DIR);
  fs.mkdirSync(romDir, { recursive: true });
  const cacheDir = path.join(root, 'tools/.cache');

  // Resolve a tool only when something selected needs it.
  const needs = new Set(selected.flatMap((p) => p.steps.map((s) => s.assembler)));
  const tools = {};
  if (needs.has('acme')) tools.acme = resolveAcme(cacheDir);
  if (needs.has('kickass')) {
    tools.java = resolveJava();
    tools.kickassJar = resolveKickAssembler(cacheDir);
  }

  console.log('== Menu_Regs.i');
  console.log(`   generated ${rel(writeMenuRegsI(root))} from Menu_Regs.h`);

  for (const project of selected) {
    console.log(`\n== ${project.name}`);
    if (project.steps.length) {
      const buildDir = path.join(root, project.dir, 'build');
      fs.rmSync(buildDir, { recursive: true, force: true });
      fs.mkdirSync(buildDir, { recursive: true });
    }
    for (const step of project.steps) assemble(step, project, tools, opts);
    for (const header of project.headers) writeHeader(header, project, romDir);
  }

  console.log(`\nBuilt ${selected.length} project${selected.length === 1 ? '' : 's'}.`);
}

try {
  main();
} catch (error) {
  console.error(`\nerror: ${error.message}`);
  process.exit(1);
}
