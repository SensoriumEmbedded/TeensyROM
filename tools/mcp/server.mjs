#!/usr/bin/env node
// SPDX-License-Identifier: MIT
//
// The TeensyROM MCP server. Reaches the board over TCP when TR_HOST is set
// (host or host:port), else over USB serial at TR_PORT or the first TeensyROM
// port found.

import { serveMcp } from './lib/mcp.mjs';
import { createTools } from './lib/tools.mjs';
import { linkTarget, openLink } from './lib/trlink.mjs';

const SERVER_INFO = { name: 'teensyrom', version: '0.1.0' };
const INSTRUCTIONS = 'Tools for a TeensyROM cartridge in a Commodore 64 or 128. Start with status. '
  + 'Most tools need the main image with the C64 at the TeensyROM menu; reset gets it there.';

const connect = () => openLink(linkTarget(process.env));

await serveMcp({ info: SERVER_INFO, instructions: INSTRUCTIONS, tools: createTools({ connect }) });
