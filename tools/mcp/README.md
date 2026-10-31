# TeensyROM MCP server

An [MCP](https://modelcontextprotocol.io) server that lets an AI agent (Claude Code, Claude Desktop, or any MCP client) drive a TeensyROM cartridge in a Commodore 64 or 128. Needs Node 24 and nothing else: no `npm install`.

## Tools

| Tool | What it does | Needs |
|---|---|---|
| `status` | Which image is running (main, minimal, none), firmware version, board type, host machine, whether the menu has booted, whether DMA is available | any image but an extension |
| `list_dir` | Lists a directory on the SD card, USB drive or built-in menu, with paging | main image, C64 at the menu |
| `read_screen` | The 40-column text screen as 25 lines of text, read by DMA | TR+, main image |
| `push_file` | Copies a file from this computer to the SD card or USB drive, replacing one already there | main image, C64 at the menu |
| `launch` | Runs a file on the C64 (PRG, CRT, SID, D64, …) | main or minimal image |
| `reset` | Resets the C64 to the TeensyROM menu | main or minimal image |

Every tool declares an `outputSchema` and returns a JSON object matching it, as `structuredContent` and as the text of its one content block (for clients older than MCP 2025-06-18, the text alone). Field names and enum values are fixed, so an agent can rely on them; a `null` means the board didn't report that field.

A command the board can't take right now (for example `list_dir` while a game runs) comes back as a tool error carrying the board's own words, usually `Busy!`. `reset` gets the board back to the menu.

`read_screen` does not read the C128 80-column screen. Avoid it while a C128 runs at 2 MHz (GEOS 128 in 80 columns), because a DMA read can hang the machine.

## Connecting to the board

Each tool call opens its own connection and closes it, so other TeensyROM apps can use the board between calls.

- **USB serial** (the default): the first `/dev/cu.usbmodem*` or `/dev/ttyACM*` port, or the one in `TR_PORT`. macOS and Linux only for now.
- **TCP**: set `TR_HOST` to the board's address (`192.168.1.50`, or `192.168.1.50:2112`). Turn on "Enable TCP Listener" in the TeensyROM settings and power-cycle first. The board takes one TCP client at a time, and the ASID player turns the listener off.

## Install

### As a Claude Code plugin

This folder is a plugin. Until it is in a marketplace, load it from a checkout:

```sh
claude --plugin-dir tools/mcp
```

The plugin asks for `tr_host` and `tr_port`; leave both empty to use the first USB serial port.

### In any MCP client

Add a stdio server that runs `node tools/mcp/server.mjs`, with `TR_HOST` or `TR_PORT` in its environment as needed. For Claude Code without the plugin:

```sh
claude mcp add teensyrom -- node /path/to/TeensyROM/tools/mcp/server.mjs
```

## Development

```sh
node --test tools/mcp/test/*.test.mjs
```

The tests run against a fake board on a local TCP port (`test/fake-board.mjs`). `test/protocol.test.mjs` checks every token the server sends against the constants in `Source/Teensy`, so a firmware change to the protocol fails here. The wire protocol is in [docs/ControlComms.md](../../docs/ControlComms.md).
