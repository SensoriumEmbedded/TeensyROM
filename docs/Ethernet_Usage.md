# Ethernet interface usage

## Hardware connection:
  * Use an Ethernet cable (RJ-45) to connect your TeensyROM to an Ethernet hub/router with internet access.

## Using a Terminal program to connect to the internet
  ### Yes, bulletin board and other telnet based services are still out there and popular!
  * Here are just a few:
    * [Oasis BBS:](https://theoasisbbs.com/) oasisbbs.hopto.org
    * 8-Bit Playground: 8bit.hoyvision.com    
    * 13th Floor BBS: 13th.hoyvision.com

  ### TeensyROM Setup
  * Select "Swiftlink/Modem" Special IO HW from the settings menu prior to running a terminal program.
    * Note: if you use the included CCGMS Terminal program, this association is automatic and this step can be skipped.
  * The default TeensyROM Ethernet settings are as follows:
    * DHCP enabled
    * MAC Address: BE:0C:64:C0:FF:EE
  * If different settings such as static IP or a custom MAC are needed, use AT commands in any terminal program as described below.
    * Customized settings are stored in the TeensyROM for later use, even when power is removed.

  ### Most C64/128 terminal programs can be used with the TeensyROM
  * Configure them to use a SwiftLink cartridge at address $DExx at 38400 baud
    
  ### **CCGMS Terminal usage**
  * CCGMS is a popular C64/128 Terminal program that is bundled into your TeensyROM Firmware
  * This version is pre-associated with the Swiftlink Special IO and defaults to the required modem type and baud rate
  * Just select CCGMS from the main TeensyROM Mem menu, no additional setup/config required!
  * Note: Phone book/autodialer entries can be saved to a 1541/pi1541 drive, if connected, and auto-loaded on start
  ### Terminal mode Connect, Dial, Log in/out, and Disconnect
  * Enter "at" and hit return, you should see an "ok" response
    * This ensures the setup is correct and communicating with the TeensyROM
  * Enter "atc" 
    * This will make sure your TeensyROM is connected to the internet and show the settings/IP address currently in use
  * To connect to a BBS, use the "atdt" command, such as this:
    * atdtoasisbbs.hopto.org:6400   (note that no spaces are used)
    * Note: Phone book/autodialers can be used instead, if set up
  * After connected, follow the BBS prompts/menus to log in or browse
  * Log out to disconnect prescribed by the BBS
    * Disconnecting will return back to AT/command mode
    * Alternately,  connection can be aborted by rapidly typing "+++" while on-line to kill the connection 
  * To see a complete list of available AT commands, enter "AT?" or see the next section below.
    * These commands include setting Network configurations such as static IP address and custom MAC address.

## AT commands
  * For use in AT/Command mode of and C64/128 terminal program such as CCGMS
  * Commands are not case sensitive
  * Settings are saved in TeensyROM and re-applied after power-down or FW update

### General Commands
| Command | Description |
|--|--|
|AT | Ping  |
|ATDT\<HostName>:\<Port> | Connect to host and enter On-line mode|
|ATI | TeensROM ID & Firmware Version |
|AT? | Quick help list of AT commands |
|ATC | Connect Ethernet using saved parameters and/or display connection info|
|ATBROWSE | Start the TeensyROM Web Browser ([Documented here](Browser_Usage.md))|
|ATE=\<0:1> | Echo On(1)/Off(0)
|ATV=\<0:1> | Verbose On(1)/Off(0)
|ATZ |Soft reset: restores echo and verbose mode, regardless of argument|
|ATH |Hook: dummy function to return OK, +++ command already disconnects|

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

### AT Response codes
* All AT commands use standard response codes when complete
  * Key word or Number depending on Verbose setting
    * Key words sent as upper case ASCII (aka lower case PETSCII)
  * Followed by a single carriage return
| Verbose Response | Non-Verbose | Indicates |
|--|--|--|
|OK          |0|Setting successful/complete                            |
|CONNECT     |1|Succesful server connection (ATDT)                     |
|RING        |2|unused                                                 |
|NO_CARRIER  |3|Connection Dropped (after CD de-assert)                |
|ERROR       |4|Syntax or formatting errors                            |
|CONNECT_1200|5|unused                                                 |
|NO_DIALTONE |6|No cable, DHCP or local host ethernet init fail (ATDT) |
|BUSY        |7|unused                                                 |
|NO_ANSWER   |8|No response from remote server (ATDT)                  |

## TeensyROM/C64/128 internet Time Synch
The TeensyROM main menu/application can synch the C64/128 system time with the internet for display in the TeensyROM menu and use in other applications.
* Go to the TeensyROM Settings Menu to control when the time is synchronized
  * Press 'f' to synch the time in the current session
  * Press 'd' to turn on/off automatic time-synch each time the TeensyROM is started
    * Assuming ethernet cable is connected
* The network settings (DHCP, MAC, etc) used by the time synchronization routine are the same as used by the Swiftlink modem interface.
  * They can be customized by AT commands as described above in this document.
* The NTP (Network Time Protocol) Server used in TeensyROM is
  * "us.pool.ntp.org"  via port 8888
  * May make this customizable in a future revision if there is interest

 <br>

[Back to main ReadMe](/README.md)
 
