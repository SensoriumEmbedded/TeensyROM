// SPDX-License-Identifier: MIT
//
// A Model Context Protocol server over stdio: newline-delimited JSON-RPC 2.0
// carrying the tools capability only.

import readline from 'node:readline';

const SUPPORTED_PROTOCOL_VERSIONS = ['2025-11-25', '2025-06-18', '2025-03-26', '2024-11-05'];
const METHOD_NOT_FOUND = -32601;
const INVALID_PARAMS = -32602;
const PARSE_ERROR = -32700;

function protocolVersionFor(requested) {
  return SUPPORTED_PROTOCOL_VERSIONS.includes(requested) ? requested : SUPPORTED_PROTOCOL_VERSIONS[0];
}

// Versions before 2025-06-18 have no outputSchema or structuredContent; the
// JSON in the text block carries the same result to their clients.
const FIRST_STRUCTURED_VERSION = '2025-06-18';

function listing(tool, structured) {
  const { run, outputSchema, ...described } = tool;
  return structured ? { ...described, outputSchema } : described;
}

function errorResult(message) {
  return { content: [{ type: 'text', text: message }], isError: true };
}

function successResult(value, structured) {
  const result = { content: [{ type: 'text', text: JSON.stringify(value) }], isError: false };
  return structured ? { ...result, structuredContent: value } : result;
}

async function callTool(tools, params, structured) {
  const tool = tools.find((candidate) => candidate.name === params?.name);
  if (!tool) return errorResult(`no tool named '${params?.name}'`);
  try {
    return successResult(await tool.run(params.arguments ?? {}), structured);
  } catch (error) {
    return errorResult(error.message);
  }
}

function handlerFor(server) {
  let structured = true;
  return {
    initialize: (params) => {
      const protocolVersion = protocolVersionFor(params?.protocolVersion);
      structured = protocolVersion >= FIRST_STRUCTURED_VERSION;
      return { protocolVersion, capabilities: { tools: {} }, serverInfo: server.info, instructions: server.instructions };
    },
    ping: () => ({}),
    'tools/list': () => ({ tools: server.tools.map((tool) => listing(tool, structured)) }),
    'tools/call': (params) => callTool(server.tools, params, structured),
  };
}

export function serveMcp(server, { input = process.stdin, output = process.stdout } = {}) {
  const handlers = handlerFor(server);
  const send = (message) => output.write(`${JSON.stringify({ jsonrpc: '2.0', ...message })}\n`);
  let queue = Promise.resolve();

  async function answer(request) {
    const handler = handlers[request.method];
    if (!handler) {
      send({ id: request.id, error: { code: METHOD_NOT_FOUND, message: `unknown method ${request.method}` } });
      return;
    }
    try {
      send({ id: request.id, result: await handler(request.params) });
    } catch (error) {
      send({ id: request.id, error: { code: INVALID_PARAMS, message: error.message } });
    }
  }

  function receive(line) {
    if (!line.trim()) return;
    let request;
    try {
      request = JSON.parse(line);
    } catch {
      send({ id: null, error: { code: PARSE_ERROR, message: 'not JSON' } });
      return;
    }
    if (request.id === undefined) return;
    // The board answers one command at a time, so requests run in order.
    queue = queue.then(() => answer(request));
  }

  const lines = readline.createInterface({ input });
  lines.on('line', receive);
  return new Promise((resolve) => lines.on('close', () => queue.then(resolve)));
}
