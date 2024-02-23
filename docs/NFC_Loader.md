
# TeensyROM NFC Loading system
The NFC loading system makes your TeensyROM enabled C64/128 into an NFC card fast launch machine! Here's a [video](https://www.youtube.com/watch?v=mDrT1I4R0ls) of it in action. Huge thanks to [**StatMat**](https://github.com/Stat-Mat) for sharing his vision and support for this project! TeensyROM and the [OneLoad64](https://www.youtube.com/watch?v=lz0CJbkplj0) project make a great pair! 

## NFC Loader Hardware
### Electronics
* The PN532 NFC Reader and the CH340 USB to serial UART interface are the two components needed to enable NFC loading.  They are available in discrete modules or in combination, here are some purchasing options:
  * Combo module from [Aliexpress (PN532+CH340)](https://www.aliexpress.us/item/3256806140123574.html)
  * Combo module from [Elechouse (PN532+CH340)](https://www.elechouse.com/product/pn532-nfc-usb-module/)
  * Individual modules from Amazon (and elsewhere):
    * [NFC module (PN532)](https://www.amazon.com/gp/product/B01I1J17LC)
    * [USB/Serial adapter (CH340)](https://www.amazon.com/gp/product/B00LZV1G6K)
    * Connections to wire these two modules together: GND/GND, Vcc/5V, TxD/RxD, RxD/TxD
### Reader enclosure/case
* There are a few 3D printable designs out there, [such as this](https://www.printables.com/model/737533-tapto-nfc-engine)
  * ![NFCEngine](/media/NFC/NFC_Engine.webp)
* Other designs are available and being designed, watch here for updates and let me know if you find other good ones.
* Note that module mounting options vary and may not match your specific reader module. Unique mounting options may be needed. 
### NFC Tag cards/media
* 3 types of NFC tags are currently supported:
  * NTAG215 (Recommended and [widely available](https://www.amazon.com/dp/B074M9J5L3))
  * NTAG213 (171 characters max)
  * NTAG216 (Not yet tested)
### Card labels
* Labels can be created using this [TapTo Label Generator](https://tapto-designer.netlify.app/)
  * Choose the "HuCard (C64)" Card template
  * Recommend printing to glossy sticker sheets, then cut out individually for your tag cards.
    * ![TapTo_Label_Designer](/media/NFC/TapTo_Label_Designer.webp)

## Tag programming
* Tags are programmed with a text field containing the path to a local file to be executed by the TeensyROM.
  * Text path format requirements:
    * The text can be one of these three formats:
      * **path/filename** (defaults to SD)
      * **SD:path/filename**
      * **USB:path/filename**
    * Example: *SD:OneLoad v5/Donkey Kong Junior.crt*
    * Character limit is 246 chars (171 for NTAG213)
* Programming directly with the TeensyROM:
  * Using the TeensyROM menu, navigate to the file on SD or USB that you would like to create a tag for
  * Press the **Left Arrow** key to select it for tag writing.
  * Folow the on-screen instructions to load a tag and write to it.
* Programming via cell phone: (Alternate programming method)
  * Copy/type the paths you want to make tags from into a text file.
  * Send/e-mail the text file to your phone.
  * Use the (free) [NFC tools](https://www.wakdev.com/en/) application to write individual path/filename lines to tags.  It's available for [iPhone](https://itunes.apple.com/us/app/nfc-tools/id1252962749) or [Android](https://play.google.com/store/apps/details?id=com.wakdev.wdnfc)
    * Field requirements: One single NFC Record, Text Type, UTF-8, "Well Known" format  (default settings)
  * Here's a [demo video](https://youtu.be/YwQviLwWHYM?t=663) for tag writing. It's made for a different system, but the tag writing process is the same.

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