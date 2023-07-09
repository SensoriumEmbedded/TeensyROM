### General AT commands
| Command | Description |
|--|--|
|AT | Ping  |
|ATC | Connect Ethernet using saved parameters and/or display connection info|
|ATDT\<HostName>:\<Port> | Connect to host and enter On-line mode|

### Commands that modify the saved parameters
**Use ATC or power cycle to apply these settings to Ethernet connection.**
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
|+++ | Disconnect from host and enter command mode

