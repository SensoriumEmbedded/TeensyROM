// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import { encodeHex, FLASH_BASE, MAIN_BASE } from './hex.mjs';
import { inspectImage, planFlash, sameBoard, findWritten, answerComplete, answerProblems, describeBoard, Refusal } from './flash-plan.mjs';

// Boards as flash-firmware.mjs hands them over, already identified.
const tr = (port, uid, extra = {}) => ({ port, bootloader: false, location: `usb:${port}`, label: `${port} (Teensy 4.1) Serial+MIDI`,
  version: 'TeensyROM v0.8.0.11', built: 'Sep 25 2026, 03:03:51', uid, kind: 'tr', ...extra });
const trPlus = (port, uid, extra = {}) => tr(port, uid, { version: 'TeensyROM+ v0.8.0.11', kind: 'tr-plus', ...extra });
const silent = (port) => tr(port, null, { version: null, built: null, kind: null });
const waiting = (location) => ({ port: null, bootloader: true, location, label: '(Teensy 4.1) Bootloader',
  version: null, built: null, uid: null, kind: null });
const trImage = { path: 'TeensyROM.hex', kind: 'tr', stamp: 'Sep 25 2026, 03:03:51', reportsUid: true };
const plusImage = { path: 'TeensyROM+.hex', kind: 'tr-plus', stamp: 'Sep 25 2026, 03:03:51', reportsUid: true };

const targets = (plan) => plan.writes.map(({ board, image }) => `${board.port ?? board.location}<-${image.kind}`);
const refuses = (options, pattern) => assert.throws(() => planFlash(options), (error) => error instanceof Refusal && pattern.test(error.message));

test('one board of the image\'s type is written, as before', () => {
  const plan = planFlash({ boards: [tr('COM12', '14470230')], images: [trImage] });
  assert.deepEqual(targets(plan), ['COM12<-tr']);
  assert.match(plan.notes.join(), /target matches/);
});

test('one board of the other type is refused, as before', () => {
  refuses({ boards: [tr('COM12', '14470230')], images: [plusImage] }, /COM12 is a TeensyROM and that image is built for TeensyROM\+/);
});

test('one board in the bootloader, or one that does not answer, is written with a note, as before', () => {
  const halted = planFlash({ boards: [waiting('usb:1')], images: [trImage] });
  assert.deepEqual(targets(halted), ['usb:1<-tr']);
  assert.match(halted.notes.join(), /in the bootloader, so the image's own target is all there is to go on/);
  const plan = planFlash({ boards: [silent('COM12')], images: [trImage] });
  assert.deepEqual(targets(plan), ['COM12<-tr']);
  assert.match(plan.notes.join(), /could not be confirmed/);
});

test('a TeensyROM beside a TeensyROM+: the image\'s type picks the board, whichever is listed first', () => {
  const boards = [trPlus('COM4', '19277260'), tr('COM12', '14470230')];
  assert.deepEqual(targets(planFlash({ boards, images: [trImage] })), ['COM12<-tr']);
  assert.deepEqual(targets(planFlash({ boards, images: [plusImage] })), ['COM4<-tr-plus']);
});

test('two boards of the image\'s type are refused until --uid names one', () => {
  const boards = [trPlus('COM4', '19277260'), trPlus('COM5', '11111111')];
  refuses({ boards, images: [plusImage] }, /2 attached boards are TeensyROM\+s.*--uid/);
  assert.deepEqual(targets(planFlash({ boards, images: [plusImage], uid: '11111111' })), ['COM5<-tr-plus']);
  assert.deepEqual(targets(planFlash({ boards, images: [plusImage], port: 'com4' })), ['COM4<-tr-plus']);
});

test('--uid and --port must name an attached board, and still get the type check', () => {
  const boards = [trPlus('COM4', '19277260'), tr('COM12', '14470230')];
  refuses({ boards, images: [trImage], uid: '999' }, /No attached board has chip ID 999/);
  refuses({ boards, images: [trImage], port: 'COM9' }, /No attached board is on COM9/);
  refuses({ boards, images: [trImage], uid: '19277260' }, /COM4 is a TeensyROM\+ and that image is built for TeensyROM\b/);
  refuses({ boards, images: [trImage], port: 'COM4' }, /COM4 is a TeensyROM\+ and that image is built for TeensyROM\b/);
  refuses({ boards, images: [trImage], uid: '19277260', port: 'COM4' }, /--uid or --port, not both/);
});

test('with several boards, no match, or a board that did not answer, is refused rather than guessed', () => {
  refuses({ boards: [tr('COM12', '1'), tr('COM13', '2')], images: [plusImage] }, /None of the attached boards is a TeensyROM\+/);
  refuses({ boards: [trPlus('COM4', '1'), silent('COM12')], images: [plusImage] }, /COM12 did not answer.*--uid or --port/);
  // Naming a board that did answer is enough.
  assert.deepEqual(targets(planFlash({ boards: [trPlus('COM4', '1'), silent('COM12')], images: [plusImage], uid: '1' })),
    ['COM4<-tr-plus']);
});

test('a board waiting in the bootloader beside another board is refused, however the target is named', () => {
  const boards = [trPlus('COM4', '19277260'), waiting('usb:2')];
  refuses({ boards, images: [plusImage] }, /waiting in the bootloader/);
  refuses({ boards, images: [plusImage], uid: '19277260' }, /waiting in the bootloader/);
  refuses({ boards, images: [plusImage], port: 'COM4' }, /waiting in the bootloader/);
  refuses({ boards, images: [plusImage, trImage], all: true }, /--all needs every board running/);
});

test('more than one image needs --all', () => {
  refuses({ boards: [tr('COM12', '1')], images: [trImage, plusImage] }, /More than one --hex needs --all/);
});

test('--all gives each board the image built for its type, in listing order', () => {
  const boards = [trPlus('COM4', '19277260'), tr('COM12', '14470230')];
  const plan = planFlash({ boards, images: [trImage, plusImage], all: true });
  assert.deepEqual(targets(plan), ['COM4<-tr-plus', 'COM12<-tr']);
  assert.deepEqual(plan.notes, []);
});

test('--all skips boards it has no image for, or that did not answer, and says so', () => {
  const plan = planFlash({ boards: [trPlus('COM4', '1'), tr('COM12', '2'), silent('COM13')], images: [plusImage], all: true });
  assert.deepEqual(targets(plan), ['COM4<-tr-plus']);
  assert.match(plan.notes.join('\n'), /COM12: skipped, no TeensyROM image/);
  assert.match(plan.notes.join('\n'), /COM13: skipped, it did not answer/);
});

test('--all writes two boards of one type with the same image', () => {
  assert.deepEqual(targets(planFlash({ boards: [trPlus('COM4', '1'), trPlus('COM5', '2')], images: [plusImage], all: true })),
    ['COM4<-tr-plus', 'COM5<-tr-plus']);
});

test('--all refuses two images of one type, --uid or --port, and a plan with nothing in it', () => {
  const boards = [trPlus('COM4', '1'), tr('COM12', '2')];
  refuses({ boards, images: [trImage, { ...trImage, path: 'other.hex' }], all: true }, /Two images are built for TeensyROM;/);
  refuses({ boards, images: [trImage], all: true, uid: '1' }, /takes no --uid or --port/);
  refuses({ boards: [tr('COM12', '2')], images: [plusImage], all: true }, /Nothing to write/);
});

test('--all warns when the images it writes come from different commits', () => {
  const older = { ...plusImage, stamp: 'Sep 23 2026, 22:45:10' };
  const plan = planFlash({ boards: [trPlus('COM4', '1'), tr('COM12', '2')], images: [trImage, older], all: true });
  assert.match(plan.notes.join(), /different build dates \(Sep 23 2026, 22:45:10 \/ Sep 25 2026, 03:03:51\)/);
  // Not for an image no attached board gets, nor for one whose date could not be read.
  assert.doesNotMatch(planFlash({ boards: [tr('COM12', '2')], images: [trImage, older], all: true }).notes.join(), /WARNING/);
  assert.doesNotMatch(planFlash({ boards: [trPlus('COM4', '1'), tr('COM12', '2')], images: [trImage, { ...plusImage, stamp: null }], all: true })
    .notes.join(), /WARNING/);
});

test('a board answering from MinimalBoot is marked as such wherever it is named', () => {
  assert.match(describeBoard(trPlus('COM7', '1', { minimal: true, built: 'Aug  5 2026, 10:41:55' })),
    /^COM7  TeensyROM\+ v0\.8\.0\.11 \(MinimalBoot\), built Aug  5 2026, 10:41:55  chip 1$/);
});

test('no board at all is refused', () => {
  refuses({ boards: [], images: [trImage] }, /No Teensy found/);
});

test('the same board is recognised across a reboot by its USB location, even on a new COM port', () => {
  assert.ok(sameBoard({ port: 'COM7', location: 'usb:1' }, { port: 'COM4', location: 'usb:1' }));
  assert.ok(!sameBoard({ port: 'COM4', location: 'usb:1' }, { port: 'COM4', location: 'usb:2' }));
  assert.ok(sameBoard({ port: '/dev/ttyACM0', location: null }, { port: '/dev/ttyACM0', location: null }));
  assert.ok(!sameBoard({ port: null, location: null }, { port: null, location: null }));
});

test('after a write the board is found by USB location, or on the /dev fallback as the one device listed', () => {
  const written = { port: 'COM12', location: 'usb:1' };
  assert.equal(findWritten([{ port: 'COM4', location: 'usb:2' }, { port: 'COM13', location: 'usb:1' }], written)?.port, 'COM13');
  assert.equal(findWritten([{ port: 'COM4', location: 'usb:2' }], written), undefined);
  // The fallback lists one device with no location, and Linux may renumber it.
  const acm = { port: '/dev/ttyACM0', location: null };
  assert.equal(findWritten([{ port: '/dev/ttyACM1', location: null }], acm)?.port, '/dev/ttyACM1');
  assert.equal(findWritten([], acm), undefined);
  assert.equal(findWritten([{ port: '/dev/ttyACM1', location: null }, { port: '/dev/ttyACM2', location: null }], acm), undefined);
  // A board known by location is never swapped for another that happens to be alone.
  assert.equal(findWritten([{ port: 'COM4', location: 'usb:2' }], { port: null, bootloader: true, location: 'usb:1' }), undefined);
});

// The written board's answer, as flash-firmware.mjs passes it: a version reply plus port and kind.
const answer = (extra = {}) => ({ port: 'COM12', version: 'TeensyROM v0.8.0.11', built: 'Sep 25 2026, 03:03:51',
  uid: '14470230', minimal: false, kind: 'tr', ...extra });

test('an answer is complete once it carries the date and chip ID the image prints', () => {
  assert.ok(answerComplete(trImage, answer()));
  assert.ok(!answerComplete(trImage, answer({ built: null })));
  assert.ok(!answerComplete(trImage, answer({ uid: null })));
  assert.ok(!answerComplete(trImage, answer({ minimal: true })));
  assert.ok(!answerComplete(trImage, answer({ version: null })));
  // An image with no stamp or no chip ID in its reply asks for neither.
  assert.ok(answerComplete({ kind: 'tr', stamp: null, reportsUid: false }, answer({ built: null, uid: null })));
});

test('the written board passes when it answers as itself, running the image', () => {
  assert.deepEqual(answerProblems(tr('COM12', '14470230'), trImage, answer()), []);
});

test('a chip ID is required when the image prints one, and must match the one reported before', () => {
  assert.match(answerProblems(tr('COM12', '14470230'), trImage, answer({ uid: '19277260' })).join(), /answers as chip 19277260, not 14470230/);
  assert.match(answerProblems(tr('COM12', '14470230'), trImage, answer({ uid: null })).join(), /no chip ID, and the image reports one/);
  // Nothing to compare with before the write: waiting in the bootloader, silent, or firmware too old to print one.
  assert.deepEqual(answerProblems(waiting('usb:1'), trImage, answer()), []);
  assert.deepEqual(answerProblems(tr('COM12', null), trImage, answer()), []);
  assert.match(answerProblems(waiting('usb:1'), trImage, answer({ uid: null })).join(), /no chip ID/);
  // Written with an image too old to print one: none is expected, even from a board that had one.
  assert.deepEqual(answerProblems(tr('COM12', '14470230'), { ...trImage, reportsUid: false }, answer({ uid: null })), []);
});

test('the build date must be the image\'s, and a reply without one fails when the image has one', () => {
  assert.match(answerProblems(tr('COM12', '14470230'), trImage, answer({ built: 'Sep 23 2026, 22:45:10' })).join(),
    /runs the build from Sep 23 2026, 22:45:10, not the image's Sep 25 2026, 03:03:51/);
  assert.match(answerProblems(tr('COM12', '14470230'), trImage, answer({ built: null })).join(), /\(no date in its reply\)/);
  assert.deepEqual(answerProblems(tr('COM12', '14470230'), { ...trImage, stamp: null }, answer({ built: null })), []);
});

test('the wrong type, or a board left in MinimalBoot, fails the check', () => {
  assert.match(answerProblems(tr('COM12', '14470230'), trImage, answer({ version: 'TeensyROM+ v0.8.0.11', kind: 'tr-plus' })).join(),
    /reports TeensyROM\+ v0\.8\.0\.11, not TeensyROM/);
  assert.match(answerProblems(tr('COM12', '14470230'), trImage, answer({ minimal: true })).join(), /still in MinimalBoot/);
});

// A hex with the given strings placed at the given addresses.
function hexWith(strings) {
  const bytes = new Map();
  for (const [address, text] of strings) [...Buffer.from(text, 'latin1')].forEach((byte, i) => bytes.set(address + i, byte));
  return encodeHex(bytes);
}

test('an image reports its cartridge, and the build stamp from the main firmware, not from MinimalBoot', () => {
  const image = inspectImage(hexWith([
    [FLASH_BASE, 'fw_t41_teensyromplus_sensorium Aug  5 2026 10:41:55'],
    [MAIN_BASE, 'fw_t41_teensyromplus_sensorium'],
    [MAIN_BASE + 0x100, 'Aug  5 2026\0' + '10:41:20\0'],
  ]));
  assert.equal(image.kind, 'tr-plus');
  assert.equal(image.stamp, 'Aug  5 2026, 10:41:20');
  assert.equal(image.regions.length, 3);
});

test('a plain TeensyROM image is told apart from the v3 and the TR+ ids', () => {
  assert.equal(inspectImage(hexWith([[MAIN_BASE, 'fw_t41_teensyrom_sensorium_v3']])).kind, 'tr');
  assert.equal(inspectImage(hexWith([[MAIN_BASE, 'fw_t41_teensyrom_sensorium\0']])).kind, 'tr');
  assert.equal(inspectImage(hexWith([[MAIN_BASE, 'no target here']])).kind, null);
});

test('an image says whether its version reply carries a chip ID, from the main firmware alone', () => {
  assert.equal(inspectImage(hexWith([[MAIN_BASE, 'Teensy: %luMHz  %.1fC  UID: %lu\r']])).reportsUid, true);
  assert.equal(inspectImage(hexWith([[FLASH_BASE, 'UID: %lu'], [MAIN_BASE, '  FW: %s\r\n']])).reportsUid, false);
});

test('no stamp is claimed when the main image has none, or more than one', () => {
  assert.equal(inspectImage(hexWith([[MAIN_BASE, 'fw_t41_teensyrom_sensorium_v3']])).stamp, null);
  assert.equal(inspectImage(hexWith([[MAIN_BASE, 'Sep 25 2026 03:03:51 Sep 23 2026 22:45:10']])).stamp, null);
});
