/*

These routines can be used to create and fire single-shot and continuous timers. The UpdateTimers
routine should be called 60 times a second.

Each timer is comprised of 8 bytes:

0   - enabled/disabled (1 = enabled)
1   - single-shot/continuous (0 = single-shot)
2,3 - the timer's current value
4,5 - the timer's frequency (60 = 1 second)
6,7 - the address of the routine to call when the timer fires

The 8 timers are stored in memory 'c64lib_timers'

*/

/*
    Clear memory used for timers
*/
// ClearTimers:
//   FillMI($00, c64lib_timers, TIMER_STRUCT_BYTES)
//   rts

/*

Create a timer that fires either on a schedule or as a single shot

r0 frequency in 60ths of a second (60 = wait 1 second)
r1 location to call when timer fires
r2L single shot or continuous (0 = single shot, 1 = continuous)
r2H timer number (0 - 7)
r3L enabled or disabled (0 = disabled, 1 = enabled)

*/
CreateTimer:
  lda r2H
  mult8
  tax
  stb r3L:c64lib_timers, x       // enabled
  stb r2L:c64lib_timers + $01, x // type
  stb r0L:c64lib_timers + $02, x // current value
  stb r0H:c64lib_timers + $03, x
  stb r0L:c64lib_timers + $04, x // frequency
  stb r0H:c64lib_timers + $05, x
  stb r1L:c64lib_timers + $06, x // address
  stb r1H:c64lib_timers + $07, x

  rts

/*

    Should be called every 1/60th of a second and will update timers and call timer targets if required.

*/
UpdateTimers:
  ldy #$00
!loop:
  tya
  mult8
  sta r0L
  tax
  // Is the timer enabled?
  jne c64lib_timers, x:#ENABLE:!continue+
  inx
  stb c64lib_timers, x:r3H
  inx
  stx r1H
  lda c64lib_timers, x // current value
  bne !return+
  dec c64lib_timers + $01, x
!return:
  dec c64lib_timers, x

  lda c64lib_timers, x
  bne !continue+
  lda c64lib_timers + $01, x
  bne !continue+

  // Counter has hit 0 - reset it
  inx
  inx
  stb c64lib_timers, x:r2L
  inx
  stb c64lib_timers, x:r2H

  lda r1H
  tax

  stb r2L:c64lib_timers, x
  inx
  stb r2H:c64lib_timers, x

  inx
  inx
  inx
  tya
  pha

  // Set up the vector to point at the timer's routine
  stb c64lib_timers, x:r2L
  inx
  stb c64lib_timers, x:r2H

  // Call the timer's routine
  jsr !dispatch+
  jmp !done_dispatch+

!dispatch:
  jmp (r2)

!done_dispatch:
  pla
  tay

  // If the timer is continuous, skip the disabling of it
  jeq r3H:#TIMER_CONTINUOUS:!continue+

  // For single shot timers, disable it after the first execution
  ldx r0L
  stb #DISABLE:c64lib_timers, x

!continue:
  iny
  cpy #$07
  bne !loop-

  rts

/*

Enable or disable a timer

r2H the timer to control
r3L enable or disable (0 = disable, 1 = enable)

*/
EnDisTimer:
  lda r2H
  mult8
  tax
  stb r3L:c64lib_timers, x
  rts
