# TeensyROM Web Browser

## Features
* Bookmark your favorite sites for quick access later.
  * Saved in the TeensyROM for future sessions. 
  * Defaults loaded with popular text sites
* Previously visited sites saved in a browsing queue, use the 'p' command to go back.
* Ability to Read filtered (via [FrogFind](http://www.frogfind.com/)) or raw/unfiltered web pages
* Download files directly to specified path on USB or SD 
  * Path saved in TeensyROM
* View doawnloaded files and launch directly from browser
* Use search engine terms to find content quickly
* Displays PETSCII characters via HTML ([development tools here](http://sensoriumembedded.com/tinyweb64/petsciitag/))

### Using a C64/128 Terminal program to surf the web, limitations
* This is a text based interface only.  There is some formatting and colors based on HTML tags, and the ability to download, but no in-line graphics, etc. 
* HTTPS is not supported directly.  However, secure sites can be filtered/loaded via FrogFind.  If you see a message regarding this, try the reload/filtered command: 'rf'
* Redirects are not automatic. Either apply filter or follow link.

### TeensyROM Setup
* These commands are available in **FW version 0.5.8 and higher**.
  * See [these instructions](General_Usage.md#firmware-updates) if you need to update
* Connect to the internet using a terminal program (such as the built-in CCGMS) as described in the [Ethernet Usage Document](Ethernet_Usage.md)
* TeensyROM will attempt to initialize Ethernet when Swiftlink starts, but you can retry/verify using **"ATC"**
* Enter **"atbrowse"** and hit return to enter browser mode
  * You should see a short list of Browser mode commands.
    
### Browser mode commands
| Command | Description |
|--|--|
| ? | Show Browser Commands |
| Return | When page is paused, hitting Return alone continues to next screen |
| S [Term] | Search the internet for [Term] via the [FrogFind](http://frogfind.com/about.php) text search engine |
| P | Go to Previous Page |
| R[m] | Re-load current page<br>Common modifiers (see below). |
| U[m] [URL]| Go to [URL] (Use Server name or IP address)<br>URL Format: host[:port][/path]<br>"HTTP://" is assumed and added<br>Common modifiers (see below). |
| #[m] | Follow link shown on current screen<br>Common modifiers (see below). |
| B[x] | Bookmark Read/Jump/Set<br>[x] modifiers:<br>&ensp;&ensp;(none) : List Bookmark names with links<br>&ensp;&ensp;U : List Bookmark namess w/ links, also show URL<br>&ensp;&ensp;#  :  Jump to bookmark #<br>&ensp;&ensp;S# : Set Current page as bookmark #<br>&ensp;&ensp;R# [Name] : Rename bookmark # to [Name] |
| DS [d]:[p] | Set download path, in this format:<br>[drive]:[path/directory] <br>Where drive is "usb" or "sd"  |
| D  | Show Download Path Directory Contents<br>Select link # to launch directly |
| X | Exit Browser mode |

### Modifiers for R[m], U[m], #[m] Commands
| Modifier | Description |
|--|--|
| (none) | Use previous/default |
| D | Download as file to the Download path<br>Note that .prg, .crt, .sid, and .hex files are auomatically downloaded without modifier |
| F | Filter the URL response through FrogFind |
| R | Read raw, directly from unfiltered URL |

 <br>

[Back to main ReadMe](/README.md)
 
