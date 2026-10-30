# GEOS 128 on the TeensyROM

## What it is
* Starts GEOS 128 from your own boot disk image on the SD card, with a USB mouse, and no disk drive.
  * GEOS itself is not included: you supply the disk image.
* Needs a C128. Runs on the TR and the TR+.
* Plug a USB mouse into the TeensyROM's USB host port.
  * It is not a 1351 mouse: only GEOS, with the TR mouse driver the TeensyROM adds, sees it.
* Find it in the main menu: `F1` (Teensy Memory), then **Utilities**, then **GEOS 128 +C128**.
  * The C128 restarts in 128 mode and GEOS 128 starts from your boot disk.
  * GEOS 128 starts in 40 or 80 columns according to the 40/80 DISPLAY key, as it does from a disk drive, and can switch from its own menu.
* For GEOS 64, see [GEOS 64 on the TeensyROM](GEOS64_Usage.md). The two work the same way, each from its own folder.

## SD card setup
* Make a folder named `/GEOS128` on the SD card.
* Copy your GEOS 128 boot disk image into it, under any file name ending in `.D81` (for example `GEOS128.D81`).
  * The TeensyROM finds the boot disk by what is on it, not by its name: the image must hold `128 DESKTOP` and the boot file `GEOBOOT128`.
  * D81 images only: the image with the deskTop and the applications on one disk.
  * Don't end the name in `-TR` (`GEOS-TR.D81`): that name is kept for the copy described below, and is skipped.
  * Versions that name their deskTop differently (Gateway, Wheels) are not found.
* If the folder holds more than one boot disk, the first one found is used. Only the first 8 `.D81` files in the folder are checked.

## The -TR copy
* GEOS saves to the disk it starts from, so the TeensyROM never starts from your original image.
* The first time you select GEOS 128, it makes a copy next to the original with `-TR` added to the name (`GEOS128.D81` becomes `GEOS128-TR.D81`) and adds the TR USB mouse driver to the copy. Your original is not changed.
* Later launches reuse the `-TR` copy, so your saved work is still there.
  * Each launch brings the copy's TR USB mouse driver up to date with the firmware's.
* To start over from your original, delete the `-TR` copy. The next launch makes a new one.

## RAM expansion: GeoRAM or REU
GEOS 128 runs beside one of two RAM expansions, chosen by the **Special IO** setting in the Settings menu (`F8`). Either one also carries the TeensyROM's USB mouse and disk, so no other Special IO is needed. See [Selecting and associating Special IO](General_Usage.md#selecting-and-associating-special-io).

| Special IO | RAM expansion | Boards |
|--|--|--|
| `REU+USB Mouse/Disk` | REU | TR+ only |
| anything else | GeoRAM | TR and TR+ |

### REU (recommended on a TR+)
* Set Special IO to `REU+USB Mouse/Disk`, then select GEOS 128.
* GEOS's own `128 CONFIGURE` runs at start-up, finds the REU (128K to 512K, as much as fits) and sets it up as GEOS does a Commodore REU of that size. GEOS knows only the REU as a RAM expansion, so this is the way to give it one.
* At launch, the TeensyROM loads the REU pre-load/save image into the REU. Choose that file with `<Shift-R>` in the file browser; the Settings menu shows the current one. If the file is not there, the REU starts empty.
* To save the REU to that file, press and release the Alt button (to the left of the menu button).

### GeoRAM
* With any other Special IO setting, GEOS 128 runs with GeoRAM (128K to 512K, as much as fits).
* At launch, the TeensyROM loads `SD:/georam.ram` into GeoRAM.
* **(TR+ only)** To save GeoRAM, press and release the Alt button.
  * The contents are written to `georam.ram` on the SD card, and the LED flashes for about 1.5 seconds once the save is done.
  * The previous `georam.ram` is kept as `georam.bak`.
  * If the power goes off during a save, the next launch loads the newest complete save: the one just made if it was fully written, otherwise the previous one.
* `128 CONFIGURE` runs at start-up as with the REU, but it does not find GeoRAM, so GEOS runs as it does with no RAM expansion. GeoRAM is there for programs that use it themselves.

### Either way
* `128 CONFIGURE` is set up in the `-TR` copy to find the TeensyROM as drive A.
* Other configuration programs on the disk are kept from running at start-up, since they look for a disk drive.

## Date and time
* GEOS starts with the date and time of the TeensyROM's real-time clock, adjusted by the Time Zone setting.
  * Set the clock and time zone in the Settings menu (`F8`, page 6). See [General Usage](General_Usage.md#the-settings-menu).
  * Without Ethernet, the clock can also be set over USB with the `SetRTCToken` command. See [Remote Command Protocol](ControlComms.md).

## Messages
If GEOS 128 can't start, the TeensyROM stays at the menu and shows one of these:

| Message | Meaning |
|--|--|
| GEOS 128 needs a C128 | The TeensyROM is in a C64. |
| GEOS 128 starts from your boot disk image on the SD card. Insert the card. | No SD card was found. |
| Copy your GEOS 128 boot disk image (.D81) to /GEOS128 on the SD card, then select this again. | There is no `/GEOS128` folder, or no D81 in it holds `128 DESKTOP`. |
| `<name>`: not a D64, D71 or D81 without error bytes | The file's size is not that of a disk image (a D81 is 819,200 bytes). |
| `<name>`: (other reason) | The image could not be read, or its directory is damaged. |
| Could not add the mouse driver to a copy of `<name>`: the disk is full | No room for the driver. Free some space on your original image, or use another one. "the directory is full" is the same for directory entries. |
| Could not add the mouse driver to a copy of `<name>`: the SD card could not be written | The copy could not be written to the SD card. |
| `<name>-TR.D81` has no desktop; remove it to start over | The `-TR` copy has lost `128 DESKTOP`. Delete it. |
| `<name>-TR.D81` was set aside and could not be put back | Power was lost while the copy was being updated, and the TeensyROM could not restore it. Look for `<name>-TR.D81.BAK` in `/GEOS128`. |
| `<name>-TR.D81`: its TR USB MOUSE is an old one; delete this copy | The copy's mouse driver is from a TeensyROM firmware whose driver cannot be updated in place. Delete the `-TR` copy; the next launch makes a new one. |
| `<name>-TR.D81`: the boot file is not on the disk | The image has no `GEOBOOT128`. |
| `<name>-TR.D81`: the boot file's disk driver is not one the TR can replace | This `GEOBOOT128` is not a version the TeensyROM can start. |
| `<name>-TR.D81`: the boot file's reset key is not where this version keeps it | This `GEOBOOT128` is not a version the TeensyROM can start. |

 <br>

[Back to main ReadMe](/README.md)
