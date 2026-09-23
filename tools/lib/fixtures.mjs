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
import { VM_BASE } from './hex.mjs';
import { ABI, HOSTID_MAGIC, HOST_ID_OFFSET, HOST_SERVICES, buildHostPackage,
         buildImage, buildManifest, buildClientCrt, BASE_SERVICES, CODE_BASE,
         SERVICE_EXAMPLE } from './extension.mjs';

export function packageFixture(root, {
  id = 'HELLO', extensions = 'hi', bank0 = Buffer.alloc(8192, 0x11), bank1 = Buffer.alloc(8192, 0x22),
  requiredServices = BASE_SERVICES,
} = {}) {
  const directory = path.join(root, 'VMS', id);
  fs.mkdirSync(directory, { recursive: true });
  fs.writeFileSync(path.join(directory, 'engine.mvm'),
    buildImage({ code: Buffer.from([0x70, 0x47]), entry: CODE_BASE | 1, requiredServices }));
  fs.writeFileSync(path.join(directory, 'manifest.vmi'), buildManifest({ id, extensions }));
  const client = buildClientCrt({ id, bank0, bank1 });
  fs.writeFileSync(path.join(directory, 'client.crt'), client);
  // A copy at the card root is how a user launches a package directly.
  fs.writeFileSync(path.join(root, id + '.crt'), client);
  return root;
}

// OTHER makes the registry ambiguous and non-matching; VENDOR requires
// registry bit 16, which no host in these tests provides.
export function registryFixture(root) {
  packageFixture(root);
  packageFixture(root, { id: 'OTHER', extensions: 'ot' });
  packageFixture(root, { id: 'VENDOR', extensions: 'vn',
                         requiredServices: BASE_SERVICES | SERVICE_EXAMPLE });
  return root;
}

// A minimal image that satisfies vm_host_slot_valid, packaged as a .TRH.
export function hostPackageFixture(root, { bytes = 0x8000, services = HOST_SERVICES,
                                           name = 'TestHost' } = {}) {
  const image = Buffer.alloc(bytes, 0xa5);
  const put = (offset, value) => image.writeUInt32LE(value >>> 0, offset);
  put(0x0, 0x42464346);
  put(0x1000, 0x432000d1);
  put(0x1004, VM_BASE + 0x2001);
  put(0x1020, VM_BASE);
  put(0x1024, bytes);
  put(HOST_ID_OFFSET, HOSTID_MAGIC);
  put(HOST_ID_OFFSET + 4, ABI);
  put(HOST_ID_OFFSET + 8, services);
  put(HOST_ID_OFFSET + 12, bytes);
  image.fill(0, HOST_ID_OFFSET + 16, HOST_ID_OFFSET + 32);
  image.write(name, HOST_ID_OFFSET + 16, 'latin1');

  const file = path.join(root, 'testhost.trh');
  fs.writeFileSync(file, buildHostPackage({ image }));
  return file;
}
