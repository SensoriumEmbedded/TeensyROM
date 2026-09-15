// Resolves (downloading if needed) the pinned arduino-cli and prints its path, so CI can
// put it on PATH before running `arduino-cli` directly. Uses the same resolver and pin as
// the build itself (tools/lib/toolchain.mjs's PINS), so the CI install step and the build
// can never drift onto different arduino-cli versions from each other.
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { resolveArduinoCli } from './lib/toolchain.mjs';

const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
console.log(resolveArduinoCli(path.join(root, 'tools/.cache')));
