# TeensyROM USB mouse registers

A USB mouse plugged into the TeensyROM's USB host port is read by the C64 or C128 through six registers. It is not a 1351: the SID's POT lines are not on the cartridge port, so a program sees the mouse only through a driver that reads these registers. Two drivers do, and either is a working example: Minesweeper's `FindMouse` and `ReadMouse` ([`Minesweeper.asm`](../Source/C64/Minesweeper/source/Minesweeper.asm)) and the GEOS input driver ([`trmouse128.asm`](../tools/geos/trmouse128.asm)).

The firmware side is [`USBMouse.c`](../Source/Teensy/MinimalBoot/Common/IO_Handlers/USBMouse.c).

## Where the registers are
Three Special IO settings carry them, at base `B`:

| Special IO | `B` |
|:--|:--|
| USB Mouse | `$DE00` |
| GeoRAM | `$DF00`, beside the [block device](TR_Block_Device.md) |
| REU+USB Mouse/Disk **(TR+ only)** | `$DE00`, beside the block device |

The TR menu's Minesweeper starts USB Mouse, and its GEOS launches start GeoRAM or REU+USB Mouse/Disk. No other Special IO or cartridge type carries the registers.

## Finding them
1. If `TRB` (`$54 $52 $42`) is at `$DF10`-`$DF12`, `B` is `$DF00`. Otherwise `B` is `$DE00`.
2. If `TRM` (`$54 $52 $4D`) is at `B+3`-`B+5`, the registers are there. Otherwise there is no TeensyROM mouse.

Step 1 uses the block device's ID because the mouse's own can't be trusted at either address alone: beside the REU, `$DF03`-`$DF05` are its address registers, which hold whatever was last written to them, and beside GeoRAM, `$DE03`-`$DE05` are its window. `TRM` answers whether or not a mouse is plugged in; with none, the registers report no movement and no buttons.

## Registers

| Offset | Name | Reads |
|:--|:--|:--|
| `+0` | X | movement since the last read of `+0`; right is positive |
| `+1` | Y | movement since the last read of `+1`; down is positive |
| `+2` | Buttons | 1 = pressed: bit 0 left, bit 1 right, bit 2 middle; bits 3-7 are any further buttons the mouse has |
| `+3`-`+5` | ID | `TRM` |

Writes to them do nothing. Nothing answers at `+6` and `+7`. Beside GeoRAM and the REU, the block device's registers start at `+8`.

## Movement
* X and Y are two's-complement bytes, at most 127 either way. Movement adds up between reads, so a larger one comes over several reads.
* One step is four of the mouse's USB counts, so the speed depends on the mouse's resolution.
* Movement made before the Special IO started, at the TR menu for instance, is dropped.
* The scroll wheel is not reported.
