# GEOS 64 on the TeensyROM

## What it is
* Starts GEOS 64 from your own boot disk image on the SD card, with a USB mouse, and no disk drive.
  * GEOS itself is not included: you supply the disk image.
* Runs on the TR and the TR+.
* Plug a USB mouse into the TeensyROM's USB host port.
  * It is not a 1351 mouse: only GEOS, with the TR mouse driver the TeensyROM adds, sees it.
* Find it in the main menu: `F1` (Teensy Memory), then **Utilities**, then **GEOS 64**.

## SD card setup
* Make a folder named `/GEOS64` on the SD card.
* Copy your GEOS 64 boot disk image into it, under any file name ending in `.D81` (for example `GEOS64.D81`).
  * The TeensyROM finds the boot disk by what is on it, not by its name: the image must hold `DESK TOP` and the boot file `GEOBOOT`.
  * D81 images only: the image with the deskTop and the applications on one disk.
  * Don't end the name in `-TR` (`GEOS-TR.D81`): that name is kept for the copy described below, and is skipped.
  * Versions that name their deskTop differently (Gateway, Wheels) are not found.
* If the folder holds more than one boot disk, the first one found is used. Only the first 8 `.D81` files in the folder are checked.

## The -TR copy
* GEOS saves to the disk it starts from, so the TeensyROM never starts from your original image.
* The first time you select GEOS 64, it makes a copy next to the original with `-TR` added to the name (`GEOS64.D81` becomes `GEOS64-TR.D81`) and adds the TR USB mouse driver to the copy. Your original is not changed.
* Later launches reuse the `-TR` copy, so your saved work is still there.
  * Each launch brings the copy's TR USB mouse driver up to date with the firmware's.
* To start over from your original, delete the `-TR` copy. The next launch makes a new one.

## GeoRAM
* GEOS 64 runs with GeoRAM (128K to 512K, as much as fits) as its Special IO, selected automatically.
* At launch, the TeensyROM loads `SD:/georam.ram` into GeoRAM.
* **(TR+ only)** To save GeoRAM, press and release the Alt button (to the left of the menu button).
  * The contents are written to `georam.ram` on the SD card, and the LED flashes for about 1.5 seconds once the save is done.
  * The previous `georam.ram` is kept as `georam.bak`.
  * If the power goes off during a save, the next launch loads the newest complete save: the one just made if it was fully written, otherwise the previous one.
* GEOS's own `CONFIGURE` runs at start-up, set up in the `-TR` copy to find the TeensyROM as drive A. It does not find GeoRAM, so GEOS does not set GeoRAM up as a RAM disk.
  * Other configuration programs on the disk (such as `CONFIG 2.0r`) are kept from running at start-up, since they look for a disk drive.

## REU (TR+ only)
* To run GEOS with the TR+'s 512K REU instead of GeoRAM, set Special IO to `REU+USB Mouse/Disk` in the Settings menu (`F8`), then select GEOS 64 or GEOS 128.
  * GEOS's own `CONFIGURE` finds the REU at start-up and sets it up as GEOS does a 1764 or 1750.
  * At launch, the TeensyROM loads the REU pre-load/save image shown in the Settings menu into the REU. To save the REU to that file, press and release the Alt button.
* With any other Special IO setting, GEOS runs with GeoRAM.

## Date and time
* GEOS starts with the date and time of the TeensyROM's real-time clock, adjusted by the Time Zone setting.
  * Set the clock and time zone in the Settings menu (`F8`, page 6). See [General Usage](General_Usage.md#the-settings-menu).
  * Without Ethernet, the clock can also be set over USB with the `SetRTCToken` command. See [Remote Command Protocol](ControlComms.md).

## Messages
If GEOS 64 can't start, the TeensyROM stays at the menu and shows one of these:

| Message | Meaning |
|--|--|
| GEOS 64 starts from your boot disk image on the SD card. Insert the card. | No SD card was found. |
| Copy your GEOS 64 boot disk image (.D81) to /GEOS64 on the SD card, then select this again. | There is no `/GEOS64` folder, or no D81 in it holds `DESK TOP`. |
| `<name>`: not a D64, D71 or D81 without error bytes | The file's size is not that of a disk image (a D81 is 819,200 bytes). |
| `<name>`: (other reason) | The image could not be read, or its directory is damaged. |
| Could not add the mouse driver to a copy of `<name>`: the disk is full | No room for the driver. Free some space on your original image, or use another one. "the directory is full" is the same for directory entries. |
| Could not add the mouse driver to a copy of `<name>`: the SD card could not be written | The copy could not be written to the SD card. |
| `<name>-TR.D81` has no desktop; remove it to start over | The `-TR` copy has lost `DESK TOP`. Delete it. |
| `<name>-TR.D81` was set aside and could not be put back | Power was lost while the copy was being updated, and the TeensyROM could not restore it. Look for `<name>-TR.D81.BAK` in `/GEOS64`. |
| `<name>-TR.D81`: its TR USB MOUSE is an old one; delete this copy | The copy's mouse driver is from a TeensyROM firmware whose driver cannot be updated in place. Delete the `-TR` copy; the next launch makes a new one. |
| `<name>-TR.D81`: the boot file is not on the disk | The image has no `GEOBOOT`. |
| `<name>-TR.D81`: the boot file's disk driver is not one the TR can replace | This `GEOBOOT` is not a version the TeensyROM can start. |

 <br>

[Back to main ReadMe](/README.md)
