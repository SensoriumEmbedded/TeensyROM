// SPDX-License-Identifier: MIT
//
// Reports pinned build dependencies that have a newer release. Dependabot can't see these
// pins (they live in `run:` shell lines and JS constants, not a manifest it understands),
// so .github/workflows/check-pins.yml runs this weekly and keeps one issue up to date.
//
//   node tools/check-pins.mjs [--report <file>]
//
// Writes a Markdown report (stdout, or --report) and, under GitHub Actions, sets the step
// output `outdated` to the number of dependencies with an update. A pin that can't be found
// where it's expected is an error, so reformatting an install line can't silently turn the
// check off.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { resolveArduinoCli } from './lib/toolchain.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const TEENSY_INDEX = 'https://www.pjrc.com/teensy/package_teensy_index.json';

// Every place each version is pinned, including the docs that state it, so a bump that
// misses one fails here. All of a dependency's pins must agree.
const DEPENDENCIES = [
  {
    name: 'CRC32 library',
    pins: [
      ['.github/workflows/build.yml', /lib install CRC32@([\d.]+)/],
      ['.github/workflows/experiment.yml', /lib install CRC32@([\d.]+)/],
      ['Source/BuildInfo.md', /CRC32 at version ([\d.]+)/],
    ],
    latest: () => arduinoCliJson(['lib', 'search', 'CRC32']).libraries.find((l) => l.name === 'CRC32').latest.version,
  },
  {
    name: 'Teensy core (teensy:avr)',
    pins: [
      ['.github/workflows/build.yml', /core install teensy:avr@([\d.]+)/],
      ['.github/workflows/experiment.yml', /core install teensy:avr@([\d.]+)/],
      ['tools/build-firmware.mjs', /TEENSY_CORE_VERSION = '([\d.]+)'/],
      ['Source/BuildInfo.md', /\* Teensyduino ([\d.]+)/],
      ['docs/Architecture/Build-System.md', /Current pinned\/recommended version is \*\*([\d.]+)\*\*/],
      ['CONTRIBUTING.md', /Build against \*\*Teensyduino ([\d.]+)\*\*/],
    ],
    latest: () => arduinoCliJson(['core', 'search', 'teensy:avr', '--additional-urls', TEENSY_INDEX]).platforms[0].latest_version,
    // Releases we've decided against, so they aren't re-reported every week. A later
    // release is still reported.
    rejected: {
      '1.62.0': 'its GCC 15.2.1 toolchain causes intermittent SD-read stalls with 2 PSRAM chips (see Source/BuildInfo.md)',
    },
  },
  {
    name: 'arduino-cli',
    pins: [['tools/lib/toolchain.mjs', /PIN_VERSION = '([\d.]+)'/]],
    latest: async () => (await githubLatestRelease('arduino/arduino-cli')).replace(/^v/, ''),
  },
  {
    // No `latest`: ACME is released on SourceForge, which offers no release API we could ask
    // without scraping the download page. The value here is catching a bump that updates the
    // constant (and so the download URL, built from it) but leaves the docs saying 0.97.
    name: 'ACME (C64 assembler)',
    pins: [
      ['tools/lib/c64-toolchain.mjs', /ACME_VERSION = '([\d.]+)'/],
      ['Source/C64/README.md', /\*\*ACME\*\* ([\d.]+)/],
      ['Source/BuildInfo.md', /acme-crossass\/\) ([\d.]+)/],
      ['docs/Architecture/C64-Software.md', /ACME cross-assembler ([\d.]+)\*\*/],
      ['docs/Architecture/Build-System.md', /ACME cross-assembler ([\d.]+)/],
    ],
  },
  {
    // No `latest`: KickAssembler is published at a single unversioned URL on theweb.dk, so a
    // new release is caught by the download's checksum failing, not by this check.
    name: 'KickAssembler',
    pins: [
      ['tools/lib/c64-toolchain.mjs', /KICKASS_VERSION = '([\d.]+)'/],
      ['Source/C64/README.md', /\*\*KickAssembler\*\* ([\d.]+)/],
    ],
  },
];

function pinnedVersion({ name, pins }) {
  const versions = pins.map(([file, pattern]) => {
    const match = fs.readFileSync(path.join(root, file), 'utf8').match(pattern);
    if (!match) throw new Error(`${name}: no pin matching ${pattern} in ${file}`);
    return match[1];
  });
  if (new Set(versions).size > 1) {
    throw new Error(`${name}: pins disagree (${pins.map(([file], i) => `${file}=${versions[i]}`).join(', ')})`);
  }
  return versions[0];
}

let cli;
function arduinoCli(argv) {
  cli ??= resolveArduinoCli(path.join(root, 'tools/.cache'));
  const result = spawnSync(cli, argv, { encoding: 'utf8', windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  if (result.error || result.status) throw new Error(`arduino-cli ${argv.join(' ')} failed\n${result.error ?? ''}${result.stderr}`);
  return result.stdout;
}
const arduinoCliJson = (argv) => JSON.parse(arduinoCli([...argv, '--format', 'json']));

async function githubLatestRelease(repo) {
  const headers = { Accept: 'application/vnd.github+json' };
  if (process.env.GITHUB_TOKEN) headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  const response = await fetch(`https://api.github.com/repos/${repo}/releases/latest`, { headers });
  if (!response.ok) throw new Error(`GitHub ${repo} latest release: HTTP ${response.status}`);
  return (await response.json()).tag_name;
}

const args = process.argv.slice(2);
const reportIndex = args.indexOf('--report');
const reportPath = reportIndex >= 0 ? args[reportIndex + 1] : null;

// Both searches read the local index, so refresh it first (a fresh CI runner has none).
arduinoCli(['lib', 'update-index']);
arduinoCli(['core', 'update-index', '--additional-urls', TEENSY_INDEX]);

const rows = [];
let outdated = 0;
for (const dep of DEPENDENCIES) {
  const pinned = pinnedVersion(dep);
  // A dependency with no `latest` is checked only for cross-file agreement: its publisher
  // offers no way to ask what the newest release is (see the note on each such entry).
  const latest = dep.latest ? await dep.latest() : 'not checked';
  let status = dep.latest ? 'up to date' : 'pins agree';
  if (dep.latest && latest !== pinned) {
    const reason = dep.rejected?.[latest];
    status = reason ? `skipped: ${reason}` : '**update available**';
    if (!reason) outdated++;
  }
  rows.push(`| ${dep.name} | ${pinned} | ${latest} | ${status} | ${dep.pins.map(([file]) => `\`${file}\``).join('<br>')} |`);
}

const report = [
  outdated ? `${outdated} pinned build ${outdated === 1 ? 'dependency has' : 'dependencies have'} a newer release.` : 'All pinned build dependencies are up to date.',
  '',
  '| Dependency | Pinned | Latest | Status | Pinned in |',
  '|---|---|---|---|---|',
  ...rows,
  '',
  'To bump a pin, update every file listed for it; this check fails until they agree. A bump can change the firmware image, so rebuild and test on hardware before merging. ' +
    'To stop reporting a release, add it to `rejected` in `tools/check-pins.mjs` with the reason.',
  '',
  '_Generated by `tools/check-pins.mjs` (`.github/workflows/check-pins.yml`)._',
].join('\n');

if (reportPath) fs.writeFileSync(reportPath, report + '\n');
console.log(report);
if (process.env.GITHUB_OUTPUT) fs.appendFileSync(process.env.GITHUB_OUTPUT, `outdated=${outdated}\n`);
