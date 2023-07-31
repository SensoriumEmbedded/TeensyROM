# Ethernet interface usage

## Hardware connection:
  * Use an Ethernet cable (RJ-45) to connect your TeensyROM to an Ethernet hub/router with internet access.

## Using a Terminal program to connect to the internet
  ### Yes, bulletin board and other telnet based services are still out there and popular!
  * Here are just a few:
    * [Oasis BBS:](https://theoasisbbs.com/) oasisbbs.hopto.org
    * 8-Bir Playground: 8bit.hoyvision.com    
    * 13th Floor BBS: 13th.hoyvision.com

  ### TeensyROM Setup
  * Select "Swiftlink/Modem" Special IO HW from the settings menu prior to running a terminal program.
  * The TeensyROM default Ethernet settings are as follows:
    * DHCP enabled
    * MAC Address: BE:0C:64:C0:FF:EE
  * If different settings such as static IP or a custom MAC are needed, use AT commands in any terminal program as described below.
    * Customized settings are stored in the TeensyROM for later use, even when power is removed or Firmware updated.

  ### Most C64/128 terminal programs can be used with the TeensyROM
  * Configure them to use a SwiftLink cartridge at address $DExx
    
  ### **CCGMS Terminal usage**
  * CCGMS is a popular C64/128 Terminal program that is bundled into your TeensyROM Firmware
  * Select "Swiftlink/Modem" Special IO HW from the settings menu 
  * Then select CCGMS from the main TeensyROM menu
  * Once CCGMS is launched, configure it for Swiftlink as follows:
    * F7: Dialer/Params
    * Press "m" two times to select "Swift/Turbo DE" as the Modem Type
    * Press "b" four times to select 38400 Baud Rate
    * Press Return to go back to main terminal
  * Note: The Setting and Phone book/autodialer features can be saved to a 1541/pi1541 drive, if connected
  ### Terminal mode Connect, Dial, Log in/out, and Disconnect
  * Enter "at" and hit return, you should see an "ok" response
    * This ensures the setup is correct and communicating with the TeensyROM
  * Enter "atc" 
    * This will make sure your TeensyROM is connected to the internet and show the settings/IP address currently in use
  * To connect to a BBS, use the "atdt" command, such as this:
    * atdtoasisbbs.hopto.org:6400   (note that no spaces are used)
  * After connected, follow the BBS prompts/menus to log in or browse
  * Log out to disconnect prescribed by the BBS
    * Disconnecting will return back to AT/command mode
    * Alternately,  connection can be aborted by rapidly typing "+++" while on-line  
  * See the complete list of available AT commands later in this document
    * Including setting Network configurations such as static IP address and custom MAC address.

## AT commands
  * For use in AT/Command mode of and C64/128 terminal program such as CCGMS
  * Settings are saved in TeensyROM and re-applied after power-down or FW update

### General Commands
| Command | Description |
|--|--|
|AT | Ping  |
|ATC | Connect Ethernet using saved parameters and/or display connection info|
|ATDT\<HostName>:\<Port> | Connect to host and enter On-line mode|

### Commands that modify the saved parameters
**Use ATC to apply these settings to current Ethernet connection.**
| Command | Description |
|--|--|
|AT+S | Display stored Ethernet settings|
|AT+DEFAULTS | Set defaults for all parameters
|AT+RNDMAC | MAC address to random value
|AT+MAC=\<XX:XX:XX:XX:XX:XX>  | MAC address to provided (hexidecimal) value
|AT+DHCP=\<0:1> | DHCP On(1)/Off(0)
|**For DHCP mode only:**|  |
|AT+DHCPTIME=\<D> |  DHCP Timeout in mS (0-65535)
|AT+DHCPRESP=\<D> |  DHCP Response Timeout in mS (0-65535)
|**For Static mode only:**|  |
|AT+MYIP=<D.D.D.D> | Local IP address
|AT+DNSIP=<D.D.D.D> | DNS IP address
|AT+GTWYIP=<D.D.D.D> | Gateway IP address
|AT+MASKIP=<D.D.D.D> | Subnet Mask IP address

### When in connected/on-line mode
| Command | Description |
|--|--|
|+++ | Disconnect from host and re-enter AT/command mode

## TeensyROM/C64/128 internet Time Synch
The TeensyROM main menu/application can synch the C64/128 system time with the internet for display in the TeensyROM menu and use in other applications.
* Go to the TeensyROM Settings Menu to control when the time is synchronized
  * Press F2 to synch the time in the current session
  * Press F1 to turn on/off automatic time-synch each time the TeensyROM is started
    * Assuming ethernet cable is connected
* The network settings (DHCP, MAC, etc) used by the time synchronization routine are the same as used by the Swiftlink modem interface.
  * They can be customized by AT commands as described elsewhere in this document.
* The NTP (Network Time Protocol) Server used in TeensyROM is
  * "us.pool.ntp.org"  via port 8888
  * May make this customizable in a future revision if there is interest

 <br>

[Back to main ReadMe](SensoriumEmbedded/TeensyROM/README.md)
 
