// SPDX-License-Identifier: MIT
//
// Deciding what flash-firmware.mjs writes to which board, kept apart from the
// USB and serial work so it can be tested with no board attached. Nothing here
// touches hardware: the boards come in already identified, the images already
// read, and what comes out is a plan or a refusal.
import { decodeHex, MAIN_BASE, VM_BASE } from './hex.mjs';

// The strings FlashUpdate.ino compiles in, and which cartridge each belongs to.
export const TARGET_IDS = [
  ['fw_t41_teensyromplus_sensorium', 'tr-plus'],
  ['fw_t41_teensyrom_sensorium_v3', 'tr'],
  ['fw_t41_teensyrom_sensorium', 'tr'],      // fab 0.2x, checked last: it is a prefix of v3
];
export const LABEL = { 'tr': 'TeensyROM', 'tr-plus': 'TeensyROM+' };

// A refusal is an expected outcome (wrong target, no way to tell boards apart),
// reported as a message and an exit code rather than a stack trace.
export class Refusal extends Error {}

const DATE = /[A-Z][a-z]{2} [ \d]\d \d{4}/g;   // __DATE__, e.g. "Aug  5 2026"
const TIME = /\d\d:\d\d:\d\d/g;                 // __TIME__

// Reads a hex's text: which cartridge it is built for, the flash regions it
// covers (a three-image build has a gap, which is expected, and is why this
// reports regions rather than one span), and the build stamp the running
// firmware will print in its version reply. The stamp is taken from the main
// image alone: MinimalBoot carries its own, which differs from the main one's
// in a build whose date is not pinned.
export function inspectImage(hexText) {
  const bytes = decodeHex(hexText);
  const addresses = [...bytes.keys()].sort((a, b) => a - b);
  const text = (from) => Buffer.from(from.map((a) => bytes.get(a))).toString('latin1');
  const target = TARGET_IDS.find(([id]) => text(addresses).includes(id));

  const regions = [];
  let start = addresses[0], previous = addresses[0];
  for (const address of addresses.slice(1)) {
    if (address !== previous + 1) { regions.push([start, previous + 1]); start = address; }
    previous = address;
  }
  regions.push([start, previous + 1]);

  const main = text(addresses.filter((a) => a >= MAIN_BASE && a < VM_BASE));
  const dates = new Set(main.match(DATE)), times = new Set(main.match(TIME));
  const stamp = dates.size === 1 && times.size === 1 ? `${[...dates][0]}, ${[...times][0]}` : null;
  return { kind: target?.[1] ?? null, id: target?.[0] ?? null, regions, bytes: addresses.length, stamp };
}

// One line per board, the same wherever a board is named.
export function describeBoard(board) {
  if (board.bootloader) return `${board.location ?? 'a board'}  in the bootloader (cannot be identified until it runs again)`;
  const running = board.version
    ? `${board.version}${board.minimal ? ' (MinimalBoot)' : ''}${board.built ? `, built ${board.built}` : ''}`
    : 'did not answer';
  return `${board.port}  ${running}${board.uid ? `  chip ${board.uid}` : ''}`;
}

// The same physical board, before and after a reboot. The USB location stays
// put while the COM port can change (the full firmware and MinimalBoot
// enumerate differently), so the location decides when both sides have one.
export function sameBoard(a, b) {
  if (a.location && b.location) return a.location === b.location;
  return Boolean(a.port && b.port) && a.port.toLowerCase() === b.port.toLowerCase();
}

// Boards: [{ port, bootloader, location, label, version, built, uid, kind }],
// images: [{ path, kind, id, stamp }]. Returns { writes: [{ board, image }],
// notes: [line] } in the order to write, or throws a Refusal.
export function planFlash({ boards, images, all = false, uid = null, port = null }) {
  if (!boards.length) throw new Refusal('No Teensy found on USB. Is the cartridge plugged in?');
  if (uid && port) throw new Refusal('Pass --uid or --port, not both.');
  if (all) return planAll(boards, images, { uid, port });
  if (images.length !== 1) throw new Refusal('More than one --hex needs --all; without it, one image goes to one board.');
  const [image] = images;
  const board = pickBoard(boards, image, { uid, port });
  return { writes: [{ board, image }], notes: checkTarget(board, image) };
}

// A board already in the bootloader answers nothing, and the Teensy Loader
// writes to whichever board it finds there. Alone, that board is the target and
// is written on the image's word; beside another board it could take an image
// meant for that other board.
function refuseWaitingBoard(boards) {
  const waiting = boards.find((b) => b.bootloader);
  if (boards.length > 1 && waiting) {
    throw new Refusal(`A board is waiting in the bootloader (${waiting.location ?? 'location unknown'}), and the Teensy Loader ` +
      'writes to whichever board it finds there. Unplug the other boards and flash that one on its own, ' +
      'or power-cycle it so it runs its firmware again.');
  }
}

function pickBoard(boards, image, { uid, port }) {
  refuseWaitingBoard(boards);
  if (uid) {
    const found = boards.find((b) => b.uid === String(uid));
    if (!found) throw new Refusal(`No attached board has chip ID ${uid}.`);
    return found;
  }
  if (port) {
    const found = boards.find((b) => b.port && b.port.toLowerCase() === String(port).toLowerCase());
    if (!found) throw new Refusal(`No attached board is on ${port}.`);
    return found;
  }
  if (boards.length === 1) return boards[0];

  // Several boards: the image's type has to single one out, and a board that
  // did not answer could be of either type.
  const silent = boards.find((b) => !b.kind);
  if (silent) {
    throw new Refusal(`${silent.port} did not answer, so it cannot be ruled out as a ${LABEL[image.kind]}. ` +
      'Name the board to write with --uid or --port.');
  }
  const matches = boards.filter((b) => b.kind === image.kind);
  if (matches.length === 1) return matches[0];
  if (!matches.length) throw new Refusal(`None of the attached boards is a ${LABEL[image.kind]}, which that image is built for.`);
  throw new Refusal(`${matches.length} attached boards are ${LABEL[image.kind]}s. Name the one to write with --uid <chip ID>.`);
}

// The check this tool exists for: a TR and a TR+ image are both valid, both
// build cleanly, and are not interchangeable.
function checkTarget(board, image) {
  if (board.bootloader) return ['the board is in the bootloader, so the image\'s own target is all there is to go on'];
  if (!board.kind) return ['WARNING: no version reply came back, so the target could not be confirmed.'];
  if (board.kind !== image.kind) {
    throw new Refusal(`Refusing to write: ${board.port} is a ${LABEL[board.kind]} and that image is built for ${LABEL[image.kind]}.\n` +
      `Rebuild with --target ${board.kind}, or pass the right --hex.`);
  }
  return [`target matches the image (${LABEL[board.kind]})`];
}

function planAll(boards, images, { uid, port }) {
  if (uid || port) throw new Refusal('--all writes every board it can match; it takes no --uid or --port.');
  const kinds = images.map((image) => image.kind);
  const twice = kinds.find((kind, i) => kinds.indexOf(kind) !== i);
  if (twice) throw new Refusal(`Two images are built for ${LABEL[twice]}; --all takes at most one per cartridge type.`);
  const waiting = boards.find((b) => b.bootloader);
  if (waiting) {
    throw new Refusal(`--all needs every board running so it can tell what each one is, and one is waiting in the ` +
      `bootloader (${waiting.location ?? 'location unknown'}). Flash that board on its own first, or power-cycle it.`);
  }

  const writes = [], notes = [];
  for (const board of boards) {
    const image = images.find((i) => i.kind === board.kind);
    if (!board.kind) notes.push(`${board.port}: skipped, it did not answer.`);
    else if (!image) notes.push(`${board.port}: skipped, no ${LABEL[board.kind]} image.`);
    else writes.push({ board, image });
  }
  if (!writes.length) throw new Refusal('Nothing to write: none of the attached boards matches an image.');
  const stamps = new Set(images.map((image) => image.stamp));
  if (stamps.size > 1) {
    notes.push(`WARNING: the images carry different build dates (${[...stamps].join(' / ')}), so they come from different commits.`);
  }
  return { writes, notes };
}
