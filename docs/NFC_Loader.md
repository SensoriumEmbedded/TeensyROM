
# TeensyROM NFC Loading system
The NFC loading system makes your TeensyROM enabled C64/128 into an NFC card instant launch machine! Here's a [video](https://www.youtube.com/watch?v=iNfQx2gx0hA) of it in action. 
<BR>

## Table of contents
  * [NFC Loader Hardware](#nfc-loader-hardware)
    + [Electronics](#electronics)
    + [NFC Tag cards/media](#nfc-tag-cards-media)
    + [Card labels](#card-labels)
    + [Reader enclosure/case](#reader-enclosure-case)
  * [Tag programming](#tag-programming)
    + [Programming tag directly with the TeensyROM](#programming-tag-directly-with-the-teensyrom)
    + [Programming tag via cell phone (Alternate)](#programming-tag-via-cell-phone)
  * [TeensyROM Software setup](#teensyrom-software-setup)

## Thank you very much to:
* [**StatMat**](https://github.com/Stat-Mat) for sharing his vision and support for this project! 
  * TeensyROM and the [OneLoad64](https://www.youtube.com/watch?v=lz0CJbkplj0) project make a great pair!
* The [**Zaparoo Project**](https://github.com/ZaparooProject/zaparoo-core) for the genesis and inspiration for NFC launching capability.  
  * Happy to help make the C64 an officially supported platform! 

## NFC Loader Hardware

### Electronics
* The PN532 NFC Reader and the CH340 USB to serial UART interface are the two components needed to enable NFC loading.  They are available in discrete modules or in combination, here are some purchasing options:
  * Here's the [lowest priced combo module](https://www.aliexpress.us/item/3256806111642889.html) (PN532+CH340) we've found, highly recommended!
  * Other combo modules from [Aliexpress (Left)](https://www.aliexpress.us/item/3256806140123574.html) and [Elechouse (Right)](https://www.elechouse.com/product/pn532-nfc-usb-module/)
    |![Ali Combo](/media/NFC/Ali_Combo.jpg)|![Elec Combo](/media/NFC/Elec_Combo.jpg)| 
    |:--:|:--:|
  * Individual modules from Amazon (and elsewhere):
    * [NFC module (PN532)](https://www.amazon.com/gp/product/B01I1J17LC)
    * [USB/Serial adapter (CH340)](https://www.amazon.com/gp/product/B00LZV1G6K)
    * Connections to wire these two modules together: GND/GND, Vcc/5V, TxD/RxD, RxD/TxD
    * ![Discrete Modules](/media/NFC/Discrete_Top_Bot.webp)

### NFC Tag cards/media
* 3 types of NFC tags are currently supported:
  * NTAG215 (Recommended and [widely available](https://www.amazon.com/dp/B074M9J5L3))
  * NTAG213 (171 characters max)
  * NTAG216 (Not yet tested)
* Note: Tags sometimes bundled with the reader electronics are typically not one of the supported types and will not work with this system.

### Card labels
* Labels can be created using this [Zaparoo Label Generator](https://design.zaparoo.org/)
  * Choose the "HuCard (C64)" Card template
  * Recommend printing to glossy sticker sheets, then cut out individually for your tag cards.
    * ![TapTo_Label_Designer](/media/NFC/TapTo_Label_Designer.webp)

### Reader enclosure-case
* These 3D printable case designs by [Bedroom Ninja](https://www.printables.com/@bedroom_ninj_1665215) fit the theme perfectly!
  * [TapTo NFC-Engine](https://www.printables.com/model/737533-tapto-nfc-engine)
    * ![NFCEngine](/media/NFC/NFC_Engine.jpg)
  * [TapTo NFC-1541](https://www.printables.com/model/791580-tapto-nfc-1541)
    * ![NFC-1541](/media/NFC/NFC_1541.jpg)
* Since electronics modules vary in size, unique internal mounting methods may be needed. Tape or glue have been known to work. :)

## Tag programming
* Tags are programmed with a text field containing the path to a local file to be executed by the TeensyROM.
  * Text path format requirements:
    * The text can be one of these formats:
      * **path/filename** (defaults to SD)
      * **SD:path/filename** (SD card source)
      * **USB:path/filename** (USB drive source)
      * **TR:path/filename** (TeensyROM built-in source)
    * Example: *SD:OneLoad v5/Donkey Kong Junior.crt*
    * Character limit is 246 chars (171 for NTAG213)

### Programming tag directly with the TeensyROM
  * Using the TeensyROM menu, navigate to the file on SD or USB that you would like to create a tag for
  * Press the **Left Arrow** key to select it for tag writing.
  * Folow the on-screen instructions to load a tag and write to it.
  * ![Write Tag Screen](/media/NFC/Write_Tag.jpg)

### Programming tag via cell phone
  * (Alternate programming method)
  * Copy/type the paths you want to make tags from into a text file.
  * Send/e-mail the text file to your phone.
  * Use the (free) [NFC tools](https://www.wakdev.com/en/) application to write individual path/filename lines to tags.  It's available for [iPhone](https://itunes.apple.com/us/app/nfc-tools/id1252962749) or [Android](https://play.google.com/store/apps/details?id=com.wakdev.wdnfc)
    * Field requirements: One single NFC Record, Text Type, UTF-8, "Well Known" format  (default settings)
  * Here's a [demo video](https://youtu.be/YwQviLwWHYM?t=663) for tag writing via cell phone. It's made for a different system, but the tag writing process is the same.

## TeensyROM Software setup
* Be sure your TeensyROM is using Firmware version 0.5.12 or later for NFC reader support
  * See update instructions [here](General_Usage.md#firmware-updates) if update is needed.
* Connect your NFC reader to the USB Host port of the TeensyROM.
  * Alternately, a powered USB hub can by used.  This allows the NFC reader, USB Thumb drive, and MIDI device(s) to be connected simultaneously.
* Power up your C64/TeensyROM
* In the TeensyROM Main Menu, select **F8** to go to the Settings Menu
* Select the letter next to "NFC Enabled" to set to "On"
* A reboot is required for this to take effect.  Either select "Re-boot TeensyROM" from the menu, or power-cycle the C64.
* This setting stays persistent when the unit is powered down again.
  * Recommend disabling this option when the reader isn't connected to prevent slow start-up.
  * Not recommended for use while using Ethernet or MIDI applications due to increased latency.

<br>

[Back to main ReadMe](/README.md)