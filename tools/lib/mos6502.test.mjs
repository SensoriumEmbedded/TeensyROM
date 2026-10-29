// SPDX-License-Identifier: MIT
import test from 'node:test';
import assert from 'node:assert/strict';
import { Mos6502, FLAG, OPCODES } from './mos6502.mjs';

const ORIGIN = 0x0200;
const STOP = 0xffff;

// Runs code at $0200 until it reaches STOP, the address a final JMP $FFFF lands on.
function machine(code, setup = () => {}) {
  const memory = new Uint8Array(0x10000);
  memory.set(code, ORIGIN);
  memory.set([0x4c, 0xff, 0xff], ORIGIN + code.length);
  const reads = [];
  const writes = [];
  const cpu = new Mos6502({
    read: (address) => {
      reads.push(address);
      return memory[address];
    },
    write: (address, value) => {
      writes.push([address, value]);
      memory[address] = value;
    },
  });
  cpu.pc = ORIGIN;
  setup(cpu, memory);
  cpu.run((c) => c.pc === STOP, 10_000);
  return { cpu, memory, reads, writes };
}

const flags = (cpu, ...names) => names.map((name) => Boolean(cpu.p & FLAG[name]));

test('the 151 documented opcodes decode and any other throws with its address', () => {
  assert.equal(OPCODES.size, 151);
  assert.throws(() => machine([0xea, 0x02]), /opcode \$02 at \$0201 is not a documented 6502 opcode/);
});

test('a copy loop through (zp),Y moves a block', () => {
  const { memory } = machine(
    [
      0xa9, 0x00, 0x85, 0x10, // LDA #$00 / STA $10
      0xa9, 0x40, 0x85, 0x11, // LDA #$40 / STA $11
      0xa0, 0x00, //             LDY #0
      0xb9, 0x00, 0x30, //       LDA $3000,Y
      0x91, 0x10, //             STA ($10),Y
      0xc8, //                   INY
      0xc0, 0x05, //             CPY #5
      0xd0, 0xf6, //             BNE -10
    ],
    (cpu, mem) => mem.set([1, 2, 3, 4, 5], 0x3000),
  );
  assert.deepEqual([...memory.subarray(0x4000, 0x4006)], [1, 2, 3, 4, 5, 0]);
});

test('binary ADC and SBC set carry and overflow', () => {
  const add = (a, b, carry) => machine([carry ? 0x38 : 0x18, 0xa9, a, 0x69, b]).cpu;
  const sub = (a, b, carry) => machine([carry ? 0x38 : 0x18, 0xa9, a, 0xe9, b]).cpu;

  let cpu = add(0x50, 0x50, 0);
  assert.equal(cpu.a, 0xa0);
  assert.deepEqual(flags(cpu, 'C', 'V', 'N', 'Z'), [false, true, true, false]);
  cpu = add(0xd0, 0x90, 0);
  assert.equal(cpu.a, 0x60);
  assert.deepEqual(flags(cpu, 'C', 'V'), [true, true]);
  cpu = add(0xff, 0x00, 1);
  assert.equal(cpu.a, 0x00);
  assert.deepEqual(flags(cpu, 'C', 'Z'), [true, true]);

  cpu = sub(0x50, 0xb0, 1);
  assert.equal(cpu.a, 0xa0);
  assert.deepEqual(flags(cpu, 'C', 'V', 'N'), [false, true, true]);
  cpu = sub(0x50, 0x30, 0);
  assert.equal(cpu.a, 0x1f);
  assert.deepEqual(flags(cpu, 'C', 'V'), [true, false]);
});

test('decimal ADC and SBC give BCD results and the NMOS flags', () => {
  const add = (a, b, carry) => machine([0xf8, carry ? 0x38 : 0x18, 0xa9, a, 0x69, b]).cpu;
  const sub = (a, b, carry) => machine([0xf8, carry ? 0x38 : 0x18, 0xa9, a, 0xe9, b]).cpu;

  let cpu = add(0x58, 0x46, 1);
  assert.equal(cpu.a, 0x05);
  assert.equal(flags(cpu, 'C')[0], true);
  cpu = add(0x12, 0x34, 0);
  assert.equal(cpu.a, 0x46);
  assert.equal(flags(cpu, 'C')[0], false);
  // the NMOS part takes Z from the binary sum ($9A) and N from the sum before its high digit is adjusted ($A0)
  cpu = add(0x99, 0x01, 0);
  assert.equal(cpu.a, 0x00);
  assert.deepEqual(flags(cpu, 'C', 'Z', 'N'), [true, false, true]);
  cpu = add(0x79, 0x01, 0);
  assert.equal(cpu.a, 0x80);
  assert.deepEqual(flags(cpu, 'C', 'N', 'V'), [false, true, true]);

  cpu = sub(0x46, 0x12, 1);
  assert.equal(cpu.a, 0x34);
  assert.equal(flags(cpu, 'C')[0], true);
  cpu = sub(0x40, 0x13, 1);
  assert.equal(cpu.a, 0x27);
  cpu = sub(0x21, 0x34, 1);
  assert.equal(cpu.a, 0x87);
  assert.equal(flags(cpu, 'C')[0], false);
  cpu = sub(0x32, 0x02, 0);
  assert.equal(cpu.a, 0x29);
});

test('shifts and rotates carry through C, in A and in memory', () => {
  let { cpu, memory } = machine(
    [0x38, 0xa9, 0x81, 0x6a, 0x26, 0x20], // SEC / LDA #$81 / ROR A / ROL $20
    (c, mem) => (mem[0x20] = 0x80),
  );
  assert.equal(cpu.a, 0xc0);
  assert.equal(memory[0x20], 0x01);
  assert.equal(flags(cpu, 'C')[0], true);

  ({ cpu, memory } = machine([0xa9, 0x81, 0x0a, 0x4e, 0x00, 0x30], (c, mem) => (mem[0x3000] = 0x01)));
  assert.equal(cpu.a, 0x02);
  assert.equal(memory[0x3000], 0x00);
  assert.deepEqual(flags(cpu, 'C', 'Z'), [true, true]);
});

test('JSR and RTS return past the call, and the stack wraps in page 1', () => {
  const { cpu, memory } = machine(
    [0x20, 0x08, 0x02, 0xa2, 0x07, 0x4c, 0x0b, 0x02, 0xa0, 0x09, 0x60], // JSR sub / LDX #7 / JMP end / sub: LDY #9 / RTS
    (c) => (c.s = 0x00),
  );
  assert.deepEqual([cpu.x, cpu.y, cpu.s], [7, 9, 0x00]);
  assert.deepEqual([memory[0x100], memory[0x1ff]], [0x02, 0x02], 'return address $0202 pushed high byte first');
});

test('BRK pushes B, and RTI returns two bytes past it with B cleared', () => {
  const { cpu, memory } = machine([0x00, 0xe8, 0xa9, 0x42], (c, mem) => {
    mem.set([0x00, 0x30], 0xfffe);
    mem.set([0xe8, 0x40], 0x3000); // INX / RTI
  });
  assert.deepEqual([cpu.a, cpu.x], [0x42, 1]);
  assert.equal(memory[0x1fb] & FLAG.B, FLAG.B);
  assert.equal(cpu.p & FLAG.B, 0);
});

test('an IRQ is held off while I is set and taken once it is clear', () => {
  const memory = new Uint8Array(0x10000);
  memory.set([0x00, 0x30], 0xfffe);
  const cpu = new Mos6502({ read: (a) => memory[a], write: (a, v) => (memory[a] = v) });
  cpu.pc = 0x0200;
  cpu.irq();
  assert.equal(cpu.pc, 0x0200);
  cpu.p &= ~FLAG.I;
  cpu.irq();
  assert.equal(cpu.pc, 0x3000);
  assert.equal(memory[0x1fb] & FLAG.B, 0);
});

test('an NMI is taken while I is set, through $FFFA, and sets I', () => {
  const memory = new Uint8Array(0x10000);
  memory.set([0x00, 0x40], 0xfffa);
  memory.set([0x00, 0x30], 0xfffe);
  const cpu = new Mos6502({ read: (a) => memory[a], write: (a, v) => (memory[a] = v) });
  cpu.pc = 0x0200;
  cpu.nmi();
  assert.equal(cpu.pc, 0x4000);
  cpu.p &= ~FLAG.I;
  cpu.irq();
  assert.equal(cpu.pc, 0x3000);
  assert.equal(cpu.p & FLAG.I, FLAG.I, 'the IRQ sets I');
});

test('reset loads the PC from $FFFC, sets I and puts S at $FD', () => {
  const memory = new Uint8Array(0x10000);
  memory.set([0x34, 0x12], 0xfffc);
  const cpu = new Mos6502({ read: (a) => memory[a], write: () => {} });
  cpu.p = FLAG.U;
  cpu.s = 0x12;
  cpu.reset();
  assert.deepEqual([cpu.pc, cpu.p & FLAG.I, cpu.s], [0x1234, FLAG.I, 0xfd]);
});

test('PHP pushes B and U, PLP keeps U set, TXS sets no flags', () => {
  const { cpu, memory } = machine([
    0x18, 0xb8, 0x08, //       CLC / CLV / PHP
    0xa9, 0x00, 0x48, 0x28, // LDA #0 / PHA / PLP
    0xa2, 0x80, 0xa9, 0x01, // LDX #$80 / LDA #1
    0x9a, //                   TXS
  ], (c) => (c.p |= FLAG.V));
  assert.equal(memory[0x1fd], FLAG.B | FLAG.U | FLAG.I);
  assert.equal(cpu.s, 0x80);
  assert.deepEqual(flags(cpu, 'U', 'V', 'Z', 'N'), [true, false, false, false]);
});

test('PLA sets Z from the pulled byte', () => {
  const { cpu } = machine([0xa9, 0x00, 0x48, 0xa9, 0x01, 0x68]); // LDA #0 / PHA / LDA #1 / PLA
  assert.deepEqual(flags(cpu, 'Z'), [true]);
});

test('CMP of equal values sets C and Z', () => {
  const { cpu } = machine([0xa9, 0x42, 0xc9, 0x42]);
  assert.deepEqual(flags(cpu, 'C', 'Z', 'N'), [true, true, false]);
});

test('absolute,Y wraps past $FFFF and zero-page,Y indexes by Y', () => {
  let { cpu } = machine([0xa0, 0x02, 0xb9, 0xff, 0xff], (c, mem) => (mem[0x0001] = 0x5a));
  assert.equal(cpu.a, 0x5a, 'LDA $FFFF,Y with Y=2 reads $0001');
  ({ cpu } = machine([0xa0, 0x03, 0xa2, 0x00, 0xb6, 0x10], (c, mem) => (mem[0x13] = 0x77)));
  assert.equal(cpu.x, 0x77, 'LDX $10,Y with Y=3 reads $13');
});

test('JMP indirect does not carry into the pointer high byte', () => {
  const { cpu } = machine([0x6c, 0xff, 0x30], (c, mem) => {
    mem[0x30ff] = 0x00;
    mem[0x3000] = 0x40;
    mem[0x3100] = 0x50;
    mem.set([0xa9, 0x01, 0x4c, 0xff, 0xff], 0x4000);
    mem.set([0xa9, 0x02, 0x4c, 0xff, 0xff], 0x5000);
  });
  assert.equal(cpu.a, 0x01);
});

test('zero-page indexing and pointers wrap within page 0', () => {
  let { cpu } = machine([0xa2, 0x02, 0xb5, 0xff], (c, mem) => (mem[0x01] = 0x11));
  assert.equal(cpu.a, 0x11, 'LDA $FF,X with X=2 reads $01');

  const pointerAcrossTheWrap = (c, mem) => {
    mem[0xff] = 0x00;
    mem[0x00] = 0x30;
    mem[0x100] = 0x40;
    mem[0x3000] = 0x22;
    mem[0x3001] = 0x33;
  };
  ({ cpu } = machine([0xa2, 0x01, 0xa1, 0xfe], pointerAcrossTheWrap));
  assert.equal(cpu.a, 0x22, 'LDA ($FE,X) with X=1 takes its pointer from $FF and $00');
  ({ cpu } = machine([0xa0, 0x01, 0xb1, 0xff], pointerAcrossTheWrap));
  assert.equal(cpu.a, 0x33, 'LDA ($FF),Y takes its pointer from $FF and $00');
});

test('a backward branch across a page boundary lands on the right address', () => {
  const { cpu } = machine([0x4c, 0x02, 0x31], (c, mem) => {
    mem.set([0xe8, 0x4c, 0xff, 0xff], 0x30f0); // INX / JMP end
    mem.set([0xa2, 0x00, 0xf0, 0xea], 0x3102); // LDX #0 / BEQ $30F0
  });
  assert.equal(cpu.x, 1);
});

test('BIT copies bits 7 and 6 and sets Z from the AND', () => {
  const { cpu } = machine([0xa9, 0x01, 0x2c, 0x00, 0x30], (c, mem) => (mem[0x3000] = 0xc0));
  assert.deepEqual(flags(cpu, 'N', 'V', 'Z'), [true, true, true]);
});

test('each operand is one bus access: a status register is read once per LDA', () => {
  const { reads, writes } = machine([0xad, 0x0d, 0xdf, 0x8d, 0x0e, 0xdf]);
  assert.equal(reads.filter((a) => a === 0xdf0d).length, 1);
  assert.equal(reads.filter((a) => a === 0xdf0e).length, 0, 'STA does not read its target');
  assert.deepEqual(writes, [[0xdf0e, 0]]);
});

test('a run that never finishes throws instead of hanging the test', () => {
  const memory = new Uint8Array(0x10000);
  memory.set([0x4c, 0x00, 0x02], 0x0200);
  const cpu = new Mos6502({ read: (a) => memory[a], write: () => {} });
  cpu.pc = 0x0200;
  assert.throws(() => cpu.run(() => false, 100), /still running after 100 steps, at \$0200/);
});

test('a run that finishes on its last allowed step returns', () => {
  const memory = new Uint8Array(0x10000).fill(0xea);
  const cpu = new Mos6502({ read: (a) => memory[a], write: () => {} });
  cpu.pc = 0x0200;
  cpu.run((c) => c.pc === 0x0203, 3);
  assert.equal(cpu.steps, 3);
});
