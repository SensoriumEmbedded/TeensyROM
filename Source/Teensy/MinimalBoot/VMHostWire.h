// SPDX-License-Identifier: MIT
// The packet wire format, included inside namespace VmRuntime by both the
// firmware and the scheduler test so neither can drift from the other.
static uint16_t crc16(const uint8_t *p, unsigned n) {
    uint16_t c = 0xffff;
    while (n--) { c ^= (uint16_t)*p++ << 8; for (unsigned b = 0; b < 8; b++) c = (c << 1) ^ ((c & 0x8000) ? 0x1021 : 0); }
    return c;
}

// The host never inspects the payload.
static unsigned encodePacket(uint8_t *bytes) {
    bytes[0] = 'M'; bytes[1] = '3'; bytes[2] = 1; bytes[3] = packet.type; bytes[4] = sequence;
    bytes[5] = packet.flags; bytes[6] = packet.length; bytes[7] = 0;
    memcpy(bytes + 8, packet.payload, packet.length);
    const auto crc = crc16(bytes, 8 + packet.length);
    bytes[8 + packet.length] = crc; bytes[9 + packet.length] = crc >> 8;
    return 10u + packet.length;
}
