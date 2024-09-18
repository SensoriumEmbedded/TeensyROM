// Pseudo 16-bit registers
// NOTE: These may or may not be completely safe for BASIC and the Kernal. Let me know if they cause problems.
// I tried to target casette related addresses, so if they're going to interfere, it's probably going to be
// with anything using cassette.

.label r0   = $fb
.label r0L  = $fb
.label r0H  = $fc
.label r1   = $fd
.label r1L  = $fd
.label r1H  = $fe
.label r2   = $9e
.label r2L  = $9e
.label r2H  = $9f
.label r3   = $9b
.label r3L  = $9b
.label r3H  = $9c
.label r4   = $be
.label r4L  = $be
.label r4H  = $bf
.label r5   = $b0
.label r5L  = $b0
.label r5H  = $b1
.label r6   = $b2
.label r6L  = $b2
.label r6H  = $b3
