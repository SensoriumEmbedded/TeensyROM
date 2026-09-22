// SPDX-License-Identifier: MIT
// Synthetic package fixtures for the loader tests. The module is a two-byte
// `bx lr` stub and the client banks are filler: these exercise the package
// FORMAT, and deliberately contain no real module or cartridge code.
//
// Everything here goes through tools/lib/extension.mjs, the same writer the
// packager uses, so the C++ tests that read these files are checking the real
// format rather than a test-only imitation of it.
import fs from 'node:fs';
import path from 'node:path';
import { buildImage, buildManifest, buildClientCrt, CODE_BASE } from './extension.mjs';

export function packageFixture(root, {
  id = 'HELLO', extensions = 'hi', bank0 = Buffer.alloc(8192, 0x11), bank1 = Buffer.alloc(8192, 0x22),
} = {}) {
  const directory = path.join(root, 'VMS', id);
  fs.mkdirSync(directory, { recursive: true });
  fs.writeFileSync(path.join(directory, 'engine.mvm'),
    buildImage({ code: Buffer.from([0x70, 0x47]), entry: CODE_BASE | 1 }));
  fs.writeFileSync(path.join(directory, 'manifest.vmi'), buildManifest({ id, extensions }));
  const client = buildClientCrt({ id, bank0, bank1 });
  fs.writeFileSync(path.join(directory, 'client.crt'), client);
  // A copy at the card root is how a user launches a package directly.
  fs.writeFileSync(path.join(root, id + '.crt'), client);
  return root;
}

// Second package, so the tests can cover ambiguous and non-matching registries.
export function registryFixture(root) {
  packageFixture(root);
  packageFixture(root, { id: 'OTHER', extensions: 'ot' });
  return root;
}
