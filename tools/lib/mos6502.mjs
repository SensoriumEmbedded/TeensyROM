// SPDX-License-Identifier: MIT
// An NMOS 6502 for host tests of C64 and C128 machine code. It runs the 151 documented
// opcodes and throws on any other. Each operand is read or written once: there are no dummy
// reads or writes, and no cycle counts, so a bus that has to see the real chip's extra
// accesses cannot be tested with it.

export const FLAG = { C: 0x01, Z: 0x02, I: 0x04, D: 0x08, B: 0x10, U: 0x20, V: 0x40, N: 0x80 };

const VECTOR = { NMI: 0xfffa, RESET: 0xfffc, IRQ: 0xfffe };
const STACK_PAGE = 0x100;

const OPCODE_TABLE = `
ADC 69 imm 65 zp 75 zpx 6d abs 7d absx 79 absy 61 indx 71 indy
AND 29 imm 25 zp 35 zpx 2d abs 3d absx 39 absy 21 indx 31 indy
ASL 0a acc 06 zp 16 zpx 0e abs 1e absx
BCC 90 rel
BCS b0 rel
BEQ f0 rel
BIT 24 zp 2c abs
BMI 30 rel
BNE d0 rel
BPL 10 rel
BRK 00 imp
BVC 50 rel
BVS 70 rel
CLC 18 imp
CLD d8 imp
CLI 58 imp
CLV b8 imp
CMP c9 imm c5 zp d5 zpx cd abs dd absx d9 absy c1 indx d1 indy
CPX e0 imm e4 zp ec abs
CPY c0 imm c4 zp cc abs
DEC c6 zp d6 zpx ce abs de absx
DEX ca imp
DEY 88 imp
EOR 49 imm 45 zp 55 zpx 4d abs 5d absx 59 absy 41 indx 51 indy
INC e6 zp f6 zpx ee abs fe absx
INX e8 imp
INY c8 imp
JMP 4c abs 6c ind
JSR 20 abs
LDA a9 imm a5 zp b5 zpx ad abs bd absx b9 absy a1 indx b1 indy
LDX a2 imm a6 zp b6 zpy ae abs be absy
LDY a0 imm a4 zp b4 zpx ac abs bc absx
LSR 4a acc 46 zp 56 zpx 4e abs 5e absx
NOP ea imp
ORA 09 imm 05 zp 15 zpx 0d abs 1d absx 19 absy 01 indx 11 indy
PHA 48 imp
PHP 08 imp
PLA 68 imp
PLP 28 imp
ROL 2a acc 26 zp 36 zpx 2e abs 3e absx
ROR 6a acc 66 zp 76 zpx 6e abs 7e absx
RTI 40 imp
RTS 60 imp
SBC e9 imm e5 zp f5 zpx ed abs fd absx f9 absy e1 indx f1 indy
SEC 38 imp
SED f8 imp
SEI 78 imp
STA 85 zp 95 zpx 8d abs 9d absx 99 absy 81 indx 91 indy
STX 86 zp 96 zpy 8e abs
STY 84 zp 94 zpx 8c abs
TAX aa imp
TAY a8 imp
TSX ba imp
TXA 8a imp
TXS 9a imp
TYA 98 imp
`;

function decodeTable() {
  const opcodes = new Map();
  for (const line of OPCODE_TABLE.trim().split('\n')) {
    const [mnemonic, ...pairs] = line.split(' ');
    for (let i = 0; i < pairs.length; i += 2) {
      opcodes.set(parseInt(pairs[i], 16), { mnemonic, mode: pairs[i + 1] });
    }
  }
  return opcodes;
}

export const OPCODES = decodeTable();

const hex = (value, digits) => '$' + value.toString(16).padStart(digits, '0');

export class Mos6502 {
  a = 0;
  x = 0;
  y = 0;
  s = 0xfd;
  p = FLAG.U | FLAG.I;
  pc = 0;
  steps = 0;

  // bus: { read(address) -> byte, write(address, byte) }
  constructor(bus) {
    this.bus = bus;
  }

  reset() {
    this.s = 0xfd;
    this.p |= FLAG.I;
    this.pc = this.read16(VECTOR.RESET);
  }

  irq() {
    if (!(this.p & FLAG.I)) this.interrupt(VECTOR.IRQ, 0);
  }

  nmi() {
    this.interrupt(VECTOR.NMI, 0);
  }

  // A run still going after limit steps throws, since in a test that is a hang.
  run(done, limit = 1_000_000) {
    for (let i = 0; i < limit; i++) {
      if (done(this)) return;
      this.step();
    }
    throw new Error(`still running after ${limit} steps, at ${hex(this.pc, 4)}`);
  }

  step() {
    const at = this.pc;
    const opcode = this.fetch();
    const decoded = OPCODES.get(opcode);
    if (!decoded) throw new Error(`opcode ${hex(opcode, 2)} at ${hex(at, 4)} is not a documented 6502 opcode`);
    this.steps++;
    this[decoded.mnemonic](this.operand(decoded.mode));
  }

  read(address) {
    return this.bus.read(address & 0xffff) & 0xff;
  }

  write(address, value) {
    this.bus.write(address & 0xffff, value & 0xff);
  }

  read16(address) {
    return this.read(address) | (this.read(address + 1) << 8);
  }

  readZeroPage16(address) {
    return this.read(address & 0xff) | (this.read((address + 1) & 0xff) << 8);
  }

  fetch() {
    const value = this.read(this.pc);
    this.pc = (this.pc + 1) & 0xffff;
    return value;
  }

  fetch16() {
    return this.fetch() | (this.fetch() << 8);
  }

  operand(mode) {
    switch (mode) {
      case 'imp':
      case 'acc':
        return { mode };
      case 'imm':
        return { mode, value: this.fetch() };
      case 'rel': {
        const offset = this.fetch();
        return { mode, address: (this.pc + (offset < 0x80 ? offset : offset - 0x100)) & 0xffff };
      }
      case 'zp':
        return { mode, address: this.fetch() };
      case 'zpx':
        return { mode, address: (this.fetch() + this.x) & 0xff };
      case 'zpy':
        return { mode, address: (this.fetch() + this.y) & 0xff };
      case 'abs':
        return { mode, address: this.fetch16() };
      case 'absx':
        return { mode, address: (this.fetch16() + this.x) & 0xffff };
      case 'absy':
        return { mode, address: (this.fetch16() + this.y) & 0xffff };
      case 'indx':
        return { mode, address: this.readZeroPage16(this.fetch() + this.x) };
      case 'indy':
        return { mode, address: (this.readZeroPage16(this.fetch()) + this.y) & 0xffff };
      case 'ind': {
        // the NMOS part does not carry into the high byte: JMP ($10FF) reads $10FF and $1000
        const pointer = this.fetch16();
        const high = (pointer & 0xff00) | ((pointer + 1) & 0xff);
        return { mode, address: this.read(pointer) | (this.read(high) << 8) };
      }
    }
  }

  load(operand) {
    if (operand.mode === 'imm') return operand.value;
    if (operand.mode === 'acc') return this.a;
    return this.read(operand.address);
  }

  store(operand, value) {
    if (operand.mode === 'acc') this.a = value;
    else this.write(operand.address, value);
  }

  setFlag(flag, on) {
    this.p = on ? this.p | flag : this.p & ~flag;
  }

  setNZ(value) {
    this.setFlag(FLAG.Z, (value & 0xff) === 0);
    this.setFlag(FLAG.N, value & 0x80);
    return value & 0xff;
  }

  push(value) {
    this.write(STACK_PAGE | this.s, value);
    this.s = (this.s - 1) & 0xff;
  }

  pull() {
    this.s = (this.s + 1) & 0xff;
    return this.read(STACK_PAGE | this.s);
  }

  push16(value) {
    this.push(value >> 8);
    this.push(value);
  }

  pull16() {
    return this.pull() | (this.pull() << 8);
  }

  interrupt(vector, breakFlag) {
    this.push16(this.pc);
    this.push(((this.p | FLAG.U) & ~FLAG.B) | breakFlag);
    this.p |= FLAG.I;
    this.pc = this.read16(vector);
  }

  branch(operand, taken) {
    if (taken) this.pc = operand.address;
  }

  compare(register, operand) {
    const value = this.load(operand);
    this.setFlag(FLAG.C, register >= value);
    this.setNZ(register - value);
  }

  modify(operand, change) {
    this.store(operand, this.setNZ(change(this.load(operand))));
  }

  // decimal mode follows https://www.6502.org/tutorials/decimal_mode.html appendix A, the NMOS flags
  ADC(operand) {
    const value = this.load(operand);
    const carry = this.p & FLAG.C;
    const binary = this.a + value + carry;
    this.setFlag(FLAG.Z, (binary & 0xff) === 0);
    if (!(this.p & FLAG.D)) {
      this.setFlag(FLAG.C, binary > 0xff);
      this.setFlag(FLAG.V, ~(this.a ^ value) & (this.a ^ binary) & 0x80);
      this.setFlag(FLAG.N, binary & 0x80);
      this.a = binary & 0xff;
      return;
    }
    let low = (this.a & 0x0f) + (value & 0x0f) + carry;
    if (low >= 0x0a) low = ((low + 0x06) & 0x0f) + 0x10;
    let sum = (this.a & 0xf0) + (value & 0xf0) + low;
    const signed = signedHigh(this.a) + signedHigh(value) + low;
    this.setFlag(FLAG.N, sum & 0x80);
    this.setFlag(FLAG.V, signed < -128 || signed > 127);
    if (sum >= 0xa0) sum += 0x60;
    this.setFlag(FLAG.C, sum >= 0x100);
    this.a = sum & 0xff;
  }

  SBC(operand) {
    const value = this.load(operand);
    const borrow = 1 - (this.p & FLAG.C);
    const binary = this.a - value - borrow;
    this.setFlag(FLAG.C, binary >= 0);
    this.setFlag(FLAG.V, (this.a ^ value) & (this.a ^ binary) & 0x80);
    this.setNZ(binary);
    if (!(this.p & FLAG.D)) {
      this.a = binary & 0xff;
      return;
    }
    let low = (this.a & 0x0f) - (value & 0x0f) - borrow;
    if (low < 0) low = ((low - 0x06) & 0x0f) - 0x10;
    let difference = (this.a & 0xf0) - (value & 0xf0) + low;
    if (difference < 0) difference -= 0x60;
    this.a = difference & 0xff;
  }

  AND(operand) { this.a = this.setNZ(this.a & this.load(operand)); }
  ORA(operand) { this.a = this.setNZ(this.a | this.load(operand)); }
  EOR(operand) { this.a = this.setNZ(this.a ^ this.load(operand)); }

  BIT(operand) {
    const value = this.load(operand);
    this.setFlag(FLAG.Z, (this.a & value) === 0);
    this.setFlag(FLAG.N, value & FLAG.N);
    this.setFlag(FLAG.V, value & FLAG.V);
  }

  ASL(operand) {
    this.modify(operand, (value) => {
      this.setFlag(FLAG.C, value & 0x80);
      return value << 1;
    });
  }

  LSR(operand) {
    this.modify(operand, (value) => {
      this.setFlag(FLAG.C, value & 0x01);
      return value >> 1;
    });
  }

  ROL(operand) {
    this.modify(operand, (value) => {
      const carryIn = this.p & FLAG.C;
      this.setFlag(FLAG.C, value & 0x80);
      return (value << 1) | carryIn;
    });
  }

  ROR(operand) {
    this.modify(operand, (value) => {
      const carryIn = this.p & FLAG.C;
      this.setFlag(FLAG.C, value & 0x01);
      return (value >> 1) | (carryIn << 7);
    });
  }

  INC(operand) { this.modify(operand, (value) => value + 1); }
  DEC(operand) { this.modify(operand, (value) => value - 1); }
  INX() { this.x = this.setNZ(this.x + 1); }
  INY() { this.y = this.setNZ(this.y + 1); }
  DEX() { this.x = this.setNZ(this.x - 1); }
  DEY() { this.y = this.setNZ(this.y - 1); }

  CMP(operand) { this.compare(this.a, operand); }
  CPX(operand) { this.compare(this.x, operand); }
  CPY(operand) { this.compare(this.y, operand); }

  BCC(operand) { this.branch(operand, !(this.p & FLAG.C)); }
  BCS(operand) { this.branch(operand, this.p & FLAG.C); }
  BNE(operand) { this.branch(operand, !(this.p & FLAG.Z)); }
  BEQ(operand) { this.branch(operand, this.p & FLAG.Z); }
  BPL(operand) { this.branch(operand, !(this.p & FLAG.N)); }
  BMI(operand) { this.branch(operand, this.p & FLAG.N); }
  BVC(operand) { this.branch(operand, !(this.p & FLAG.V)); }
  BVS(operand) { this.branch(operand, this.p & FLAG.V); }

  JMP(operand) { this.pc = operand.address; }

  JSR(operand) {
    this.push16((this.pc - 1) & 0xffff);
    this.pc = operand.address;
  }

  RTS() { this.pc = (this.pull16() + 1) & 0xffff; }

  BRK() {
    this.pc = (this.pc + 1) & 0xffff;
    this.interrupt(VECTOR.IRQ, FLAG.B);
  }

  RTI() {
    this.PLP();
    this.pc = this.pull16();
  }

  LDA(operand) { this.a = this.setNZ(this.load(operand)); }
  LDX(operand) { this.x = this.setNZ(this.load(operand)); }
  LDY(operand) { this.y = this.setNZ(this.load(operand)); }
  STA(operand) { this.store(operand, this.a); }
  STX(operand) { this.store(operand, this.x); }
  STY(operand) { this.store(operand, this.y); }

  TAX() { this.x = this.setNZ(this.a); }
  TAY() { this.y = this.setNZ(this.a); }
  TXA() { this.a = this.setNZ(this.x); }
  TYA() { this.a = this.setNZ(this.y); }
  TSX() { this.x = this.setNZ(this.s); }
  TXS() { this.s = this.x; }

  PHA() { this.push(this.a); }
  PHP() { this.push(this.p | FLAG.B | FLAG.U); }
  PLA() { this.a = this.setNZ(this.pull()); }
  PLP() { this.p = (this.pull() & ~FLAG.B) | FLAG.U; }

  CLC() { this.p &= ~FLAG.C; }
  SEC() { this.p |= FLAG.C; }
  CLI() { this.p &= ~FLAG.I; }
  SEI() { this.p |= FLAG.I; }
  CLD() { this.p &= ~FLAG.D; }
  SED() { this.p |= FLAG.D; }
  CLV() { this.p &= ~FLAG.V; }
  NOP() {}
}

function signedHigh(byte) {
  return (byte & 0xf0) - ((byte & 0x80) << 1);
}
