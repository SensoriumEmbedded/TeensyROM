#!/usr/bin/env node
// SPDX-License-Identifier: MIT
//
// Flash a built firmware image to an attached TeensyROM over USB.
//
//   node tools/flash-firmware.mjs                     # newest hex in build/firmware
//   node tools/flash-firmware.mjs --hex <path>
//   node tools/flash-firmware.mjs --check             # identify only, write nothing
//   node tools/flash-firmware.mjs --all               # every board, each with its own type's image
//   node tools/flash-firmware.mjs --help              # every option, and how the board is chosen
//
// The point of this tool is the check it does BEFORE writing: a TR and a TR+
// image are both valid, both build cleanly, and are not interchangeable. The
// running firmware refuses a mismatched image at its own updater with
// "Verify file for TeensyROM+: Failed!", but only after you have carried the
// file to an SD card. Here we compare the target string compiled into the hex
// against the banner the attached board reports, and refuse early. With more
// than one board attached, the same comparison (or the chip ID) decides which
// board is written, and the others are watched so a write that reaches the
// wrong board is caught.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn, spawnSync, execFileSync } from 'node:child_process';
import { findBoards, identify, boardKind, teensyTool } from './lib/trserial.mjs';
import { inspectImage, planFlash, describeBoard, sameBoard, Refusal, LABEL } from './lib/flash-plan.mjs';

// Refusals here are expected outcomes (wrong target, no board, no loader), so
// report them as a message and an exit code rather than a stack trace.
function fail(message) { console.error(`\n${message}`); process.exit(1); }

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
// Repo-relative, or the absolute path for an image outside the repo.
const shown = (file) => {
  const relative = path.relative(root, file);
  return relative.startsWith('..') || path.isAbsolute(relative) ? path.resolve(file) : relative;
};

const USAGE = 'Usage: node tools/flash-firmware.mjs [--hex <file>]... [--uid <chip ID> | --port <port>] [--all] [--check] [--help]';

const HELP = `${USAGE}

Writes a built firmware image to a TeensyROM on USB. It first asks the board
what it is and refuses an image built for the other cartridge (a TeensyROM
image on a TeensyROM+, or the reverse), before anything is written.

Options:
  --hex <file>      The image to write. Default: the newest .hex in build/firmware.
                    Give it twice with --all, once per cartridge type.
  --uid <chip ID>   Write the board with this chip ID (the UID in its version
                    reply; --check lists every board's).
  --port <port>     Write the board on this serial port (COM12, /dev/cu.usbmodem...).
  --all             Write every attached board with the image built for its type,
                    one board at a time, stopping at the first failure. Default
                    images: the newest TeensyROM and the newest TeensyROM+ .hex in
                    build/firmware.
  --check           Identify the boards and show what would be written; write nothing.
  --help, -h        Show this help.

Which board is written:
  - One board attached: that board.
  - --uid or --port: that board.
  - Otherwise, the one board whose type matches the image. If two boards match,
    none does, or a board does not answer, the tool lists what is attached and
    stops; name the board with --uid.
  - A board already waiting in the bootloader cannot be asked what it is. On its
    own it is written on the image's word. Beside another board the tool refuses,
    because the Teensy Loader writes to whichever board it finds there.

After each write the board must come back with its own chip ID, running the
image's build date, and every other board must still report what it did before.
TeensyROM settings survive: a USB write leaves the flash that holds them alone.

Writing: on Windows through the Teensy Loader that Teensyduino installs, with
no button to press. On macOS and Linux with teensy_loader_cli, and a press of
the program button on the board named; with several boards attached, the
button is what chooses the board, so press the right one.

Examples:
  node tools/flash-firmware.mjs --check
      list the attached boards and the image that would be written
  node tools/flash-firmware.mjs --hex build/firmware/TeensyROM+_0.8.0.11_full.hex
      one board, or a TeensyROM beside a TeensyROM+: the TeensyROM+ is written
  node tools/flash-firmware.mjs --hex build/firmware/TeensyROM+_0.8.0.11_full.hex --uid 19277260
      two TeensyROM+ boards: name the one to write
  node tools/flash-firmware.mjs --all
      a TeensyROM and a TeensyROM+: each gets the newest image built for it
  node tools/flash-firmware.mjs --all --hex build/firmware/TeensyROM_0.8.0.11_full.hex --hex build/firmware/TeensyROM+_0.8.0.11_full.hex`;

function parseArgs(argv) {
  const opts = { hex: [], uid: null, port: null, all: false, check: false, help: false };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    const value = () => {
      if (!argv[i + 1] || argv[i + 1].startsWith('--')) throw new Error(`Missing value for ${arg}\n${USAGE}`);
      return argv[++i];
    };
    if (arg === '--hex') opts.hex.push(value());
    else if (arg === '--uid') opts.uid = value();
    else if (arg === '--port') opts.port = value();
    else if (arg === '--all') opts.all = true;
    else if (arg === '--check') opts.check = true;
    else if (arg === '--help' || arg === '-h') opts.help = true;
    else throw new Error(`Unknown argument ${arg}\n${USAGE}`);
  }
  return opts;
}

// The .hex files in build/firmware, newest first.
function builtHexes() {
  const dir = path.join(root, 'build/firmware');
  if (!fs.existsSync(dir)) return [];
  return fs.readdirSync(dir).filter((name) => name.endsWith('.hex'))
    .map((name) => path.join(dir, name))
    .sort((a, b) => fs.statSync(b).mtimeMs - fs.statSync(a).mtimeMs);
}

function readImage(file) {
  if (!fs.existsSync(file)) fail(`No such hex: ${file}`);
  return { path: file, ...inspectImage(fs.readFileSync(file, 'utf8')) };
}

// The newest image of each cartridge type, for --all with no --hex.
function newestImagePerKind() {
  const images = [];
  for (const file of builtHexes()) {
    const image = readImage(file);
    if (image.kind && !images.some((i) => i.kind === image.kind)) images.push(image);
  }
  return images;
}

function printImage(image) {
  console.log(`Image: ${shown(image.path)}`);
  if (!image.kind) fail('This hex carries no TeensyROM target ID. It is not a TR firmware image; refusing to write it.');
  console.log(`  built for ${LABEL[image.kind]}  (${image.id})${image.stamp ? `, built ${image.stamp}` : ''}`);
  for (const [from, to] of image.regions) {
    console.log(`  0x${from.toString(16).padStart(8, '0')} .. 0x${to.toString(16).padStart(8, '0')}` +
      `   ${((to - from) / 1024).toFixed(1)}K`);
  }
}

// Every board with what it says it is running; a board in the bootloader
// cannot be asked. One version exchange per running board, about 3 s each.
function identifyBoards(found) {
  return found.map((board) => {
    if (board.bootloader) return { ...board, version: null, built: null, uid: null, kind: null };
    const { version, built, uid, minimal } = identify(board.port);
    return { ...board, version, built, uid, minimal, kind: boardKind(version) };
  });
}

// The Teensy Loader app stays open after a write with the last image loaded and
// in Auto mode, so a later press of any board's program button would write
// that image to it. Closed once the writes are done and checked.
function closeTeensyLoader() {
  if (process.platform !== 'win32') return;
  spawnSync('taskkill', ['/IM', 'teensy.exe', '/F'], { stdio: 'ignore' });
}

// macOS and Linux write with PJRC's teensy_loader_cli.
function writeWithLoaderCli(file, board) {
  let loader;
  try { loader = execFileSync('command', ['-v', 'teensy_loader_cli'], { shell: true, encoding: 'utf8' }).trim(); }
  catch { loader = ''; }
  if (!loader) {
    fail('teensy_loader_cli is not installed.\n' +
      '  macOS:  brew install teensy_loader_cli\n' +
      '  Linux:  https://www.pjrc.com/teensy/loader_cli.html');
  }

  // -w waits for the board to appear in HalfKay. TeensyROM exposes no HID
  // rebootor, so -s (soft reboot) does not work on it and the button is the
  // reliable way in; teensy_reboot is the alternative but needs the GUI loader.
  // -w takes the first board that appears there, so the button chooses the board.
  console.log(`\nWriting. Press the program button on the board at ${board.port ?? board.location ?? 'the USB port'}` +
    ' if it does not start within a few seconds.');
  const write = spawnSync(loader, ['--mcu=TEENSY41', '-w', '-v', file], { stdio: 'inherit' });
  if (write.status !== 0) fail(`teensy_loader_cli exited ${write.status}`);
}

// Windows: PJRC ships no teensy_loader_cli build there, but Teensyduino installs
// teensy_post_compile, which is what the Arduino IDE uploads with. It hands the
// hex to the Teensy Loader app and, with -reboot, asks the running board into
// HalfKay; if the board does not go, the program button still works. Whether it
// returns before or after the write, the board is watched until it has left
// serial and come back, so the check below reads the new firmware.
async function writeWithTeensyLoader(file, board) {
  const postCompile = teensyTool('teensy_post_compile');
  if (!postCompile) fail('Teensyduino is not installed: teensy_post_compile was not found in the Arduino data directory.');
  const args = [
    `-file=${path.basename(file, '.hex')}`,
    `-path=${path.dirname(path.resolve(file))}`,
    `-tools=${path.dirname(postCompile)}`,
    '-board=TEENSY41',
    '-reboot',
  ];
  // The board's whole address, as the Arduino IDE passes it. -port on its own
  // is not enough: with two Teensys attached, teensy_post_compile set it aside
  // ("Found 2 Teensy boards, but using auto-search") and rebooted the other one.
  // A board already in the bootloader is attached alone (planFlash refuses
  // otherwise), and auto-search finds it.
  if (board.port && board.location) {
    args.push(`-port=${board.location}`, `-portlabel=${board.label}`, '-portprotocol=Teensy');
  }

  console.log('\nWriting through the Teensy Loader. Press the program button on the Teensy if it does not start within a few seconds.');
  let status = null;
  spawn(postCompile, args, { stdio: 'inherit' }).on('exit', (code) => { status = code; });

  let left = board.bootloader;
  let strayed = 0;
  const deadline = Date.now() + 120000;
  while (Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 500));
    if (status !== null && status !== 0) fail(`teensy_post_compile exited ${status}`);
    const now = findBoards();
    const waiting = now.filter((b) => b.bootloader).length;
    const target = now.find((b) => b.port && sameBoard(b, board));
    // A board in the bootloader while the target is still running, or two of
    // them at once, means the reboot reached another board, and the Teensy
    // Loader is about to write this image to it. Closing the loader stops that
    // if it has not begun; that board then waits in the bootloader, unharmed.
    // Two sightings in a row, so a listing caught mid-re-enumeration cannot
    // stop a write that is going to the right board.
    strayed = (target && waiting) || waiting > 1 ? strayed + 1 : 0;
    if (strayed >= 2) {
      closeTeensyLoader();
      fail(`STOPPED: another board went into the bootloader instead of ${board.port ?? 'the one being written'}, so this\n` +
        'image could reach it. The Teensy Loader is closed.\n' +
        'That board keeps its bootloader and cannot be harmed by a failed write. To put it right:\n' +
        '  1. Unplug every other board.\n' +
        '  2. If it is still waiting in the bootloader, run this tool with the image built for THAT board;\n' +
        '     a board in the bootloader is written on the image\'s word, so pick the image with care.\n' +
        '  3. Otherwise run --check, and if it now reports the wrong type, press its program button and do step 2.');
    }
    if (!target) left = true;
    else if (left && status === 0) return;
  }
  fail('The board did not come back within 2 minutes. Check the Teensy Loader window, then re-run with --check.');
}

// The written board once its full firmware is back on serial and answering.
// The COM port comes back before the firmware answers on it, and MinimalBoot
// answers first on a port of its own, naming its own build, so this waits for
// the full firmware's banner. If only MinimalBoot has answered after 20 s, that
// answer is returned for the check to report; if nothing has, null.
async function waitForBoard(board) {
  const deadline = Date.now() + 20000;
  let minimal = null;
  while (Date.now() < deadline) {
    const back = findBoards().find((b) => b.port && sameBoard(b, board));
    if (back) {
      const info = identify(back.port, 1500);
      if (info.version && !info.minimal) return { back, info };
      if (info.minimal) minimal = { back, info };
    } else {
      await new Promise((resolve) => setTimeout(resolve, 500));
    }
  }
  return minimal;
}

// Confirm what is actually running, rather than trusting that the write took:
// the written board must answer with its own chip ID and the image's build
// date, and every other board must be as it was. Note what the date can and
// cannot tell you: it comes from the HEAD commit (see sourceDateEpoch in
// build-firmware.mjs), not from the moment of compilation, so it tells one
// commit's firmware from another's but says nothing about a rebuild of
// uncommitted edits. Commit first if you need the banner to prove which build
// is on the board. Returns the written board as it now reports itself, or null
// when it has not come back yet.
async function checkAfterWrite(board, image, others) {
  const answered = await waitForBoard(board);
  if (!answered) {
    console.log('\nWritten. Board has not re-enumerated yet — re-run with --check to confirm.');
    return null;
  }
  const { back, info } = answered;
  const now = findBoards();
  const problems = [];
  if (info.minimal) problems.push(`${back.port} is still in MinimalBoot: the full firmware did not start within 20 s`);
  if (board.uid && info.uid !== board.uid) problems.push(`${back.port} answers as chip ${info.uid ?? '(none)'}, not ${board.uid}`);
  if (boardKind(info.version) !== image.kind) problems.push(`${back.port} reports ${info.version ?? 'nothing'}, not ${LABEL[image.kind]}`);
  if (image.stamp && info.built && info.built !== image.stamp) {
    problems.push(`${back.port} runs the build from ${info.built}, not the image's ${image.stamp}`);
  }
  for (const other of others) {
    const current = now.find((b) => b.port && sameBoard(b, other));
    const after = current ? identify(current.port) : {};
    if (after.version !== other.version || after.built !== other.built || after.uid !== other.uid) {
      problems.push(`${other.port} changed: it was ${describeBoard(other)}, and is now ` +
        (current ? describeBoard({ ...current, ...after }) : 'not on serial'));
    }
  }
  if (problems.length) {
    fail(`CHECK FAILED after writing ${board.port ?? board.location ?? 'the board'}:\n  - ${problems.join('\n  - ')}\n` +
      'A board that took the wrong image still has its bootloader and can be put right: unplug every other\n' +
      'board, press its program button, and run this tool with the image built for it.');
  }
  console.log(`\nWritten. ${back.port} now runs ${info.version}${info.built ? `, built ${info.built}` : ''}` +
    `${info.uid ? `, chip ${info.uid}` : ''}.`);
  return { ...back, version: info.version, built: info.built, uid: info.uid, minimal: info.minimal, kind: boardKind(info.version) };
}

let opts;
try { opts = parseArgs(process.argv.slice(2)); } catch (error) { fail(error.message); }
if (opts.help) { console.log(HELP); process.exit(0); }

// Without --hex: the newest image of each type for --all, and otherwise the
// newest image of either type, whichever board it is for.
const images = opts.hex.length ? opts.hex.map(readImage)
  : opts.all ? newestImagePerKind()
  : builtHexes().slice(0, 1).map(readImage);
if (!images.length && !opts.check) fail('No hex found in build/firmware — build one first, or pass --hex <path>.');
for (const image of images) printImage(image);

const boards = identifyBoards(findBoards());
if (!boards.length) fail('No Teensy found on USB. Is the cartridge plugged in?');
console.log(`\nBoard${boards.length > 1 ? 's' : ''}:`);
for (const board of boards) console.log(`  ${describeBoard(board)}`);

if (!images.length) { console.log('\n--check: no image in build/firmware to check against; nothing written.'); process.exit(0); }

let plan;
try { plan = planFlash({ boards, images, all: opts.all, uid: opts.uid, port: opts.port }); }
catch (error) { if (error instanceof Refusal) fail(error.message); throw error; }
for (const note of plan.notes) console.log(`  ${note}`);
console.log('\nTo write:');
for (const { board, image } of plan.writes) {
  console.log(`  ${shown(image.path)}  ->  ${board.port ?? board.location ?? 'the board in the bootloader'}`);
}

if (opts.check) { console.log('\n--check: nothing written.'); process.exit(0); }

// What each board reports now; a written board's entry is replaced by what it
// reports after the write, so the next write compares against that.
const current = [...boards];
for (const { board, image } of plan.writes) {
  const others = current.filter((b) => !sameBoard(b, board));
  if (process.platform === 'win32') await writeWithTeensyLoader(image.path, board);
  else writeWithLoaderCli(image.path, board);
  const after = await checkAfterWrite(board, image, others);
  if (!after && plan.writes.length > 1) fail('Stopping before the next board, since this one could not be checked.');
  if (after) current[current.findIndex((b) => sameBoard(b, board))] = after;
}
closeTeensyLoader();
if (process.platform === 'win32') console.log('Closed the Teensy Loader, so it cannot write this image to a board later.');
console.log('TeensyROM settings are kept: a USB write leaves the flash that holds them alone.');
