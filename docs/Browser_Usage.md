# TeensyROM Web Browser usage


## Using a Terminal program to surf the web
  ### There are many text based web sites out there; use your TeensyROM to Search, Surf, and Download directly to SD or USB.

### TeensyROM Setup
* Connect to the internet using a terminal program (such as the built-in CCGMS) as described in the [Ethernet Usage Document](Ethernet_Usage.md)
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
| U[m] [URL]| Go to [URL] ("HTTP://" is assumed and added)<br>Common modifiers (see below). |
| #[m] | Follow link shown on current screen<br>Common modifiers (see below). |
| B[x] | Bookmark Read/Jump/Set<br>[x] modifiers:<br>&ensp;&ensp;(none) : List Bookmarks with links<br>&ensp;&ensp;#  :  Jump to bookmark #<br>&ensp;&ensp;s# : Set Current page as bookmark # |
| D [d]:[p] | Set download path, in this format:<br>[drive]:[path/directory] <br>Where drive is "usb" or "sd"  |
| X | Exit Browser mode |

Note: Download path and bookmarks are saved in the TeensyROM for future sessions. 

### Command modifiers (For R[m], U[m], & #[m] commands)
| Modifier | Description |
|--|--|
| (none) | Use previous/default |
| D | Download as file to the Download path |
| F | Filter the URL response through FrogFind |
| R | Read raw, directly from unfiltered URL |

 <br>

[Back to main ReadMe](/README.md)
 
